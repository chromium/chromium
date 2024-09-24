// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;
import static org.chromium.ui.test.util.ViewUtils.createMotionEvent;

import android.content.pm.ActivityInfo;
import android.view.View;

import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerChrome;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerImpl;
import org.chromium.chrome.browser.compositor.layouts.SceneChangeObserver;
import org.chromium.chrome.browser.compositor.layouts.StaticLayout;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.layouts.animation.CompositorAnimationHandler;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabModelObserver;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.components.browser_ui.widget.gesture.SwipeGestureListener.ScrollDirection;
import org.chromium.components.browser_ui.widget.gesture.SwipeGestureListener.SwipeHandler;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.content_public.browser.test.util.UiUtils;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.concurrent.TimeoutException;

/** Tests swiping the toolbar to switch tabs. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class ToolbarSwipeTest {

    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStateRule mBlankCTATabInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    private float mPxToDp = 1.0f;
    private float mTabsViewWidthDp;

    @Before
    public void setUp() throws InterruptedException {
        float dpToPx =
                InstrumentationRegistry.getInstrumentation()
                        .getContext()
                        .getResources()
                        .getDisplayMetrics()
                        .density;
        mPxToDp = 1.0f / dpToPx;

        CompositorAnimationHandler.setTestingMode(true);
    }

    @After
    public void tearDown() {
        sActivityTestRule
                .getActivity()
                .setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED);
    }

    @Test
    @MediumTest
    @Feature({"Android-TabSwitcher"})
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testToolbarSwipeOnlyTab() throws TimeoutException {
        initToolbarSwipeTest(false, 0, false);
        runToolbarSideSwipeTestOnCurrentModel(ScrollDirection.RIGHT, 0, false);
        runToolbarSideSwipeTestOnCurrentModel(ScrollDirection.LEFT, 0, false);
    }

    @Test
    @MediumTest
    @Feature({"Android-TabSwitcher"})
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testToolbarSwipePrevTab() throws TimeoutException {
        initToolbarSwipeTest(true, 1, false);
        runToolbarSideSwipeTestOnCurrentModel(ScrollDirection.RIGHT, 0, true);
    }

    @Test
    @MediumTest
    @Feature({"Android-TabSwitcher"})
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testToolbarSwipeNextTab() throws TimeoutException {
        initToolbarSwipeTest(true, 0, false);
        runToolbarSideSwipeTestOnCurrentModel(ScrollDirection.LEFT, 1, true);
    }

    @Test
    @MediumTest
    @Feature({"Android-TabSwitcher"})
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testToolbarSwipePrevTabNone() throws TimeoutException {
        initToolbarSwipeTest(true, 0, false);
        runToolbarSideSwipeTestOnCurrentModel(ScrollDirection.RIGHT, 0, false);
    }

    @Test
    @MediumTest
    @Feature({"Android-TabSwitcher"})
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testToolbarSwipeNextTabNone() throws TimeoutException {
        initToolbarSwipeTest(true, 1, false);
        runToolbarSideSwipeTestOnCurrentModel(ScrollDirection.LEFT, 1, false);
    }

    @Test
    @MediumTest
    @Feature({"Android-TabSwitcher"})
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testToolbarSwipeNextThenPrevTab() throws TimeoutException {
        initToolbarSwipeTest(true, 0, false);

        runToolbarSideSwipeTestOnCurrentModel(ScrollDirection.LEFT, 1, true);

        final TabModel tabModel =
                sActivityTestRule.getActivity().getTabModelSelector().getModel(false);
        Assert.assertEquals("Incorrect tab index after first swipe.", 1, tabModel.index());

        runToolbarSideSwipeTestOnCurrentModel(ScrollDirection.RIGHT, 0, true);
    }

    @Test
    @MediumTest
    @Feature({"Android-TabSwitcher"})
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testToolbarSwipeNextThenPrevTabIncognito() throws TimeoutException {
        initToolbarSwipeTest(true, 0, true);

        runToolbarSideSwipeTestOnCurrentModel(ScrollDirection.LEFT, 1, true);

        final TabModel tabModel =
                sActivityTestRule.getActivity().getTabModelSelector().getModel(true);
        Assert.assertEquals("Incorrect tab index after first swipe.", 1, tabModel.index());

        runToolbarSideSwipeTestOnCurrentModel(ScrollDirection.RIGHT, 0, true);
    }

    /**
     * Initialize a test for the toolbar swipe behavior.
     *
     * @param useTwoTabs Whether the test should use two tabs. One tab is used if {@code false}.
     * @param selectedTab The tab index in the current model to have selected after the tabs are
     *     loaded.
     * @param incognito Whether the test should run on incognito tabs.
     */
    private void initToolbarSwipeTest(boolean useTwoTabs, int selectedTab, boolean incognito) {
        if (incognito) {
            // If incognito, there is no default tab, so open a new one and switch to it.
            sActivityTestRule.loadUrlInNewTab(generateSolidColorUrl("#00ff00"), true);
            sActivityTestRule.getActivity().getTabModelSelector().selectModel(true);
        } else {
            // If not incognito, use the tab the test started on.
            sActivityTestRule.loadUrl(generateSolidColorUrl("#00ff00"));
        }

        if (useTwoTabs) {
            sActivityTestRule.loadUrlInNewTab(generateSolidColorUrl("#0000ff"), incognito);
        }

        ChromeTabUtils.switchTabInCurrentTabModel(sActivityTestRule.getActivity(), selectedTab);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        final TabModelSelector tabModelSelector =
                sActivityTestRule.getActivity().getTabModelSelector();
        final TabModel tabModel = tabModelSelector.getModel(incognito);

        Assert.assertEquals(
                "Incorrect model selected.",
                incognito,
                tabModelSelector.getCurrentModel().isIncognito());
        Assert.assertEquals("Incorrect starting index.", selectedTab, tabModel.index());
        Assert.assertEquals("Incorrect tab count.", useTwoTabs ? 2 : 1, tabModel.getCount());
    }

    private void runToolbarSideSwipeTestOnCurrentModel(
            @ScrollDirection int direction, int finalIndex, boolean expectsSelection)
            throws TimeoutException {
        final CallbackHelper selectCallback = new CallbackHelper();
        final ChromeTabbedActivity activity = sActivityTestRule.getActivity();
        final int id = activity.getCurrentTabModel().getTabAt(finalIndex).getId();
        final TabModelSelectorTabModelObserver observer =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return new TabModelSelectorTabModelObserver(
                                    activity.getTabModelSelector()) {
                                @Override
                                public void didSelectTab(
                                        Tab tab, @TabSelectionType int type, int lastId) {
                                    if (tab.getId() == id) selectCallback.notifyCalled();
                                }
                            };
                        });

        int tabSelectedCallCount = selectCallback.getCallCount();

        // Listen for changes in the layout to indicate the swipe has completed.
        final CallbackHelper staticLayoutCallbackHelper = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    activity.getCompositorViewHolderForTesting()
                            .getLayoutManager()
                            .addSceneChangeObserver(
                                    new SceneChangeObserver() {
                                        @Override
                                        public void onSceneChange(Layout layout) {
                                            if (layout instanceof StaticLayout) {
                                                staticLayoutCallbackHelper.notifyCalled();
                                            }
                                        }
                                    });
                });

        int callLayoutChangeCount = staticLayoutCallbackHelper.getCallCount();
        performToolbarSideSwipe(direction);
        staticLayoutCallbackHelper.waitForCallback(callLayoutChangeCount, 1);

        if (expectsSelection) selectCallback.waitForCallback(tabSelectedCallCount, 1);
        ThreadUtils.runOnUiThreadBlocking(() -> observer.destroy());

        Assert.assertEquals(
                "Index after toolbar side swipe is incorrect",
                finalIndex,
                activity.getCurrentTabModel().index());
    }

    private void performToolbarSideSwipe(@ScrollDirection int direction) {
        Assert.assertTrue(
                "Unexpected direction for side swipe " + direction,
                direction == ScrollDirection.LEFT || direction == ScrollDirection.RIGHT);
        final View toolbar = sActivityTestRule.getActivity().findViewById(R.id.toolbar);

        int[] toolbarPos = new int[2];
        toolbar.getLocationOnScreen(toolbarPos);
        final int width = toolbar.getWidth();
        final int height = toolbar.getHeight();

        final int fromX = toolbarPos[0] + width / 2;
        final int toX = toolbarPos[0] + (direction == ScrollDirection.LEFT ? 0 : width);
        final int y = toolbarPos[1] + height / 2;
        final int stepCount = 25;
        final long duration = 500;

        View toolbarRoot =
                sActivityTestRule
                        .getActivity()
                        .getRootUiCoordinatorForTesting()
                        .getToolbarManager()
                        .getContainerViewForTesting();
        TouchCommon.performDrag(toolbarRoot, fromX, toX, y, y, stepCount, duration);
    }

    /** Test that swipes and tab transitions are not causing URL bar to be focused. */
    @Test
    @MediumTest
    @Restriction({DeviceFormFactor.PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    @Feature({"Android-TabSwitcher"})
    public void testOSKIsNotShownDuringSwipe() throws InterruptedException {
        final View urlBar = sActivityTestRule.getActivity().findViewById(R.id.url_bar);
        final LayoutManagerChrome layoutManager = updateTabsViewSize();
        final SwipeHandler swipeHandler = layoutManager.getToolbarSwipeHandler();

        UiUtils.settleDownUI(InstrumentationRegistry.getInstrumentation());
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> urlBar.requestFocus());
        UiUtils.settleDownUI(InstrumentationRegistry.getInstrumentation());

        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> urlBar.clearFocus());
        UiUtils.settleDownUI(InstrumentationRegistry.getInstrumentation());
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), sActivityTestRule.getActivity());
        UiUtils.settleDownUI(InstrumentationRegistry.getInstrumentation());

        Assert.assertFalse(
                "Keyboard somehow got shown",
                sActivityTestRule
                        .getKeyboardDelegate()
                        .isKeyboardShowing(sActivityTestRule.getActivity(), urlBar));

        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    swipeHandler.onSwipeStarted(ScrollDirection.RIGHT, createMotionEvent(0, 0));
                    float swipeXChange = mTabsViewWidthDp / 2.f;
                    swipeHandler.onSwipeUpdated(
                            createMotionEvent(swipeXChange / mPxToDp, 0.f),
                            swipeXChange / mPxToDp,
                            0.f,
                            swipeXChange / mPxToDp,
                            0.f);
                });

        CriteriaHelper.pollUiThread(
                () -> {
                    return !sActivityTestRule
                            .getActivity()
                            .getLayoutManager()
                            .getActiveLayout()
                            .shouldDisplayContentOverlay();
                });

        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    Assert.assertFalse(
                            "Keyboard should be hidden while swiping",
                            sActivityTestRule
                                    .getKeyboardDelegate()
                                    .isKeyboardShowing(sActivityTestRule.getActivity(), urlBar));
                    swipeHandler.onSwipeFinished();
                });

        CriteriaHelper.pollUiThread(
                () -> {
                    LayoutManagerImpl driver = sActivityTestRule.getActivity().getLayoutManager();
                    return driver.getActiveLayout().shouldDisplayContentOverlay();
                },
                "Layout not requesting Tab Android view be attached");

        Assert.assertFalse(
                "Keyboard should not be shown",
                sActivityTestRule
                        .getKeyboardDelegate()
                        .isKeyboardShowing(sActivityTestRule.getActivity(), urlBar));
    }

    private LayoutManagerChrome updateTabsViewSize() {
        View tabsView = sActivityTestRule.getActivity().getTabsView();
        mTabsViewWidthDp = tabsView.getWidth() * mPxToDp;
        return sActivityTestRule.getActivity().getLayoutManager();
    }

    /**
     * Generate a URL that shows a web page with a solid color. This makes visual debugging easier.
     *
     * @param htmlColor The HTML/CSS color the page should display.
     * @return A URL that shows the solid color when loaded.
     */
    private static String generateSolidColorUrl(String htmlColor) {
        return UrlUtils.encodeHtmlDataUri(
                "<html><head><style>"
                        + "  body { background-color: "
                        + htmlColor
                        + ";}"
                        + "</style></head>"
                        + "<body></body></html>");
    }
}
