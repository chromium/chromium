// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.Matchers.allOf;
import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.graphics.Canvas;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.view.View;
import android.widget.ImageButton;

import androidx.appcompat.content.res.AppCompatResources;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.espresso.matcher.ViewMatchers.Visibility;
import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.browser_controls.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.layouts.LayoutTestUtils;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.omnibox.LocationBarCoordinator;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_management.TabUiFeatureUtilities;
import org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.toolbar.ButtonDataImpl;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButton;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButtonCoordinator;
import org.chromium.chrome.browser.ui.appmenu.AppMenuCoordinator;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.OmniboxTestUtils;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.DisableAnimationsTestRule;
import org.chromium.ui.test.util.UiRestriction;

/**
 * Instrumentation tests for {@link ToolbarPhone}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
public class ToolbarPhoneTest {
    @ClassRule
    public static DisableAnimationsTestRule sEnableAnimationsRule =
            new DisableAnimationsTestRule(true);
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Mock
    private MenuButtonCoordinator mMenuButtonCoordinator;

    @Mock
    private MenuButtonCoordinator.SetFocusFunction mFocusFunction;
    @Mock
    private Runnable mRequestRenderRunnable;
    @Mock
    ThemeColorProvider mThemeColorProvider;
    @Mock
    GradientDrawable mLocationbarBackgroundDrawable;

    private Canvas mCanvas = new Canvas();
    private ToolbarPhone mToolbar;
    private View mToolbarButtonsContainer;
    private MenuButton mMenuButton;
    private OmniboxTestUtils mOmnibox;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mActivityTestRule.startMainActivityOnBlankPage();
        mToolbar = mActivityTestRule.getActivity().findViewById(R.id.toolbar);
        mToolbarButtonsContainer = mToolbar.findViewById(R.id.toolbar_buttons);
        mOmnibox = new OmniboxTestUtils(mActivityTestRule.getActivity());
    }

    @Test
    @MediumTest
    public void testDrawTabSwitcherAnimation_menuButtonDrawn() {
        mMenuButton = Mockito.spy(mToolbar.findViewById(R.id.menu_button_wrapper));
        mToolbar.setMenuButtonCoordinatorForTesting(mMenuButtonCoordinator);
        doReturn(mMenuButton).when(mMenuButtonCoordinator).getMenuButton();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mToolbar.draWithoutBackground(mCanvas);
            verify(mMenuButtonCoordinator)
                    .drawTabSwitcherAnimationOverlay(mToolbarButtonsContainer, mCanvas, 255);

            mToolbar.setTextureCaptureMode(true);
            mToolbar.draw(mCanvas);
            verify(mMenuButtonCoordinator, times(2))
                    .drawTabSwitcherAnimationOverlay(mToolbarButtonsContainer, mCanvas, 255);
            mToolbar.setTextureCaptureMode(false);
        });
    }

    @Test
    @MediumTest
    public void testFocusAnimation_menuButtonHidesAndShows() {
        TestThreadUtils.runOnUiThreadBlocking(() -> { mToolbar.onUrlFocusChange(true); });
        onView(allOf(withId(R.id.menu_button_wrapper), withEffectiveVisibility(Visibility.GONE)));

        TestThreadUtils.runOnUiThreadBlocking(() -> { mToolbar.onUrlFocusChange(false); });
        onView(allOf(
                withId(R.id.menu_button_wrapper), withEffectiveVisibility(Visibility.VISIBLE)));
    }

    @Test
    @MediumTest
    public void testOptionalButtonPadding_paddingUpdatesWithMenuVisibility() {
        mToolbar.setMenuButtonCoordinatorForTesting(mMenuButtonCoordinator);
        Drawable drawable = AppCompatResources.getDrawable(
                mActivityTestRule.getActivity(), R.drawable.ic_toolbar_share_offset_24dp);

        // When menu is hidden, optional button should have no padding.
        doReturn(false).when(mMenuButtonCoordinator).isVisible();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mToolbar.updateOptionalButton(new ButtonDataImpl(false, drawable, null,
                    mActivityTestRule.getActivity().getString(R.string.share), false, null, false,
                    AdaptiveToolbarButtonVariant.UNKNOWN));
            mToolbar.updateButtonVisibility();
        });

        int padding =
                mToolbar.findViewById(R.id.optional_toolbar_button_container).getPaddingStart();
        assertEquals("Optional button's padding should be 0 when menu button is not visible", 0,
                padding);

        // However when menu is visible, optional button should have
        // toolbar_phone_optional_button_padding padding.
        doReturn(true).when(mMenuButtonCoordinator).isVisible();
        TestThreadUtils.runOnUiThreadBlocking(() -> { mToolbar.updateButtonVisibility(); });
        padding = mToolbar.findViewById(R.id.optional_toolbar_button_container).getPaddingStart();
        int expectedPadding = mActivityTestRule.getActivity().getResources().getDimensionPixelSize(
                R.dimen.toolbar_phone_optional_button_padding);
        assertEquals(
                "Optional button should have a 12dp start padding set when menu button is visible",
                expectedPadding, padding);
    }

    @Test
    @MediumTest
    public void testLocationBarLengthWithOptionalButton() {
        // The purpose of this test is to document the expected behavior for setting
        // paddings and sizes of toolbar elements based on the visibility of the menu button.
        // This test fails if View#isShown() is used to determine visibility.
        // See https://crbug.com/1176992 for an example when it caused an issue.
        Drawable drawable = AppCompatResources.getDrawable(
                mActivityTestRule.getActivity(), R.drawable.ic_toolbar_share_offset_24dp);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Has to be created on the main thread.
            // clang-format off
            MenuButtonCoordinator realMenuButtonCoordinator = new MenuButtonCoordinator(
                    new OneshotSupplierImpl<AppMenuCoordinator>(),
                    new TestControlsVisibilityDelegate(),
                    mActivityTestRule.getActivity().getWindowAndroid(), mFocusFunction,
                    mRequestRenderRunnable, true, () -> false, mThemeColorProvider,
                    () -> null, () -> {}, org.chromium.chrome.R.id.menu_button_wrapper);
            // clang-format on
            mToolbar.setMenuButtonCoordinatorForTesting(realMenuButtonCoordinator);
            mToolbar.updateOptionalButton(new ButtonDataImpl(false, drawable, null,
                    mActivityTestRule.getActivity().getString(R.string.share), false, null, false,
                    AdaptiveToolbarButtonVariant.UNKNOWN));
            // Make sure the button is visible in the beginning of the test.
            assertEquals(realMenuButtonCoordinator.isVisible(), true);

            // Make the ancestors of the menu button invisible.
            mToolbarButtonsContainer.setVisibility(View.INVISIBLE);

            // Ancestor's invisibility doesn't affect menu button's visibility.
            assertEquals("Menu button should be visible even if its parents are not",
                    realMenuButtonCoordinator.isVisible(), true);
            float offsetWhenParentInvisible = mToolbar.getLocationBarWidthOffsetForOptionalButton();

            // Make menu's ancestors visible.
            mToolbarButtonsContainer.setVisibility(View.VISIBLE);
            assertEquals(realMenuButtonCoordinator.isVisible(), true);
            float offsetWhenParentVisible = mToolbar.getLocationBarWidthOffsetForOptionalButton();

            assertEquals("Offset should be the same even if menu button's parents are invisible "
                            + "if it is visible",
                    offsetWhenParentInvisible, offsetWhenParentVisible, 0);

            // Sanity check that the offset is different when menu button is invisible
            realMenuButtonCoordinator.getMenuButton().setVisibility(View.INVISIBLE);
            assertEquals(realMenuButtonCoordinator.isVisible(), false);
            float offsetWhenButtonInvisible = mToolbar.getLocationBarWidthOffsetForOptionalButton();
            Assert.assertNotEquals("Offset should be different when menu button is invisible",
                    offsetWhenButtonInvisible, offsetWhenParentVisible);
        });
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.OMNIBOX_MODERNIZE_VISUAL_UPDATE})
    @CommandLineFlags.
    Add({"enable-features=" + ChromeFeatureList.OMNIBOX_MODERNIZE_VISUAL_UPDATE + "<Study",
            "force-fieldtrials=Study/Group",
            "force-fieldtrial-params=Study.Group:modernize_visual_update_active_color_on_omnibox/true"})
    public void
    testToolbarColorSameAsSuggestionColorWhenFocus_activeColorOmnibox() {
        LocationBarCoordinator locationBarCoordinator =
                (LocationBarCoordinator) mToolbar.getLocationBar();
        ColorDrawable toolbarBackgroundDrawable = mToolbar.getBackgroundDrawable();
        mToolbar.setLocationBarBackgroundDrawableForTesting(mLocationbarBackgroundDrawable);
        int nonFocusedRadius = mActivityTestRule.getActivity().getResources().getDimensionPixelSize(
                R.dimen.modern_toolbar_background_corner_radius);
        int focusedRadius = mActivityTestRule.getActivity().getResources().getDimensionPixelSize(
                R.dimen.omnibox_suggestion_bg_round_corner_radius);

        // Focus on the Omnibox
        mOmnibox.requestFocus();
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(toolbarBackgroundDrawable.getColor(),
                    Matchers.is(locationBarCoordinator.getDropdownBackgroundColor(
                            false /*isIncognito*/)));
        });
        verify(mLocationbarBackgroundDrawable)
                .setTint(
                        locationBarCoordinator.getSuggestionBackgroundColor(false /*isIncognito*/));
        verify(mLocationbarBackgroundDrawable, atLeastOnce()).setCornerRadius(focusedRadius);

        // Clear focus on the Omnibox
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            locationBarCoordinator.getPhoneCoordinator().getViewForDrawing().clearFocus();
        });
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(toolbarBackgroundDrawable.getColor(),
                    Matchers.not(locationBarCoordinator.getDropdownBackgroundColor(
                            false /*isIncognito*/)));
        });
        verify(mLocationbarBackgroundDrawable, atLeastOnce()).setTint(anyInt());
        verify(mLocationbarBackgroundDrawable, atLeastOnce()).setCornerRadius(nonFocusedRadius);
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.OMNIBOX_MODERNIZE_VISUAL_UPDATE})
    @CommandLineFlags.
    Add({"enable-features=" + ChromeFeatureList.OMNIBOX_MODERNIZE_VISUAL_UPDATE + "<Study",
            "force-fieldtrials=Study/Group",
            "force-fieldtrial-params=Study.Group:modernize_visual_update_active_color_on_omnibox/false"})
    public void
    testToolbarColorSameAsSuggestionColorWhenFocus_noActiveColorOmnibox() {
        LocationBarCoordinator locationBarCoordinator =
                (LocationBarCoordinator) mToolbar.getLocationBar();
        ColorDrawable toolbarBackgroundDrawable = mToolbar.getBackgroundDrawable();
        mToolbar.setLocationBarBackgroundDrawableForTesting(mLocationbarBackgroundDrawable);

        // Focus on the Omnibox
        mOmnibox.requestFocus();
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(toolbarBackgroundDrawable.getColor(),
                    Matchers.is(locationBarCoordinator.getDropdownBackgroundColor(
                            false /*isIncognito*/)));
        });
        verify(mLocationbarBackgroundDrawable)
                .setTint(locationBarCoordinator.getDropdownBackgroundColor(false /*isIncognito*/));
        verify(mLocationbarBackgroundDrawable, never()).setCornerRadius(anyInt());

        // Clear focus on the Omnibox
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            locationBarCoordinator.getPhoneCoordinator().getViewForDrawing().clearFocus();
        });
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(toolbarBackgroundDrawable.getColor(),
                    Matchers.not(locationBarCoordinator.getDropdownBackgroundColor(
                            false /*isIncognito*/)));
        });
        verify(mLocationbarBackgroundDrawable, atLeastOnce()).setTint(anyInt());
        verify(mLocationbarBackgroundDrawable, never()).setCornerRadius(anyInt());
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.OMNIBOX_MODERNIZE_VISUAL_UPDATE})
    @CommandLineFlags.
    Add({"enable-features=" + ChromeFeatureList.OMNIBOX_MODERNIZE_VISUAL_UPDATE + "<Study",
            "force-fieldtrials=Study/Group",
            "force-fieldtrial-params=Study.Group:modernize_visual_update_active_color_on_omnibox/false"})
    public void
    testToolbarColorChangedAfterScroll_noActiveColorOmnibox() {
        LocationBarCoordinator locationBarCoordinator =
                (LocationBarCoordinator) mToolbar.getLocationBar();
        ColorDrawable toolbarBackgroundDrawable = mToolbar.getBackgroundDrawable();
        mToolbar.setLocationBarBackgroundDrawableForTesting(mLocationbarBackgroundDrawable);
        View statusViewBackground =
                mActivityTestRule.getActivity().findViewById(R.id.location_bar_status_icon_bg);

        // Focus on the Omnibox
        mOmnibox.requestFocus();
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(toolbarBackgroundDrawable.getColor(),
                    Matchers.is(locationBarCoordinator.getDropdownBackgroundColor(
                            false /*isIncognito*/)));
        });
        verify(mLocationbarBackgroundDrawable)
                .setTint(locationBarCoordinator.getDropdownBackgroundColor(false /*isIncognito*/));
        assertEquals(statusViewBackground.getVisibility(), View.INVISIBLE);

        // Scroll the dropdown
        TestThreadUtils.runOnUiThreadBlocking(() -> { mToolbar.onSuggestionDropdownScroll(); });
        verify(mLocationbarBackgroundDrawable)
                .setTint(ChromeColors.getSurfaceColor(
                        mActivityTestRule.getActivity(), R.dimen.toolbar_text_box_elevation));
        assertEquals(statusViewBackground.getVisibility(), View.VISIBLE);

        // Scroll the dropdown back to the top
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mToolbar.onSuggestionDropdownOverscrolledToTop(); });
        verify(mLocationbarBackgroundDrawable, atLeastOnce())
                .setTint(locationBarCoordinator.getDropdownBackgroundColor(false /*isIncognito*/));
        assertEquals(statusViewBackground.getVisibility(), View.INVISIBLE);

        // Clear focus on the Omnibox
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            locationBarCoordinator.getPhoneCoordinator().getViewForDrawing().clearFocus();
        });
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(toolbarBackgroundDrawable.getColor(),
                    Matchers.not(locationBarCoordinator.getDropdownBackgroundColor(
                            false /*isIncognito*/)));
        });
        verify(mLocationbarBackgroundDrawable, atLeastOnce()).setTint(anyInt());
        verify(mLocationbarBackgroundDrawable, never()).setCornerRadius(anyInt());
    }

    @Test
    @MediumTest
    @DisableFeatures({ChromeFeatureList.OMNIBOX_MODERNIZE_VISUAL_UPDATE})
    public void testLocationBarCornerShouldNeverUpdatedWithoutExperiment() {
        LocationBarCoordinator locationBarCoordinator =
                (LocationBarCoordinator) mToolbar.getLocationBar();
        mToolbar.setLocationBarBackgroundDrawableForTesting(mLocationbarBackgroundDrawable);

        // Focus on the Omnibox
        mOmnibox.requestFocus();
        verify(mLocationbarBackgroundDrawable, never()).setCornerRadius(anyInt());

        // Clear focus on the Omnibox
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            locationBarCoordinator.getPhoneCoordinator().getViewForDrawing().clearFocus();
        });
        verify(mLocationbarBackgroundDrawable, never()).setCornerRadius(anyInt());
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.START_SURFACE_ANDROID,
            ChromeFeatureList.TAB_TO_GTS_ANIMATION, ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID})
    public void
    testEnterTabSwitcher_toolbarVisibleUntilTransitionEnds_startSurfaceEnabled() {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        TabModelSelector tabModelSelector = cta.getTabModelSelectorSupplier().get();
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(tabModelSelector.isTabStateInitialized(), Matchers.is(true));
            Criteria.checkThat(tabModelSelector.getTotalTabCount(), Matchers.is(1));
        });

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            cta.findViewById(org.chromium.chrome.tab_ui.R.id.tab_switcher_button).performClick();
        });

        // When the Start surface refactoring is enabled, the ToolbarPhone is shown on the grid tab
        // switcher rather than the Start surface toolbar.
        if (!TabUiTestHelper.getIsStartSurfaceRefactorEnabledFromUIThread(cta)) {
            CriteriaHelper.pollUiThread(() -> mToolbar.getVisibility() != View.VISIBLE);
        }
        LayoutTestUtils.waitForLayout(cta.getLayoutManager(), LayoutType.TAB_SWITCHER);
        CriteriaHelper.pollUiThread(() -> {
            RecyclerView tabList = cta.findViewById(R.id.tab_list_view);
            RecyclerView.ViewHolder viewHolder =
                    tabList == null ? null : tabList.findViewHolderForAdapterPosition(0);
            if (viewHolder != null) {
                viewHolder.itemView.performClick();
                return true;
            }
            return false;
        });
        CriteriaHelper.pollUiThread(() -> mToolbar.getVisibility() == View.VISIBLE);
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.START_SURFACE_ANDROID,
            ChromeFeatureList.TAB_TO_GTS_ANIMATION, ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID})
    @DisableAnimationsTestRule.EnsureAnimationsOn
    public void
    testEnterTabSwitcher_toolbarVisibleUntilTransitionEnds_startSurfaceEnabled_animationsEnabled() {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        TabModelSelector tabModelSelector = cta.getTabModelSelectorSupplier().get();
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(tabModelSelector.isTabStateInitialized(), Matchers.is(true));
            Criteria.checkThat(tabModelSelector.getTotalTabCount(), Matchers.is(1));
        });

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            cta.findViewById(org.chromium.chrome.tab_ui.R.id.tab_switcher_button).performClick();
        });

        if (TabUiFeatureUtilities.isTabToGtsAnimationEnabled()) {
            Assert.assertTrue(mToolbar.getVisibility() == View.VISIBLE);
        }

        // When the Start surface refactoring is enabled, the ToolbarPhone is shown on the grid tab
        // switcher rather than the Start surface toolbar.
        if (!TabUiTestHelper.getIsStartSurfaceRefactorEnabledFromUIThread(cta)) {
            CriteriaHelper.pollUiThread(() -> mToolbar.getVisibility() != View.VISIBLE);
        }
        LayoutTestUtils.waitForLayout(cta.getLayoutManager(), LayoutType.TAB_SWITCHER);
        CriteriaHelper.pollUiThread(() -> {
            RecyclerView tabList = cta.findViewById(R.id.tab_list_view);
            RecyclerView.ViewHolder viewHolder =
                    tabList == null ? null : tabList.findViewHolderForAdapterPosition(0);
            if (viewHolder != null) {
                viewHolder.itemView.performClick();
                return true;
            }
            return false;
        });
        CriteriaHelper.pollUiThread(() -> mToolbar.getVisibility() == View.VISIBLE);
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID})
    @DisableFeatures({ChromeFeatureList.START_SURFACE_ANDROID})
    @DisableAnimationsTestRule.EnsureAnimationsOn
    public void testEnterTabSwitcher_toolbarVisibleUntilTransitionEnds_startSurfaceDisabled() {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        TabModelSelector tabModelSelector = cta.getTabModelSelectorSupplier().get();
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(tabModelSelector.isTabStateInitialized(), Matchers.is(true));
            Criteria.checkThat(tabModelSelector.getTotalTabCount(), Matchers.is(1));
        });

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            cta.findViewById(org.chromium.chrome.tab_ui.R.id.tab_switcher_button).performClick();
        });

        if (TabUiFeatureUtilities.isTabToGtsAnimationEnabled()) {
            Assert.assertTrue(mToolbar.getVisibility() == View.VISIBLE);
        }

        CriteriaHelper.pollUiThread(() -> mToolbar.getVisibility() != View.VISIBLE);
        LayoutTestUtils.waitForLayout(cta.getLayoutManager(), LayoutType.TAB_SWITCHER);
        CriteriaHelper.pollUiThread(() -> {
            RecyclerView tabList = cta.findViewById(R.id.tab_list_view);
            RecyclerView.ViewHolder viewHolder =
                    tabList == null ? null : tabList.findViewHolderForAdapterPosition(0);
            if (viewHolder != null) {
                viewHolder.itemView.performClick();
                return true;
            }
            return false;
        });
        CriteriaHelper.pollUiThread(() -> mToolbar.getVisibility() == View.VISIBLE);
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.START_SURFACE_ANDROID,
            ChromeFeatureList.TAB_TO_GTS_ANIMATION, ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID})
    @DisableAnimationsTestRule.EnsureAnimationsOn
    public void
    testToolbarTabSwitcherButtonNotClickableDuringTransition_startSurfaceEnabled() {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        TabModelSelector tabModelSelector = cta.getTabModelSelectorSupplier().get();
        ImageButton tabSwitcherButton = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            return cta.findViewById(org.chromium.chrome.tab_ui.R.id.tab_switcher_button);
        });
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(tabModelSelector.isTabStateInitialized(), Matchers.is(true));
            Criteria.checkThat(tabModelSelector.getTotalTabCount(), Matchers.is(1));
            Criteria.checkThat(tabSwitcherButton.isClickable(), Matchers.is(true));
        });

        TestThreadUtils.runOnUiThreadBlocking(() -> { tabSwitcherButton.performClick(); });

        if (TabUiFeatureUtilities.isTabToGtsAnimationEnabled()) {
            Assert.assertTrue(mToolbar.getVisibility() == View.VISIBLE);
        }

        CriteriaHelper.pollUiThread(() -> mToolbar.getVisibility() != View.VISIBLE);
        LayoutTestUtils.waitForLayout(cta.getLayoutManager(), LayoutType.TAB_SWITCHER);
        CriteriaHelper.pollUiThread(() -> {
            RecyclerView tabList = cta.findViewById(R.id.tab_list_view);
            RecyclerView.ViewHolder viewHolder =
                    tabList == null ? null : tabList.findViewHolderForAdapterPosition(0);
            if (viewHolder != null) {
                viewHolder.itemView.performClick();
                return true;
            }
            return false;
        });
        if (TabUiFeatureUtilities.isTabToGtsAnimationEnabled()) {
            Assert.assertFalse(
                    "Tab switcher button should not be clickable", tabSwitcherButton.isClickable());
        }

        CriteriaHelper.pollUiThread(() -> mToolbar.getVisibility() == View.VISIBLE);
        LayoutTestUtils.waitForLayout(cta.getLayoutManager(), LayoutType.BROWSING);
        CriteriaHelper.pollUiThread(() -> {
            // Check that clicks become enabled after the transition.
            return tabSwitcherButton.isClickable();
        }, "Tab switcher button did not become clickable.");
        // Ensure it is possible to return to tab switcher.
        TestThreadUtils.runOnUiThreadBlocking(() -> { tabSwitcherButton.performClick(); });
        LayoutTestUtils.waitForLayout(cta.getLayoutManager(), LayoutType.TAB_SWITCHER);
    }

    @Test
    @MediumTest
    @DisableFeatures({ChromeFeatureList.TAB_TO_GTS_ANIMATION})
    @EnableFeatures(
            {ChromeFeatureList.START_SURFACE_ANDROID, ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID})
    @DisableAnimationsTestRule.EnsureAnimationsOn
    public void
    testToolbarTabSwitcherButtonNotClickableDuringTransition_startSurfaceEnabled_noAnimation() {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        TabModelSelector tabModelSelector = cta.getTabModelSelectorSupplier().get();
        ImageButton tabSwitcherButton = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            return cta.findViewById(org.chromium.chrome.tab_ui.R.id.tab_switcher_button);
        });
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(tabModelSelector.isTabStateInitialized(), Matchers.is(true));
            Criteria.checkThat(tabModelSelector.getTotalTabCount(), Matchers.is(1));
            Criteria.checkThat(tabSwitcherButton.isClickable(), Matchers.is(true));
        });

        TestThreadUtils.runOnUiThreadBlocking(() -> { tabSwitcherButton.performClick(); });

        if (TabUiFeatureUtilities.isTabToGtsAnimationEnabled()) {
            Assert.assertTrue(mToolbar.getVisibility() == View.VISIBLE);
        }

        CriteriaHelper.pollUiThread(() -> mToolbar.getVisibility() != View.VISIBLE);
        LayoutTestUtils.waitForLayout(cta.getLayoutManager(), LayoutType.TAB_SWITCHER);
        CriteriaHelper.pollUiThread(() -> {
            RecyclerView tabList = cta.findViewById(R.id.tab_list_view);
            RecyclerView.ViewHolder viewHolder =
                    tabList == null ? null : tabList.findViewHolderForAdapterPosition(0);
            if (viewHolder != null) {
                viewHolder.itemView.performClick();
                return true;
            }
            return false;
        });
        if (TabUiFeatureUtilities.isTabToGtsAnimationEnabled()) {
            Assert.assertFalse(
                    "Tab switcher button should not be clickable", tabSwitcherButton.isClickable());
        }

        CriteriaHelper.pollUiThread(() -> mToolbar.getVisibility() == View.VISIBLE);
        LayoutTestUtils.waitForLayout(cta.getLayoutManager(), LayoutType.BROWSING);
        CriteriaHelper.pollUiThread(() -> {
            // Check that clicks become enabled after the transition.
            return tabSwitcherButton.isClickable();
        }, "Tab switcher button did not become clickable.");
        // Ensure it is possible to return to tab switcher.
        TestThreadUtils.runOnUiThreadBlocking(() -> { tabSwitcherButton.performClick(); });
        LayoutTestUtils.waitForLayout(cta.getLayoutManager(), LayoutType.TAB_SWITCHER);
    }

    @Test
    @MediumTest
    @EnableFeatures(
            {ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID, ChromeFeatureList.TAB_TO_GTS_ANIMATION})
    @DisableFeatures({ChromeFeatureList.START_SURFACE_ANDROID})
    @DisableAnimationsTestRule.EnsureAnimationsOn
    public void
    testToolbarTabSwitcherButtonNotClickableDuringTransition_startSurfaceDisabled() {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        TabModelSelector tabModelSelector = cta.getTabModelSelectorSupplier().get();
        ImageButton tabSwitcherButton = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            return cta.findViewById(org.chromium.chrome.tab_ui.R.id.tab_switcher_button);
        });
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(tabModelSelector.isTabStateInitialized(), Matchers.is(true));
            Criteria.checkThat(tabModelSelector.getTotalTabCount(), Matchers.is(1));
            Criteria.checkThat(tabSwitcherButton.isClickable(), Matchers.is(true));
        });

        TestThreadUtils.runOnUiThreadBlocking(() -> { tabSwitcherButton.performClick(); });

        if (TabUiFeatureUtilities.isTabToGtsAnimationEnabled()) {
            Assert.assertTrue(mToolbar.getVisibility() == View.VISIBLE);
        }

        CriteriaHelper.pollUiThread(() -> mToolbar.getVisibility() != View.VISIBLE);
        LayoutTestUtils.waitForLayout(cta.getLayoutManager(), LayoutType.TAB_SWITCHER);
        CriteriaHelper.pollUiThread(() -> {
            RecyclerView tabList = cta.findViewById(R.id.tab_list_view);
            RecyclerView.ViewHolder viewHolder =
                    tabList == null ? null : tabList.findViewHolderForAdapterPosition(0);
            if (viewHolder != null) {
                viewHolder.itemView.performClick();
                Assert.assertFalse("Clickable should be false during transition.",
                        tabSwitcherButton.isClickable());
                return true;
            }
            return false;
        });
        if (TabUiFeatureUtilities.isTabToGtsAnimationEnabled()) {
            Assert.assertFalse(
                    "Tab switcher button should not be clickable", tabSwitcherButton.isClickable());
        }
        CriteriaHelper.pollUiThread(() -> mToolbar.getVisibility() == View.VISIBLE);
        LayoutTestUtils.waitForLayout(cta.getLayoutManager(), LayoutType.BROWSING);
        CriteriaHelper.pollUiThread(() -> {
            // Check that clicks become enabled after the transition.
            return tabSwitcherButton.isClickable();
        }, "Tab switcher button did not become clickable.");
        // Ensure it is possible to return to tab switcher.
        TestThreadUtils.runOnUiThreadBlocking(() -> { tabSwitcherButton.performClick(); });
        LayoutTestUtils.waitForLayout(cta.getLayoutManager(), LayoutType.TAB_SWITCHER);
    }

    private static class TestControlsVisibilityDelegate
            extends BrowserStateBrowserControlsVisibilityDelegate {
        public TestControlsVisibilityDelegate() {
            super(new ObservableSupplier<Boolean>() {
                @Override
                public Boolean addObserver(Callback<Boolean> obs) {
                    return false;
                }

                @Override
                public void removeObserver(Callback<Boolean> obs) {}

                @Override
                public Boolean get() {
                    return false;
                }
            });
        }
    }
}
