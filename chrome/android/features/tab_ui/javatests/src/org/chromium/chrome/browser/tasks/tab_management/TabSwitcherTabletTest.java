// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.Visibility.GONE;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withClassName;
import static androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.is;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.graphics.Bitmap;
import android.support.test.InstrumentationRegistry;

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
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerImpl;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutHelperManager;
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
import org.chromium.chrome.features.start_surface.StartSurfaceLayout;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.TabStripUtils;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
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
@CommandLineFlags.
Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, "force-fieldtrials=Study/Group"})
@EnableFeatures({ChromeFeatureList.GRID_TAB_SWITCHER_FOR_TABLETS + "<Study"})
@DisableFeatures(ChromeFeatureList.TAB_TO_GTS_ANIMATION)
@Restriction(
        {Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE, UiRestriction.RESTRICTION_TYPE_TABLET})
public class TabSwitcherTabletTest {
    private static final long CALLBACK_WAIT_TIMEOUT = 15L;
    @ClassRule
    public static DisableAnimationsTestRule sEnableAnimationsRule =
            new DisableAnimationsTestRule(false);
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private List<WeakReference<Bitmap>> mAllBitmaps = new LinkedList<>();
    private TabSwitcher.TabListDelegate mTabListDelegate;

    private CallbackHelper mLayoutChangedCallbackHelper = new CallbackHelper();
    private int mCurrentlyActiveLayout;
    private Callback<LayoutManagerImpl> mLayoutManagerCallback;

    @Before
    public void setUp() throws ExecutionException {
        mActivityTestRule.startMainActivityOnBlankPage();
        CriteriaHelper.pollUiThread(mActivityTestRule.getActivity()
                                            .getTabModelSelectorSupplier()
                                            .get()::isTabStateInitialized);

        Layout layout = mActivityTestRule.getActivity().getLayoutManager().getOverviewLayout();
        assertTrue(layout instanceof StartSurfaceLayout);
        StartSurfaceLayout mStartSurfaceLayout = (StartSurfaceLayout) layout;

        mTabListDelegate = mStartSurfaceLayout.getStartSurfaceForTesting().getGridTabListDelegate();
        Callback<Bitmap> mBitmapListener = (bitmap) -> mAllBitmaps.add(new WeakReference<>(bitmap));
        mTabListDelegate.setBitmapCallbackForTesting(mBitmapListener);

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
                        -> mActivityTestRule.getActivity().getLayoutManagerSupplier().addObserver(
                                mLayoutManagerCallback));

        prepareTabs(1, 1);
    }

    @After
    public void cleanup() {
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> mActivityTestRule.getActivity()
                                   .getLayoutManagerSupplier()
                                   .removeObserver(mLayoutManagerCallback));
    }

    @Test
    @MediumTest
    public void testEnterAndExitTabSwitcherVerifyThumbnails()
            throws ExecutionException, TimeoutException {
        enterGTSWithThumbnailChecking();
        exitGTSAndVerifyThumbnailsAreReleased();
    }

    @Test
    @MediumTest
    public void testToggleIncognitoSwitcher() throws InterruptedException, ExecutionException {
        TabUiTestHelper.enterTabSwitcher(mActivityTestRule.getActivity());

        // Start with incognito switcher.
        final Tab currTab = mActivityTestRule.getActivity().getCurrentTabModel().getTabAt(0);
        assertTrue(currTab.isIncognito());
        // Toggle to normal switcher.
        clickIncognitoToggleButton();

        final Tab newTab = mActivityTestRule.getActivity().getCurrentTabModel().getTabAt(0);
        assertFalse(newTab.isIncognito());
    }

    @Test
    @MediumTest
    public void testTabSwitcherToolbar()
            throws InterruptedException, ExecutionException, TimeoutException {
        TabUiTestHelper.enterTabSwitcher(mActivityTestRule.getActivity());

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
    public void testTabSwitcherToolbar_withPolishFlag_incognitoTabsOpen()
            throws InterruptedException, ExecutionException, TimeoutException {
        TabUiTestHelper.enterTabSwitcher(mActivityTestRule.getActivity());

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
        exitSwitcherPolishWithTabClick(0);

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
        TabUiTestHelper.enterTabSwitcher(mActivityTestRule.getActivity());

        ScrimCoordinator scrimCoordinator = mActivityTestRule.getActivity()
                                                    .getRootUiCoordinatorForTesting()
                                                    .getScrimCoordinator();
        assertTrue(scrimCoordinator.isShowingScrim());

        exitSwitcherPolishWithTabClick(0);
        assertFalse(scrimCoordinator.isShowingScrim());
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
                                                      -> mActivityTestRule.getActivity()
                                                                 .getTabModelSelectorSupplier()
                                                                 .get()
                                                                 .addObserver(observer));
        StripLayoutHelperManager manager =
                TabStripUtils.getStripLayoutHelperManager(mActivityTestRule.getActivity());
        TabStripUtils.clickCompositorButton(manager.getModelSelectorButton(),
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
        try {
            tabModelSelectedCallback.waitForCallback(0);
        } catch (TimeoutException e) {
            Assert.fail("Tab model selected event never occurred.");
        }
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mActivityTestRule.getActivity().getTabModelSelector().removeObserver(observer);
        });
    }

    private void prepareTabs(int numTabs, int numIncognitoTabs) {
        int oldCount = mTabListDelegate.getBitmapFetchCountForTesting();
        TabUiTestHelper.prepareTabsWithThumbnail(
                mActivityTestRule, numTabs, numIncognitoTabs, null);
        assertEquals(0, mTabListDelegate.getBitmapFetchCountForTesting() - oldCount);
    }

    private void exitGTSAndVerifyThumbnailsAreReleased()
            throws TimeoutException, ExecutionException {
        assertTrue(mActivityTestRule.getActivity().getLayoutManager().isLayoutVisible(
                LayoutType.TAB_SWITCHER));

        final int index = mActivityTestRule.getActivity().getCurrentTabModel().index();
        exitSwitcherWithTabClick(index);

        assertThumbnailsAreReleased();
    }

    private void exitSwitcherWithTabClick(int index) throws TimeoutException {
        TabUiTestHelper.clickNthCardFromTabSwitcher(mActivityTestRule.getActivity(), index);
        mLayoutChangedCallbackHelper.waitForCallback(1, 1, CALLBACK_WAIT_TIMEOUT, TimeUnit.SECONDS);
        assertTrue(mCurrentlyActiveLayout == LayoutType.TAB_SWITCHER);
    }

    private void exitSwitcherPolishWithTabClick(int index) throws TimeoutException {
        TabUiTestHelper.clickNthCardFromTabletTabSwitcherPolish(
                mActivityTestRule.getActivity(), index);
        mLayoutChangedCallbackHelper.waitForCallback(1, 1, CALLBACK_WAIT_TIMEOUT, TimeUnit.SECONDS);
        assertTrue(mCurrentlyActiveLayout == LayoutType.TAB_SWITCHER);
    }

    private void enterGTSWithThumbnailChecking() {
        Tab currentTab = mActivityTestRule.getActivity().getTabModelSelector().getCurrentTab();
        // Native tabs need to be invalidated first to trigger thumbnail taking, so skip them.
        boolean checkThumbnail = !currentTab.isNativePage();

        if (checkThumbnail) {
            mActivityTestRule.getActivity().getTabContentManager().removeTabThumbnail(
                    currentTab.getId());
        }
        TabUiTestHelper.enterTabSwitcher(mActivityTestRule.getActivity());

        TabUiTestHelper.verifyAllTabsHaveThumbnail(
                mActivityTestRule.getActivity().getCurrentTabModel());
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
