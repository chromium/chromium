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
import static org.hamcrest.Matchers.not;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.graphics.Bitmap;
import android.view.ViewGroup;
import android.view.ViewStub;

import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

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
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
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
import java.util.HashSet;
import java.util.Set;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/**
 * Tests for the {@link TabSwitcher} on tablet
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.
Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, "force-fieldtrials=Study/Group"})
@EnableFeatures({ChromeFeatureList.TAB_STRIP_REDESIGN, ChromeFeatureList.EMPTY_STATES})
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

    private CallbackHelper mLayoutChangedCallbackHelper = new CallbackHelper();
    private int mCurrentlyActiveLayout;
    private Callback<LayoutManagerImpl> mLayoutManagerCallback;

    private Set<WeakReference<Bitmap>> mAllBitmaps = new HashSet<>();
    private TabSwitcher.TabListDelegate mTabListDelegate;

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

            @Override
            public void onStartedShowing(int layoutType) {
                if (layoutType != LayoutType.TAB_SWITCHER) {
                    return;
                }
                setupForThumbnailCheck();
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
        if (mTabListDelegate != null) mTabListDelegate.resetBitmapFetchCountForTesting();
    }

    @Test
    @MediumTest
    @RequiresRestart
    public void testEnterAndExitTabSwitcher() throws TimeoutException {
        Layout layout = sActivityTestRule.getActivity().getLayoutManager().getOverviewLayout();
        assertNull("StartSurface layout should not be initialized", layout);
        ViewStub tabSwitcherStub = (ViewStub) sActivityTestRule.getActivity().findViewById(
                R.id.tab_switcher_view_holder_stub);
        assertTrue("TabSwitcher view stub should not be inflated",
                tabSwitcherStub.getParent() != null);

        TabUiTestHelper.prepareTabsWithThumbnail(sActivityTestRule, 1, 0, null);
        enterGTSWithThumbnailChecking();
        layout = sActivityTestRule.getActivity().getLayoutManager().getOverviewLayout();
        assertTrue("OverviewLayout should be TabSwitcherAndStartSurfaceLayout layout",
                layout instanceof TabSwitcherAndStartSurfaceLayout);
        ViewGroup tabSwitcherViewHolder =
                sActivityTestRule.getActivity().findViewById(R.id.tab_switcher_view_holder);
        assertNotNull("TabSwitcher view should be inflated", tabSwitcherViewHolder);

        exitGTSAndVerifyThumbnailsAreReleased(1);
        assertFalse(sActivityTestRule.getActivity().getLayoutManager().isLayoutVisible(
                LayoutType.TAB_SWITCHER));
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
        onView(allOf(withId(R.id.incognito_switch),
                       withParent(withId(R.id.tab_switcher_switches_and_menu))))
                .check(matches(withEffectiveVisibility(GONE)));
        onView(allOf(withId(R.id.new_tab_button), withParent(withId(R.id.tab_switcher_toolbar))))
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
        // Tablet toolbar is not hidden.
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
    public void testTabSwitcherScrim() throws TimeoutException {
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
    public void testGridTabSwitcherOnNoNextTab() throws ExecutionException {
        // Assert the grid tab switcher is not yet showing.
        onView(withId(R.id.tab_switcher_view_holder)).check(matches(withEffectiveVisibility(GONE)));

        // Close the only tab through the tab strip.
        closeTab(false, sActivityTestRule.getActivity().getCurrentTabModel().getTabAt(0).getId());

        // Assert the grid tab switcher is shown automatically, since there is no next tab.
        onView(withId(R.id.tab_switcher_view_holder))
                .check(matches(withEffectiveVisibility(VISIBLE)));
    }

    @Test
    @MediumTest
    public void testGridTabSwitcherOnCloseAllTabs() throws ExecutionException {
        // Assert the grid tab switcher is not yet showing.
        onView(withId(R.id.tab_switcher_view_holder)).check(matches(withEffectiveVisibility(GONE)));

        // Close all tabs.
        ChromeTabUtils.closeAllTabs(InstrumentationRegistry.getInstrumentation(),
                sActivityTestRule.getActivity().getTabModelSelectorSupplier());

        // Assert the grid tab switcher is shown automatically, since there is no next tab.
        onView(withId(R.id.tab_switcher_view_holder))
                .check(matches(withEffectiveVisibility(VISIBLE)));
    }

    @Test
    @MediumTest
    public void testGridTabSwitcherToggleIncognitoWithNoRegularTab() throws ExecutionException {
        // Close all the regular tabs.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            sActivityTestRule.getActivity().getCurrentTabModel().closeTab(
                    sActivityTestRule.getActivity().getActivityTab());
        });
        assertEquals("Expected to be 0 tabs in regular model", 0,
                sActivityTestRule.getActivity()
                        .getTabModelSelectorSupplier()
                        .get()
                        .getModel(false)
                        .getCount());
        // Open an incognito tab.
        prepareTabs(0, 1);
        assertTrue("Expected to be in Incognito model",
                sActivityTestRule.getActivity().getCurrentTabModel().isIncognito());
        // Assert the grid tab switcher is not yet showing.
        onView(withId(R.id.tab_switcher_view_holder)).check(matches(withEffectiveVisibility(GONE)));
        // Toggle to normal switcher.
        clickIncognitoToggleButton();
        // Assert the grid tab switcher is shown automatically, since there is no regular tab.
        onView(withId(R.id.tab_switcher_view_holder))
                .check(matches(withEffectiveVisibility(VISIBLE)));
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.START_SURFACE_REFACTOR)
    public void testGridTabSwitcher_RefactorEnabled() throws ExecutionException {
        prepareTabs(2, 0);
        // Verifies that the dialog visibility supplier doesn't crash when closing a Tab without the
        // grid tab switcher is inflated.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            sActivityTestRule.getActivity().getCurrentTabModel().closeTab(
                    sActivityTestRule.getActivity().getActivityTab());
        });

        Layout layout =
                sActivityTestRule.getActivity().getLayoutManager().getTabSwitcherLayoutForTesting();
        assertNull("StartSurface layout should not be initialized", layout);
        ViewStub tabSwitcherStub = (ViewStub) sActivityTestRule.getActivity().findViewById(
                R.id.tab_switcher_view_holder_stub);
        assertTrue("TabSwitcher view stub should not be inflated",
                tabSwitcherStub.getParent() != null);

        // Click tab switcher button
        TabUiTestHelper.enterTabSwitcher(sActivityTestRule.getActivity());

        layout =
                sActivityTestRule.getActivity().getLayoutManager().getTabSwitcherLayoutForTesting();
        assertTrue("OverviewLayout should be TabSwitcherAndStartSurfaceLayout layout",
                layout instanceof TabSwitcherLayout);
        ViewGroup tabSwitcherViewHolder =
                sActivityTestRule.getActivity().findViewById(R.id.tab_switcher_view_holder);
        assertNotNull("TabSwitcher view should be inflated", tabSwitcherViewHolder);
    }

    // Regression test for crbug.com/1487114.
    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.START_SURFACE_REFACTOR,
            ChromeFeatureList.DEFER_TAB_SWITCHER_LAYOUT_CREATION})
    public void
    testGridTabSwitcher_DeferredTabSwitcherLayoutCreation_RefactorEnabled()
            throws ExecutionException {
        prepareTabs(2, 0);
        // Verifies that the dialog visibility supplier doesn't crash when closing a Tab without the
        // grid tab switcher is inflated.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            sActivityTestRule.getActivity().getCurrentTabModel().closeTab(
                    sActivityTestRule.getActivity().getActivityTab());
        });

        Layout layout =
                sActivityTestRule.getActivity().getLayoutManager().getTabSwitcherLayoutForTesting();
        assertNull("StartSurface layout should not be initialized", layout);
        ViewStub tabSwitcherStub = (ViewStub) sActivityTestRule.getActivity().findViewById(
                R.id.tab_switcher_view_holder_stub);
        assertTrue("TabSwitcher view stub should not be inflated",
                tabSwitcherStub.getParent() != null);

        // Click tab switcher button
        TabUiTestHelper.enterTabSwitcher(sActivityTestRule.getActivity());

        layout =
                sActivityTestRule.getActivity().getLayoutManager().getTabSwitcherLayoutForTesting();
        assertTrue("OverviewLayout should be TabSwitcherAndStartSurfaceLayout layout",
                layout instanceof TabSwitcherLayout);
        ViewGroup tabSwitcherViewHolder =
                sActivityTestRule.getActivity().findViewById(R.id.tab_switcher_view_holder);
        assertNotNull("TabSwitcher view should be inflated", tabSwitcherViewHolder);
    }

    @Test
    @MediumTest
    public void testEmptyStateView() throws Exception {
        prepareTabs(1, 0);
        TabUiTestHelper.enterTabSwitcher(sActivityTestRule.getActivity());

        // Close the last tab.
        closeTab(false, sActivityTestRule.getActivity().getCurrentTabModel().getTabAt(0).getId());

        // Check whether empty view show up.
        onView(allOf(withId(R.id.empty_state_container),
                       withParent(withId(R.id.tab_switcher_view_holder))))
                .check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testEmptyStateView_ToggleIncognito() throws Exception {
        prepareTabs(1, 1);
        TabUiTestHelper.enterTabSwitcher(sActivityTestRule.getActivity());

        // Close the last normal tab.
        closeTab(false, sActivityTestRule.getActivity().getCurrentTabModel().getTabAt(0).getId());

        // Switch to incognito tab switcher.
        clickIncognitoToggleButton();

        // Check empty view should never show up in incognito tab switcher.
        onView(allOf(withId(R.id.empty_state_container),
                       withParent(withId(R.id.tab_switcher_view_holder))))
                .check(matches(not(isDisplayed())));

        // Close the last incognito tab.
        closeTab(true, sActivityTestRule.getActivity().getCurrentTabModel().getTabAt(0).getId());

        // Incognito tab switcher should exit to go to normal tab switcher and we should see empty
        // view.
        onView(allOf(withId(R.id.empty_state_container),
                       withParent(withId(R.id.tab_switcher_view_holder))))
                .check(matches(isDisplayed()));
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

    private void prepareTabs(int numTabs, int numIncognitoTabs) {
        TabUiTestHelper.createTabs(sActivityTestRule.getActivity(), false, numTabs);
        TabUiTestHelper.createTabs(sActivityTestRule.getActivity(), true, numIncognitoTabs);
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
            TestThreadUtils.runOnUiThreadBlocking(() -> {
                sActivityTestRule.getActivity().getTabContentManager().removeTabThumbnail(
                        currentTab.getId());
            });
        }
        TabUiTestHelper.enterTabSwitcher(sActivityTestRule.getActivity());

        TabUiTestHelper.verifyAllTabsHaveThumbnail(
                sActivityTestRule.getActivity().getCurrentTabModel());
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

    private void retrieveTabListDelegate() {
        Layout layout = sActivityTestRule.getActivity().getLayoutManager().getOverviewLayout();
        assertTrue(layout instanceof TabSwitcherAndStartSurfaceLayout);
        TabSwitcherAndStartSurfaceLayout mTabSwitcherAndStartSurfaceLayout =
                (TabSwitcherAndStartSurfaceLayout) layout;

        mTabListDelegate = mTabSwitcherAndStartSurfaceLayout.getStartSurfaceForTesting()
                                   .getGridTabListDelegate();
    }

    private void setupForThumbnailCheck() {
        retrieveTabListDelegate();
        Callback<Bitmap> mBitmapListener = (bitmap) -> mAllBitmaps.add(new WeakReference<>(bitmap));
        mTabListDelegate.setBitmapCallbackForTesting(mBitmapListener);
        assertEquals(0, mTabListDelegate.getBitmapFetchCountForTesting());
    }

    private void exitGTSAndVerifyThumbnailsAreReleased(int tabsWithThumbnail)
            throws TimeoutException {
        assertTrue(sActivityTestRule.getActivity().getLayoutManager().isLayoutVisible(
                LayoutType.TAB_SWITCHER));

        if (mTabListDelegate == null) retrieveTabListDelegate();
        assertTrue(mTabListDelegate.getBitmapFetchCountForTesting() > 0);
        assertEquals(tabsWithThumbnail, mAllBitmaps.size());

        final int index = sActivityTestRule.getActivity().getCurrentTabModel().index();
        exitSwitcherWithTabClick(index);
        assertThumbnailsAreReleased();
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
}
