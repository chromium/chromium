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
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerChrome;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerImpl;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.layouts.animation.CompositorAnimationHandler;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.components.browser_ui.widget.gesture.SwipeGestureListener.ScrollDirection;
import org.chromium.components.browser_ui.widget.gesture.SwipeGestureListener.SwipeHandler;
import org.chromium.content_public.browser.test.util.UiUtils;
import org.chromium.ui.base.DeviceFormFactor;

/** Tests swiping the toolbar to switch tabs. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class ToolbarSwipeTest {
    @Rule
    public AutoResetCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.fastAutoResetCtaActivityRule();

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
        mActivityTestRule
                .getActivity()
                .setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED);
    }

    @Test
    @MediumTest
    @Feature({"Android-TabSwitcher"})
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testToolbarSwipeOnlyTab() {
        WebPageStation pageStation =
                initToolbarSwipeTest(
                        /* useTwoTabs= */ false,
                        /* selectFirstTab= */ true,
                        /* incognito= */ false);

        pageStation =
                pageStation
                        .swipeToolbarToPreviousTabTo()
                        .arriveAt(WebPageStation.newBuilder().initFrom(pageStation).build());
        assertSelectedTabIndex(pageStation, 0);

        pageStation =
                pageStation
                        .swipeToolbarToNextTabTo()
                        .arriveAt(WebPageStation.newBuilder().initFrom(pageStation).build());
        assertSelectedTabIndex(pageStation, 0);
    }

    @Test
    @MediumTest
    @Feature({"Android-TabSwitcher"})
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testToolbarSwipePrevTab() {
        WebPageStation pageStation =
                initToolbarSwipeTest(
                        /* useTwoTabs= */ true,
                        /* selectFirstTab= */ false,
                        /* incognito= */ false);
        pageStation = pageStation.swipeToolbarToPreviousTab(pageStation);
        assertSelectedTabIndex(pageStation, 0);
    }

    @Test
    @MediumTest
    @Feature({"Android-TabSwitcher"})
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testToolbarSwipeNextTab() {
        WebPageStation pageStation =
                initToolbarSwipeTest(
                        /* useTwoTabs= */ true, /* selectFirstTab= */ true, /* incognito= */ false);
        pageStation = pageStation.swipeToolbarToNextTab(pageStation);
        assertSelectedTabIndex(pageStation, 1);
    }

    @Test
    @MediumTest
    @Feature({"Android-TabSwitcher"})
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testToolbarSwipePrevTabNone() {
        WebPageStation pageStation =
                initToolbarSwipeTest(
                        /* useTwoTabs= */ true, /* selectFirstTab= */ true, /* incognito= */ false);
        pageStation = pageStation.swipeToolbarToPreviousTab(pageStation);
        assertSelectedTabIndex(pageStation, 0);
    }

    @Test
    @MediumTest
    @Feature({"Android-TabSwitcher"})
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testToolbarSwipeNextTabNone() {
        WebPageStation pageStation =
                initToolbarSwipeTest(
                        /* useTwoTabs= */ true,
                        /* selectFirstTab= */ false,
                        /* incognito= */ false);
        pageStation = pageStation.swipeToolbarToNextTab(pageStation);
        assertSelectedTabIndex(pageStation, 1);
    }

    @Test
    @MediumTest
    @Feature({"Android-TabSwitcher"})
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testToolbarSwipeNextThenPrevTab() {
        WebPageStation pageStation =
                initToolbarSwipeTest(
                        /* useTwoTabs= */ true, /* selectFirstTab= */ true, /* incognito= */ false);
        pageStation = pageStation.swipeToolbarToNextTab(pageStation);
        assertSelectedTabIndex(pageStation, 1);
        pageStation = pageStation.swipeToolbarToPreviousTab(pageStation);
        assertSelectedTabIndex(pageStation, 0);
    }

    @Test
    @MediumTest
    @Feature({"Android-TabSwitcher"})
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testToolbarSwipeNextThenPrevTabIncognito() {
        WebPageStation pageStation =
                initToolbarSwipeTest(
                        /* useTwoTabs= */ true, /* selectFirstTab= */ false, /* incognito= */ true);
        pageStation = pageStation.swipeToolbarToNextTab(pageStation);
        assertSelectedTabIndex(pageStation, 1);
        pageStation = pageStation.swipeToolbarToPreviousTab(pageStation);
        assertSelectedTabIndex(pageStation, 0);
    }

    /**
     * Initialize a test for the toolbar swipe behavior.
     *
     * @param useTwoTabs Whether the test should use two tabs. One tab is used if {@code false}.
     * @param selectFirstTab The tab index in the current model to have selected after the tabs are
     *     loaded.
     * @param incognito Whether the test should run on incognito tabs.
     */
    private WebPageStation initToolbarSwipeTest(
            boolean useTwoTabs, boolean selectFirstTab, boolean incognito) {
        WebPageStation pageStation;
        if (incognito) {
            // If incognito, there is no default tab, so open a new tab/window and switch to it.
            pageStation =
                    mActivityTestRule
                            .startOnBlankPage()
                            .openNewIncognitoTabOrWindowFast()
                            .loadWebPageProgrammatically(generateSolidColorUrl("#00ff00"));
        } else {
            // If not incognito, use the tab the test started on.
            pageStation = mActivityTestRule.startOnWebPage(generateSolidColorUrl("#00ff00"));
        }
        Tab tab1 = pageStation.getTab();

        if (useTwoTabs) {
            pageStation = pageStation.openFakeLinkToWebPage(generateSolidColorUrl("#0000ff"));
            if (selectFirstTab) {
                pageStation = pageStation.selectTabFast(tab1, WebPageStation::newBuilder);
            }
        }
        return pageStation;
    }

    /** Test that swipes and tab transitions are not causing URL bar to be focused. */
    @Test
    @MediumTest
    @Restriction({DeviceFormFactor.PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    @Feature({"Android-TabSwitcher"})
    public void testOSKIsNotShownDuringSwipe() throws InterruptedException {
        final View urlBar = mActivityTestRule.getActivity().findViewById(R.id.url_bar);
        final LayoutManagerChrome layoutManager = updateTabsViewSize();
        final SwipeHandler swipeHandler = layoutManager.getToolbarSwipeHandler();

        UiUtils.settleDownUI(InstrumentationRegistry.getInstrumentation());
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> urlBar.requestFocus());
        UiUtils.settleDownUI(InstrumentationRegistry.getInstrumentation());

        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> urlBar.clearFocus());
        UiUtils.settleDownUI(InstrumentationRegistry.getInstrumentation());
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
        UiUtils.settleDownUI(InstrumentationRegistry.getInstrumentation());

        Assert.assertFalse(
                "Keyboard somehow got shown",
                mActivityTestRule.getKeyboardDelegate().isKeyboardShowing(urlBar));

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
                    return !mActivityTestRule
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
                            mActivityTestRule.getKeyboardDelegate().isKeyboardShowing(urlBar));
                    swipeHandler.onSwipeFinished();
                });

        CriteriaHelper.pollUiThread(
                () -> {
                    LayoutManagerImpl driver = mActivityTestRule.getActivity().getLayoutManager();
                    return driver.getActiveLayout().shouldDisplayContentOverlay();
                },
                "Layout not requesting Tab Android view be attached");

        Assert.assertFalse(
                "Keyboard should not be shown",
                mActivityTestRule.getKeyboardDelegate().isKeyboardShowing(urlBar));
    }

    private LayoutManagerChrome updateTabsViewSize() {
        View tabsView = mActivityTestRule.getActivity().getTabsView();
        mTabsViewWidthDp = tabsView.getWidth() * mPxToDp;
        return mActivityTestRule.getActivity().getLayoutManager();
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

    private static void assertSelectedTabIndex(WebPageStation station, int selectedIndex) {
        Assert.assertEquals(
                selectedIndex, ChromeTabUtils.getIndexOnUiThread(station.getTabModel()));
    }
}
