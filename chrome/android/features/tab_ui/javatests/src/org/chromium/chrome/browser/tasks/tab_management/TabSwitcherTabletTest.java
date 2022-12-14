// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.Visibility.GONE;
import static androidx.test.espresso.matcher.ViewMatchers.Visibility.VISIBLE;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withClassName;
import static androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.is;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.graphics.Bitmap;
import android.support.test.InstrumentationRegistry;
import android.view.ViewGroup;
import android.view.ViewStub;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.GarbageCollectionTestUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.RequiresRestart;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerImpl;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutHelperManager;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutTab;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutStateProvider.LayoutStateObserver;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.toolbar.top.TabSwitcherModeTopToolbar;
import org.chromium.chrome.browser.toolbar.top.ToolbarTablet;
import org.chromium.chrome.features.start_surface.TabSwitcherAndStartSurfaceLayout;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.TabStripUtils;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.DisableAnimationsTestRule;
import org.chromium.ui.test.util.UiRestriction;

import java.lang.ref.WeakReference;
import java.util.LinkedList;
import java.util.List;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/**
 * Tests for the {@link TabSwitcher} on tablet
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, "force-fieldtrials=Study/Group",
        "force-fieldtrial-params=Study.Group:enable_launch_polish/false/delay_creation/false"})
@EnableFeatures({ChromeFeatureList.GRID_TAB_SWITCHER_FOR_TABLETS + "<Study",
        ChromeFeatureList.TAB_STRIP_REDESIGN})
@DisableFeatures(ChromeFeatureList.TAB_TO_GTS_ANIMATION)
@Restriction(
        {Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE, UiRestriction.RESTRICTION_TYPE_TABLET})
@Batch(Batch.PER_CLASS)
public class TabSwitcherTabletTest {
    private static final long CALLBACK_WAIT_TIMEOUT = 15L;
    @ClassRule
    public static DisableAnimationsTestRule sEnableAnimationsRule =
            new DisableAnimationsTestRule(false);
    @ClassRule
    public static final ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();
    @Rule
    public final BlankCTATabInitialStateRule mInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    private List<WeakReference<Bitmap>> mAllBitmaps = new LinkedList<>();
    private TabSwitcher.TabListDelegate mTabListDelegate;

    private CallbackHelper mLayoutChangedCallbackHelper = new CallbackHelper();
    private int mCurrentlyActiveLayout;
    private Callback<LayoutManagerImpl> mLayoutManagerCallback;

    @Before
    public void setUp() throws ExecutionException {
        CriteriaHelper.pollUiThread(sActivityTestRule.getActivity()
                                            .getTabModelSelectorSupplier()
                                            .get()::isTabStateInitialized);

        LayoutStateObserver mLayoutObserver = new LayoutStateProvider.LayoutStateObserver() {
            @Override
            public void onFinishedHiding(int layoutType) {
                mCurrentlyActiveLayout = layoutType;
                mLayoutChangedCallbackHelper.notifyCalled();
            }
        };
        mLayoutManagerCallback = (manager) -> manager.addObserver(mLayoutObserver);
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> sActivityTestRule.getActivity().getLayoutManagerSupplier().addObserver(
                                mLayoutManagerCallback));
    }

    @After
    public void cleanup() throws TimeoutException {
        final ChromeTabbedActivity activity = sActivityTestRule.getActivity();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            activity.getLayoutManagerSupplier().removeObserver(mLayoutManagerCallback);
        });
    }

    @Test
    @MediumTest
    @RequiresRestart
    public void testEnterAndExitTabSwitcherVerifyThumbnails()
            throws ExecutionException, TimeoutException {
        prepareTabsWithThumbnail(1, 1);
        enterGTSWithThumbnailChecking();
        exitGTSAndVerifyThumbnailsAreReleased();
    }

    @Test
    @MediumTest
    public void testToggleIncognitoSwitcher() throws Exception {
        prepareTabs(1, 1);
        TabUiTestHelper.enterTabSwitcher(sActivityTestRule.getActivity());

        // Start with incognito switcher.
        assertTrue("Expected to be in Incognito model",
                sActivityTestRule.getActivity().getCurrentTabModel().isIncognito());
        // Toggle to normal switcher.
        clickIncognitoToggleButton();

        final Tab newTab = sActivityTestRule.getActivity().getCurrentTabModel().getTabAt(0);
        assertFalse(newTab.isIncognito());

        exitSwitcherWithTabClick(0);
    }

    @Test
    @MediumTest
    public void testTabSwitcherToolbar() throws Exception {
        prepareTabs(1, 1);
        TabUiTestHelper.enterTabSwitcher(sActivityTestRule.getActivity());

        // Assert hidden views.
        onView(allOf(withId(R.id.toolbar), withClassName(is(ToolbarTablet.class.getName()))))
                .check(matches(withEffectiveVisibility(GONE)));
        onView(allOf(withId(R.id.incognito_switch),
                       withParent(withId(R.id.tab_switcher_switches_and_menu))))
                .check(matches(withEffectiveVisibility(GONE)));
        onView(allOf(withId(R.id.new_tab_button), withParent(withId(R.id.tab_switcher_toolbar))))
                .check(matches(withEffectiveVisibility(GONE)));
        onView(allOf(withId(R.id.incognito_tabs_stub),
                       withParent(withId(R.id.tab_switcher_toolbar))))
                .check(matches(withEffectiveVisibility(GONE)));
        onView(allOf(withId(R.id.tab_switcher_mode_tab_switcher_button),
                       withParent(withId(R.id.tab_switcher_switches_and_menu))))
                .check(matches(withEffectiveVisibility(GONE)));

        // Assert visible views.
        onView(allOf(withId(R.id.new_tab_view), withParent(withId(R.id.tab_switcher_toolbar))))
                .check(matches(isDisplayed()));
        onView(allOf(withId(R.id.menu_button_wrapper),
                       withParent(withId(R.id.tab_switcher_switches_and_menu))))
                .check(matches(isDisplayed()));

        // Exit switcher.
        exitSwitcherWithTabClick(0);

        // Assert tablet toolbar shows and switcher toolbar is gone.
        onView(allOf(withId(R.id.tab_switcher_toolbar),
                       withClassName(is(TabSwitcherModeTopToolbar.class.getName()))))
                .check(matches(withEffectiveVisibility(GONE)));
        onView(allOf(withId(R.id.toolbar), withClassName(is(ToolbarTablet.class.getName()))))
                .check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({"force-fieldtrial-params=Study.Group:enable_launch_polish/true"})
    public void testTabSwitcherToolbar_withPolishFlag_incognitoTabsOpen() throws Exception {
        prepareTabs(1, 1);
        TabUiTestHelper.enterTabSwitcher(sActivityTestRule.getActivity());

        // Assert hidden views.
        onView(allOf(withId(R.id.incognito_switch),
                       withParent(withId(R.id.tab_switcher_switches_and_menu))))
                .check(matches(withEffectiveVisibility(GONE)));
        onView(allOf(withId(R.id.new_tab_button), withParent(withId(R.id.tab_switcher_toolbar))))
                .check(matches(withEffectiveVisibility(GONE)));
        onView(allOf(withId(R.id.tab_switcher_mode_tab_switcher_button),
                       withParent(withId(R.id.tab_switcher_switches_and_menu))))
                .check(matches(withEffectiveVisibility(GONE)));

        // Assert visible views.
        onView(allOf(withId(R.id.new_tab_view), withParent(withId(R.id.tab_switcher_toolbar))))
                .check(matches(isDisplayed()));
        onView(allOf(withId(R.id.menu_button_wrapper),
                       withParent(withId(R.id.tab_switcher_switches_and_menu))))
                .check(matches(isDisplayed()));
        onView(allOf(withId(R.id.incognito_toggle_tabs),
                       withParent(withId(R.id.tab_switcher_toolbar))))
                .check(matches(isDisplayed()));
        // Tablet toolbar is not hidden for polish
        onView(allOf(withId(R.id.toolbar), withClassName(is(ToolbarTablet.class.getName()))))
                .check(matches(isDisplayed()));

        // Exit switcher.
        exitSwitcherWithTabClick(0);

        // Assert tablet toolbar shows and switcher toolbar is gone.
        onView(allOf(withId(R.id.tab_switcher_toolbar),
                       withClassName(is(TabSwitcherModeTopToolbar.class.getName()))))
                .check(matches(withEffectiveVisibility(GONE)));
        onView(allOf(withId(R.id.toolbar), withClassName(is(ToolbarTablet.class.getName()))))
                .check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({"force-fieldtrial-params=Study.Group:enable_launch_polish/true"})
    public void testTabSwitcherV1Scrim() throws TimeoutException {
        prepareTabs(1, 1);
        TabUiTestHelper.enterTabSwitcher(sActivityTestRule.getActivity());

        ScrimCoordinator scrimCoordinator = sActivityTestRule.getActivity()
                                                    .getRootUiCoordinatorForTesting()
                                                    .getScrimCoordinator();
        assertTrue(scrimCoordinator.isShowingScrim());
        assertEquals(ChromeColors.getPrimaryBackgroundColor(sActivityTestRule.getActivity(), true),
                sActivityTestRule.getActivity()
                        .getRootUiCoordinatorForTesting()
                        .getStatusBarColorController()
                        .getScrimColorForTesting());

        exitSwitcherWithTabClick(0);
        assertFalse(scrimCoordinator.isShowingScrim());
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({"force-fieldtrial-params=Study.Group:enable_launch_polish/true"})
    public void testGridTabSwitcherOnNoNextTab() throws ExecutionException {
        // Assert the grid tab switcher is not yet showing.
        onView(withId(R.id.grid_tab_switcher_view_holder))
                .check(matches(withEffectiveVisibility(GONE)));

        // Close the only tab through the tab strip.
        closeTab(false, sActivityTestRule.getActivity().getCurrentTabModel().getTabAt(0).getId());

        // Assert the grid tab switcher is shown automatically, since there is no next tab.
        onView(withId(R.id.grid_tab_switcher_view_holder))
                .check(matches(withEffectiveVisibility(VISIBLE)));

        TestThreadUtils.runOnUiThreadBlocking(()
                                                      -> sActivityTestRule.getActivity()
                                                                 .findViewById(R.id.new_tab_button)
                                                                 .performClick());
    }

    @Test
    @MediumTest
    @CommandLineFlags.
    Add({"force-fieldtrial-params=Study.Group:enable_launch_polish/true/delay_creation/true"})
    public void testGridTabSwitcherV1DelayCreate() {
        Layout layout = sActivityTestRule.getActivity().getLayoutManager().getOverviewLayout();
        assertNull("StartSurface layout should not be initialized", layout);
        ViewStub tabSwitcherStub = (ViewStub) sActivityTestRule.getActivity().findViewById(
                R.id.grid_tab_switcher_view_holder_stub);
        assertTrue("TabSwitcher view stub should not be inflated",
                tabSwitcherStub.getParent() != null);

        // Click tab switcher button
        TabUiTestHelper.enterTabSwitcher(sActivityTestRule.getActivity());

        layout = sActivityTestRule.getActivity().getLayoutManager().getOverviewLayout();
        assertTrue("OverviewLayout should be TabSwitcherAndStartSurfaceLayout layout",
                layout instanceof TabSwitcherAndStartSurfaceLayout);
        ViewGroup tabSwitcherViewHolder =
                sActivityTestRule.getActivity().findViewById(R.id.grid_tab_switcher_view_holder);
        assertNotNull("TabSwitcher view should be inflated", tabSwitcherViewHolder);
    }

    protected void clickIncognitoToggleButton() {
        final CallbackHelper tabModelSelectedCallback = new CallbackHelper();
        TabModelSelectorObserver observer = new TabModelSelectorObserver() {
            @Override
            public void onTabModelSelected(TabModel newModel, TabModel oldModel) {
                tabModelSelectedCallback.notifyCalled();
            }
        };
        TestThreadUtils.runOnUiThreadBlocking(()
                                                      -> sActivityTestRule.getActivity()
                                                                 .getTabModelSelectorSupplier()
                                                                 .get()
                                                                 .addObserver(observer));
        StripLayoutHelperManager manager =
                TabStripUtils.getStripLayoutHelperManager(sActivityTestRule.getActivity());
        TabStripUtils.clickCompositorButton(manager.getModelSelectorButton(),
                InstrumentationRegistry.getInstrumentation(), sActivityTestRule.getActivity());
        try {
            tabModelSelectedCallback.waitForCallback(0);
        } catch (TimeoutException e) {
            Assert.fail("Tab model selected event never occurred.");
        }
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            sActivityTestRule.getActivity().getTabModelSelector().removeObserver(observer);
        });
    }

    private void prepareTabsWithThumbnail(int numTabs, int numIncognitoTabs) {
        setupForThumbnailCheck();
        int oldCount = mTabListDelegate.getBitmapFetchCountForTesting();
        TabUiTestHelper.prepareTabsWithThumbnail(
            sActivityTestRule, numTabs, numIncognitoTabs, null);
        assertEquals(0, mTabListDelegate.getBitmapFetchCountForTesting() - oldCount);
    }

    private void prepareTabs(int numTabs, int numIncognitoTabs) {
        TabUiTestHelper.createTabs(sActivityTestRule.getActivity(), false, numTabs);
        TabUiTestHelper.createTabs(sActivityTestRule.getActivity(), true, numIncognitoTabs);
    }

    private void setupForThumbnailCheck() {
        Layout layout = sActivityTestRule.getActivity().getLayoutManager().getOverviewLayout();
        assertTrue(layout instanceof TabSwitcherAndStartSurfaceLayout);
        TabSwitcherAndStartSurfaceLayout mTabSwitcherAndStartSurfaceLayout =
                (TabSwitcherAndStartSurfaceLayout) layout;

        mTabListDelegate = mTabSwitcherAndStartSurfaceLayout.getStartSurfaceForTesting()
                                   .getGridTabListDelegate();
        Callback<Bitmap> mBitmapListener = (bitmap) -> mAllBitmaps.add(new WeakReference<>(bitmap));
        mTabListDelegate.setBitmapCallbackForTesting(mBitmapListener);
    }

    private void exitGTSAndVerifyThumbnailsAreReleased()
            throws TimeoutException, ExecutionException {
        assertTrue(sActivityTestRule.getActivity().getLayoutManager().isLayoutVisible(
                LayoutType.TAB_SWITCHER));

        final int index = sActivityTestRule.getActivity().getCurrentTabModel().index();
        exitSwitcherWithTabClick(index);

        assertThumbnailsAreReleased();
    }

    private void exitSwitcherWithTabClick(int index) throws TimeoutException {
        TabUiTestHelper.clickNthCardFromTabSwitcher(sActivityTestRule.getActivity(), index);
        mLayoutChangedCallbackHelper.waitForCallback(1, 1, CALLBACK_WAIT_TIMEOUT, TimeUnit.SECONDS);
        assertTrue(mCurrentlyActiveLayout == LayoutType.TAB_SWITCHER);
    }

    private void enterGTSWithThumbnailChecking() {
        Tab currentTab = sActivityTestRule.getActivity().getTabModelSelector().getCurrentTab();
        // Native tabs need to be invalidated first to trigger thumbnail taking, so skip them.
        boolean checkThumbnail = !currentTab.isNativePage();

        if (checkThumbnail) {
            sActivityTestRule.getActivity().getTabContentManager().removeTabThumbnail(
                    currentTab.getId());
        }
        TabUiTestHelper.enterTabSwitcher(sActivityTestRule.getActivity());

        TabUiTestHelper.verifyAllTabsHaveThumbnail(
                sActivityTestRule.getActivity().getCurrentTabModel());
    }

    private void assertThumbnailsAreReleased() {
        CriteriaHelper.pollUiThread(() -> {
            for (WeakReference<Bitmap> bitmap : mAllBitmaps) {
                if (!GarbageCollectionTestUtils.canBeGarbageCollected(bitmap)) {
                    return false;
                }
            }
            return true;
        });
    }

    private void closeTab(final boolean incognito, final int id) {
        ChromeTabUtils.closeTabWithAction(InstrumentationRegistry.getInstrumentation(),
                sActivityTestRule.getActivity(), () -> {
                    StripLayoutTab tab = TabStripUtils.findStripLayoutTab(
                            sActivityTestRule.getActivity(), incognito, id);
                    TabStripUtils.clickCompositorButton(tab.getCloseButton(),
                            InstrumentationRegistry.getInstrumentation(),
                            sActivityTestRule.getActivity());
                });
    }
}
