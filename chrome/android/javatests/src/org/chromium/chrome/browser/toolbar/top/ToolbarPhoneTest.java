// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.equalTo;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.graphics.Canvas;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.os.Build.VERSION_CODES;
import android.view.View;

import androidx.annotation.ColorInt;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.graphics.ColorUtils;
import androidx.test.espresso.matcher.ViewMatchers.Visibility;
import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.AfterClass;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import org.chromium.base.CallbackUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.browser_controls.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.night_mode.ChromeNightModeTestUtils;
import org.chromium.chrome.browser.omnibox.LocationBarCoordinator;
import org.chromium.chrome.browser.omnibox.SearchEngineUtils;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.toolbar.ButtonData;
import org.chromium.chrome.browser.toolbar.ButtonDataImpl;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButton;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButtonCoordinator;
import org.chromium.chrome.browser.toolbar.optional_button.OptionalButtonCoordinator;
import org.chromium.chrome.browser.toolbar.top.ToolbarPhone.VisualState;
import org.chromium.chrome.browser.ui.appmenu.AppMenuCoordinator;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.NewTabPageTestUtils;
import org.chromium.chrome.test.util.OmniboxTestUtils;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.omnibox.OmniboxFeatureList;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.ViewUtils;

/** Instrumentation tests for {@link ToolbarPhone}. */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Restriction(DeviceFormFactor.PHONE)
public class ToolbarPhoneTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Mock private MenuButtonCoordinator mMenuButtonCoordinator;

    @Mock private MenuButtonCoordinator.SetFocusFunction mFocusFunction;
    @Mock private Runnable mRequestRenderRunnable;
    @Mock ThemeColorProvider mThemeColorProvider;
    @Mock GradientDrawable mLocationbarBackgroundDrawable;
    @Mock OptionalButtonCoordinator mOptionalButtonCoordinator;
    @Mock private SearchEngineUtils mSearchEngineUtils;

    private Canvas mCanvas = new Canvas();
    private ToolbarPhone mToolbar;
    private View mToolbarButtonsContainer;
    private MenuButton mMenuButton;
    private OmniboxTestUtils mOmnibox;
    private TemplateUrlService mTemplateUrlService;

    @ParameterAnnotations.UseMethodParameterBefore(NightModeTestUtils.NightModeParams.class)
    public void setupNightMode(boolean nightModeEnabled) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ChromeNightModeTestUtils.setUpNightModeForChromeActivity(nightModeEnabled);
                });
    }

    @BeforeClass
    public static void setUpBeforeActivityLaunched() {
        ChromeNightModeTestUtils.setUpNightModeBeforeChromeActivityLaunched();
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mActivityTestRule.startMainActivityOnBlankPage();
        TemplateUrlService originalService =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                TemplateUrlServiceFactory.getForProfile(
                                        ProfileManager.getLastUsedRegularProfile()));
        mTemplateUrlService = Mockito.spy(originalService);
        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);
        mToolbar = mActivityTestRule.getActivity().findViewById(R.id.toolbar);
        mToolbarButtonsContainer = mToolbar.findViewById(R.id.toolbar_buttons);
        mOmnibox = new OmniboxTestUtils(mActivityTestRule.getActivity());
    }

    @AfterClass
    public static void tearDownAfterActivityDestroyed() {
        ChromeNightModeTestUtils.tearDownNightModeAfterChromeActivityDestroyed();
    }

    @Test
    @MediumTest
    public void testDrawTabSwitcherAnimation_menuButtonDrawn() {
        mMenuButton = Mockito.spy(mToolbar.findViewById(R.id.menu_button_wrapper));
        mToolbar.setMenuButtonCoordinatorForTesting(mMenuButtonCoordinator);
        doReturn(mMenuButton).when(mMenuButtonCoordinator).getMenuButton();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mToolbar.drawWithoutBackground(mCanvas);
                    verify(mMenuButtonCoordinator)
                            .drawTabSwitcherAnimationOverlay(
                                    mToolbarButtonsContainer, mCanvas, 255);

                    mToolbar.setTextureCaptureMode(true);
                    mToolbar.draw(mCanvas);
                    verify(mMenuButtonCoordinator, times(2))
                            .drawTabSwitcherAnimationOverlay(
                                    mToolbarButtonsContainer, mCanvas, 255);
                    mToolbar.setTextureCaptureMode(false);
                });
    }

    @Test
    @MediumTest
    public void testFocusAnimation_menuButtonHidesAndShows() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mToolbar.onUrlFocusChange(true);
                });
        onView(allOf(withId(R.id.menu_button_wrapper), withEffectiveVisibility(Visibility.GONE)));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mToolbar.onUrlFocusChange(false);
                });
        onView(
                allOf(
                        withId(R.id.menu_button_wrapper),
                        withEffectiveVisibility(Visibility.VISIBLE)));
    }

    @Test
    @MediumTest
    public void testLocationBarLengthWithOptionalButton() {
        // The purpose of this test is to document the expected behavior for setting
        // paddings and sizes of toolbar elements based on the visibility of the menu button.
        // This test fails if View#isShown() is used to determine visibility.
        // See https://crbug.com/1176992 for an example when it caused an issue.
        Drawable drawable =
                AppCompatResources.getDrawable(
                        mActivityTestRule.getActivity(), R.drawable.ic_toolbar_share_offset_24dp);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Has to be created on the main thread.
                    MenuButtonCoordinator realMenuButtonCoordinator =
                            new MenuButtonCoordinator(
                                    new OneshotSupplierImpl<AppMenuCoordinator>(),
                                    new TestControlsVisibilityDelegate(),
                                    mActivityTestRule.getActivity().getWindowAndroid(),
                                    mFocusFunction,
                                    mRequestRenderRunnable,
                                    true,
                                    () -> false,
                                    mThemeColorProvider,
                                    () -> null,
                                    CallbackUtils.emptyRunnable(),
                                    R.id.menu_button_wrapper);
                    mToolbar.setMenuButtonCoordinatorForTesting(realMenuButtonCoordinator);
                    mToolbar.updateOptionalButton(
                            new ButtonDataImpl(
                                    false,
                                    drawable,
                                    null,
                                    mActivityTestRule.getActivity().getString(R.string.share),
                                    false,
                                    null,
                                    false,
                                    AdaptiveToolbarButtonVariant.UNKNOWN,
                                    0,
                                    false));
                    // Make sure the button is visible in the beginning of the test.
                    assertEquals(realMenuButtonCoordinator.isVisible(), true);

                    // Make the ancestors of the menu button invisible.
                    mToolbarButtonsContainer.setVisibility(View.INVISIBLE);

                    // Ancestor's invisibility doesn't affect menu button's visibility.
                    assertEquals(
                            "Menu button should be visible even if its parents are not",
                            realMenuButtonCoordinator.isVisible(),
                            true);
                    float offsetWhenParentInvisible =
                            mToolbar.getLocationBarWidthOffsetForOptionalButton();

                    // Make menu's ancestors visible.
                    mToolbarButtonsContainer.setVisibility(View.VISIBLE);
                    assertEquals(realMenuButtonCoordinator.isVisible(), true);
                    float offsetWhenParentVisible =
                            mToolbar.getLocationBarWidthOffsetForOptionalButton();

                    assertEquals(
                            "Offset should be the same even if menu button's parents are invisible "
                                    + "if it is visible",
                            offsetWhenParentInvisible,
                            offsetWhenParentVisible,
                            0);

                    // Confidence check that the offset is different when menu button is invisible.
                    realMenuButtonCoordinator.getMenuButton().setVisibility(View.INVISIBLE);
                    assertEquals(realMenuButtonCoordinator.isVisible(), false);
                    float offsetWhenButtonInvisible =
                            mToolbar.getLocationBarWidthOffsetForOptionalButton();
                    assertNotEquals(
                            "Offset should be different when menu button is invisible",
                            offsetWhenButtonInvisible,
                            offsetWhenParentVisible);
                });
    }

    @Test
    @MediumTest
    public void testToolbarColorSameAsSuggestionColorWhenFocus_activeColorOmnibox() {
        LocationBarCoordinator locationBarCoordinator =
                (LocationBarCoordinator) mToolbar.getLocationBar();
        ColorDrawable toolbarBackgroundDrawable = mToolbar.getBackgroundDrawable();
        mToolbar.setLocationBarBackgroundDrawableForTesting(mLocationbarBackgroundDrawable);
        int nonFocusedRadius =
                mActivityTestRule
                        .getActivity()
                        .getResources()
                        .getDimensionPixelSize(R.dimen.modern_toolbar_background_corner_radius);
        int focusedRadius =
                mActivityTestRule
                        .getActivity()
                        .getResources()
                        .getDimensionPixelSize(R.dimen.omnibox_suggestion_bg_round_corner_radius);

        // Focus on the Omnibox
        mOmnibox.requestFocus();
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            toolbarBackgroundDrawable.getColor(),
                            Matchers.is(
                                    locationBarCoordinator.getDropdownBackgroundColor(
                                            /* isIncognito= */ false)));
                });
        verify(mLocationbarBackgroundDrawable)
                .setTint(
                        locationBarCoordinator.getSuggestionBackgroundColor(
                                /* isIncognito= */ false));
        verify(mLocationbarBackgroundDrawable, atLeastOnce()).setCornerRadius(focusedRadius);

        // Clear focus on the Omnibox
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    locationBarCoordinator.getPhoneCoordinator().getViewForDrawing().clearFocus();
                });
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            toolbarBackgroundDrawable.getColor(),
                            Matchers.not(
                                    locationBarCoordinator.getDropdownBackgroundColor(
                                            /* isIncognito= */ false)));
                });
        verify(mLocationbarBackgroundDrawable, atLeastOnce()).setTint(anyInt());
        verify(mLocationbarBackgroundDrawable, atLeastOnce()).setCornerRadius(nonFocusedRadius);
    }

    @Test
    @MediumTest
    public void testOptionalButton_NotDrawnWhenZeroWidth() {
        Drawable drawable =
                AppCompatResources.getDrawable(
                        mActivityTestRule.getActivity(), R.drawable.ic_toolbar_share_offset_24dp);
        ButtonData buttonData =
                new ButtonDataImpl(
                        true,
                        drawable,
                        null,
                        mActivityTestRule.getActivity().getString(R.string.share),
                        false,
                        null,
                        true,
                        AdaptiveToolbarButtonVariant.UNKNOWN,
                        0,
                        false);

        // Show a button, this will inflate the optional button view and create its coordinator.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mToolbar.updateOptionalButton(buttonData);
                });

        CriteriaHelper.pollUiThread(
                () ->
                        mToolbar.getOptionalButtonViewForTesting() != null
                                && mToolbar.getOptionalButtonViewForTesting().getVisibility()
                                        == View.VISIBLE);

        // Replace the coordinator with a mock, and set the button to visible with 0 width.
        View optionalButtonView = mToolbar.findViewById(R.id.optional_toolbar_button_container);
        when(mOptionalButtonCoordinator.getViewForDrawing()).thenReturn(optionalButtonView);
        when(mOptionalButtonCoordinator.getViewWidth()).thenReturn(0);
        when(mOptionalButtonCoordinator.getViewVisibility()).thenReturn(View.VISIBLE);

        mToolbar.setOptionalButtonCoordinatorForTesting(mOptionalButtonCoordinator);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Draw the toolbar.
                    mToolbar.drawWithoutBackground(mCanvas);
                    // Optional button shouldn't be drawn because its width is zero.
                    verify(mOptionalButtonCoordinator, never()).getViewForDrawing();
                });
    }

    @Test
    @MediumTest
    public void testOptionalButton_NotDrawnWhenNotVisible() {
        Drawable drawable =
                AppCompatResources.getDrawable(
                        mActivityTestRule.getActivity(), R.drawable.ic_toolbar_share_offset_24dp);
        ButtonData buttonData =
                new ButtonDataImpl(
                        true,
                        drawable,
                        null,
                        mActivityTestRule.getActivity().getString(R.string.share),
                        false,
                        null,
                        true,
                        AdaptiveToolbarButtonVariant.UNKNOWN,
                        0,
                        false);

        // Show a button, this will inflate the optional button view and create its coordinator.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mToolbar.updateOptionalButton(buttonData);
                });

        CriteriaHelper.pollUiThread(
                () ->
                        mToolbar.getOptionalButtonViewForTesting() != null
                                && mToolbar.getOptionalButtonViewForTesting().getVisibility()
                                        == View.VISIBLE);

        // Replace the coordinator with a mock, and set the button to gone with regular width.
        View optionalButtonView = mToolbar.findViewById(R.id.optional_toolbar_button_container);
        when(mOptionalButtonCoordinator.getViewForDrawing()).thenReturn(optionalButtonView);
        when(mOptionalButtonCoordinator.getViewWidth()).thenReturn(optionalButtonView.getWidth());
        when(mOptionalButtonCoordinator.getViewVisibility()).thenReturn(View.GONE);

        mToolbar.setOptionalButtonCoordinatorForTesting(mOptionalButtonCoordinator);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Draw the toolbar.
                    mToolbar.drawWithoutBackground(mCanvas);
                    // Optional button shouldn't be drawn because its visibility is gone.
                    verify(mOptionalButtonCoordinator, never()).getViewForDrawing();
                });
    }

    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1473282")
    public void testOptionalButton_DrawnWhenVisible() {
        Drawable drawable =
                AppCompatResources.getDrawable(
                        mActivityTestRule.getActivity(), R.drawable.ic_toolbar_share_offset_24dp);
        ButtonData buttonData =
                new ButtonDataImpl(
                        true,
                        drawable,
                        null,
                        mActivityTestRule.getActivity().getString(R.string.share),
                        false,
                        null,
                        true,
                        AdaptiveToolbarButtonVariant.UNKNOWN,
                        0,
                        false);

        // Show a button, this will inflate the optional button view and create its coordinator.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mToolbar.updateOptionalButton(buttonData);
                });

        CriteriaHelper.pollUiThread(() -> mToolbar.getOptionalButtonViewForTesting() != null);
        ViewUtils.onViewWaiting(
                allOf(equalTo(mToolbar.getOptionalButtonViewForTesting()), isDisplayed()));

        // Replace the coordinator with a mock, and set the button to visible with regular width.
        View optionalButtonView = mToolbar.findViewById(R.id.optional_toolbar_button_container);
        when(mOptionalButtonCoordinator.getViewForDrawing()).thenReturn(optionalButtonView);
        when(mOptionalButtonCoordinator.getViewWidth()).thenReturn(optionalButtonView.getWidth());
        when(mOptionalButtonCoordinator.getViewVisibility()).thenReturn(View.VISIBLE);

        mToolbar.setOptionalButtonCoordinatorForTesting(mOptionalButtonCoordinator);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Draw the toolbar.
                    mToolbar.drawWithoutBackground(mCanvas);
                    // Optional button should be drawn.
                    verify(mOptionalButtonCoordinator, atLeastOnce()).getViewForDrawing();
                });
    }

    @Test
    @MediumTest
    public void testToolbarBackgroundChange() {
        ColorDrawable toolbarBackgroundDrawable = mToolbar.getBackgroundDrawable();
        @ColorInt
        int homeSurfaceToolbarBackgroundColor =
                ColorUtils.setAlphaComponent(
                        ChromeColors.getSurfaceColor(
                                mToolbar.getContext(),
                                R.dimen.home_surface_background_color_elevation),
                        0);

        assertEquals(false, mToolbar.isLocationBarShownInNtp());
        assertNotEquals(homeSurfaceToolbarBackgroundColor, toolbarBackgroundDrawable.getColor());

        // Load the new tab page.
        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);
        Tab tab = mActivityTestRule.getActivity().getActivityTab();
        NewTabPageTestUtils.waitForNtpLoaded(tab);
        assertEquals(true, mToolbar.isLocationBarShownInNtp());
        assertEquals(homeSurfaceToolbarBackgroundColor, toolbarBackgroundDrawable.getColor());

        // Focus on the Omnibox.
        mOmnibox.requestFocus();
        assertNotEquals(homeSurfaceToolbarBackgroundColor, toolbarBackgroundDrawable.getColor());
    }

    @Test
    @MediumTest
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testRealSearchBoxAppearanceChange(boolean nightModeEnabled) {
        LocationBarCoordinator locationBarCoordinator =
                (LocationBarCoordinator) mToolbar.getLocationBar();
        View iconBackground = mToolbar.findViewById(R.id.location_bar_status_icon_bg);
        int expectedEndMarginForNtp =
                mToolbar.getResources()
                        .getDimensionPixelSize(R.dimen.location_bar_url_action_offset_ntp);
        int expectedEndMargin =
                mToolbar.getResources()
                        .getDimensionPixelSize(R.dimen.location_bar_url_action_offset);

        assertEquals(false, mToolbar.isLocationBarShownInNtp());
        assertEquals(View.INVISIBLE, iconBackground.getVisibility());
        assertEquals(
                expectedEndMargin,
                locationBarCoordinator.getUrlActionContainerEndMarginForTesting());

        // Load the new tab page.
        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);
        Tab tab = mActivityTestRule.getActivity().getActivityTab();
        NewTabPageTestUtils.waitForNtpLoaded(tab);
        assertEquals(true, mToolbar.isLocationBarShownInNtp());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mToolbar.setNtpSearchBoxScrollFractionForTesting(1);
                    mToolbar.updateLocationBarForNtp(
                            VisualState.NEW_TAB_NORMAL, /* hasFocus= */ false);
                });
        if (nightModeEnabled) {
            assertEquals(View.INVISIBLE, iconBackground.getVisibility());
        } else {
            assertEquals(View.VISIBLE, iconBackground.getVisibility());
        }
        assertEquals(
                expectedEndMarginForNtp,
                locationBarCoordinator.getUrlActionContainerEndMarginForTesting());

        // Focus on the Omnibox.
        mOmnibox.requestFocus();
        assertEquals(View.INVISIBLE, iconBackground.getVisibility());
        assertEquals(
                expectedEndMargin,
                locationBarCoordinator.getUrlActionContainerEndMarginForTesting());
    }

    @Test
    @MediumTest
    @DisableIf.Build(sdk_equals = VERSION_CODES.TIRAMISU, message = "crbug.com/339034032")
    public void testToolbarBackgroundChangedWhenSearchEngineHasNoLogo() {
        when(mTemplateUrlService.doesDefaultSearchEngineHaveLogo()).thenReturn(false);

        ColorDrawable toolbarBackgroundDrawable = mToolbar.getBackgroundDrawable();
        @ColorInt
        int homeSurfaceToolbarBackgroundColor =
                ChromeColors.getSurfaceColor(
                        mToolbar.getContext(),
                        org.chromium.chrome.browser.toolbar.R.dimen
                                .home_surface_background_color_elevation);

        assertEquals(false, mToolbar.isLocationBarShownInGeneralNtp());
        assertNotEquals(homeSurfaceToolbarBackgroundColor, toolbarBackgroundDrawable.getColor());

        // Load the new tab page.
        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);
        Tab tab = mActivityTestRule.getActivity().getActivityTab();
        NewTabPageTestUtils.waitForNtpLoaded(tab);
        assertEquals(true, mToolbar.isLocationBarShownInGeneralNtp());
        assertEquals(homeSurfaceToolbarBackgroundColor, toolbarBackgroundDrawable.getColor());

        // Focus the Omnibox.
        mOmnibox.requestFocus();
        assertNotEquals(homeSurfaceToolbarBackgroundColor, toolbarBackgroundDrawable.getColor());
    }

    @Test
    @MediumTest
    @EnableFeatures(OmniboxFeatureList.ANIMATE_SUGGESTIONS_LIST_APPEARANCE)
    public void testFocusAnimation_optionalButtonRestored() {
        mToolbar.setOptionalButtonCoordinatorForTesting(mOptionalButtonCoordinator);
        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);
        Tab tab = mActivityTestRule.getActivity().getActivityTab();
        NewTabPageTestUtils.waitForNtpLoaded(tab);
        assertEquals(true, mToolbar.isLocationBarShownInNtp());

        ButtonData buttonData =
                new ButtonDataImpl(
                        true,
                        AppCompatResources.getDrawable(
                                mActivityTestRule.getActivity(),
                                R.drawable.ic_toolbar_share_offset_24dp),
                        null,
                        mActivityTestRule.getActivity().getString(R.string.share),
                        false,
                        null,
                        true,
                        AdaptiveToolbarButtonVariant.UNKNOWN,
                        0,
                        false);
        mToolbar.updateOptionalButton(buttonData);
        verify(mOptionalButtonCoordinator).updateButton(buttonData);

        mOmnibox.requestFocus();
        verify(mOptionalButtonCoordinator).updateButton(null);
        mOmnibox.clearFocus();
        verify(mOptionalButtonCoordinator, times(2)).updateButton(buttonData);
    }

    @Test
    @MediumTest
    public void testGetLocationBarOffsetForFocusAnimation() {
        SearchEngineUtils.setInstanceForTesting(mSearchEngineUtils);

        // Test focus on non-NTP pages.
        doReturn(true).when(mSearchEngineUtils).shouldShowSearchEngineLogo();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(
                            0,
                            mToolbar.getLocationBarOffsetForFocusAnimation(/* hasFocus= */ true));
                });

        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);
        Tab tab = mActivityTestRule.getActivity().getActivityTab();
        NewTabPageTestUtils.waitForNtpLoaded(tab);
        assertEquals(true, mToolbar.isLocationBarShownInNtp());

        // Test focus when should not show search engine logo.
        doReturn(false).when(mSearchEngineUtils).shouldShowSearchEngineLogo();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(
                            0,
                            mToolbar.getLocationBarOffsetForFocusAnimation(/* hasFocus= */ true));
                });

        // Test un-focus on NTP.
        doReturn(true).when(mSearchEngineUtils).shouldShowSearchEngineLogo();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(
                            0,
                            mToolbar.getLocationBarOffsetForFocusAnimation(/* hasFocus= */ false));
                });

        // Test focus on NTP.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertNotEquals(
                            0,
                            mToolbar.getLocationBarOffsetForFocusAnimation(/* hasFocus= */ true));
                });
    }

    private static class TestControlsVisibilityDelegate
            extends BrowserStateBrowserControlsVisibilityDelegate {
        public TestControlsVisibilityDelegate() {
            super(new ObservableSupplierImpl<>(false));
        }
    }
}
