// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browser_controls;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;

import android.view.View;
import android.view.ViewGroup;

import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.transit.Element;
import org.chromium.base.test.transit.TripBuilder;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.ViewportTestUtils;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.ControlsPosition;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.fullscreen.FullscreenManagerTestUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.ToolbarPositionController.ToolbarPositionAndSource;
import org.chromium.chrome.browser.toolbar.settings.AddressBarPreference;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.browser_controls.BrowserControlsFacility;
import org.chromium.chrome.test.transit.browser_controls.BrowserControlsOffsetCondition;
import org.chromium.chrome.test.transit.browser_controls.BrowserControlsPositionCondition;
import org.chromium.chrome.test.transit.page.BasePageStation;
import org.chromium.chrome.test.transit.page.FrameInfoUpdatedCondition;
import org.chromium.chrome.test.transit.page.PageLoadCallbackCondition;
import org.chromium.chrome.test.transit.page.WebContentsPresentCondition;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.transit.testhtmls.TopBottomLinksPageStation;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.third_party.android.swiperefresh.SwipeRefreshLayout;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.test.util.DeviceRestriction;

import java.util.concurrent.TimeoutException;

/** Browser test for {@link TopControlsStacker}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add(ChromeSwitches.DISABLE_MINIMUM_SHOW_DURATION)
@Restriction(DeviceRestriction.RESTRICTION_TYPE_NON_AUTO)
public class BrowserControlsPTTest {

    @Rule
    public AutoResetCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.fastAutoResetCtaActivityRule();

    private TopBottomLinksPageStation mPage;
    private ChromeTabbedActivity mActivity;
    private ViewportTestUtils mViewportTestUtils;

    /**
     * Destination page station for navigations that keep top browser controls scrolled off. Extends
     * {@link BasePageStation} rather than {@link WebPageStation} because {@link WebPageStation}
     * requires top controls to be shown on arrival.
     */
    private static class ScrolledOffWebPageStation extends BasePageStation<ChromeTabbedActivity> {
        protected ScrolledOffWebPageStation(Config config) {
            super(ChromeTabbedActivity.class, config);
            Element<WebContents> webContents =
                    declareEnterConditionAsElement(
                            new WebContentsPresentCondition(loadedTabElement));
            declareEnterCondition(new FrameInfoUpdatedCondition(webContents));
            declareEnterCondition(BrowserControlsOffsetCondition.scrolledOff(getActivityElement()));
        }

        public static Builder<ScrolledOffWebPageStation> newBuilder() {
            return new Builder<>(ScrolledOffWebPageStation::new);
        }

        public TripBuilder dragDownFromPageTopTo() {
            return runTo(
                    () ->
                            dragDownFromPageTop(
                                    mActivityElement.value(), activityTabElement.value()));
        }
    }

    private static void dragDownFromPageTop(ChromeTabbedActivity activity, Tab tab) {
        View contentView = tab.getView();
        float x = contentView.getWidth() / 2f;
        float fromY = activity.getBrowserControlsManager().getTopControlsHeight() + 100f;
        float toY = contentView.getHeight();
        // Use wall-clock pacing so DidOverscroll returns from the renderer mid-gesture and
        // subsequent MOVE events drive SwipeRefreshLayout past activation instead of coalescing.
        TouchCommon.performWallClockDrag(
                activity,
                x,
                x,
                fromY,
                toY,
                /* duration= */ 500,
                /* dispatchIntervalMs= */ 16,
                /* preventFling= */ true);
    }

    @Before
    public void setup() {
        mViewportTestUtils = new ViewportTestUtils(mActivityTestRule);
        mViewportTestUtils.setUpForBrowserControls();

        var blankPage = mActivityTestRule.startOnBlankPage();
        mPage =
                TopBottomLinksPageStation.loadPage(
                        mActivityTestRule.getActivityTestRule(), blankPage);
        mActivity = mActivityTestRule.getActivity();
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        AddressBarPreference.setToolbarPositionAndSource(
                                ToolbarPositionAndSource.TOP_SETTINGS));
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.PHONE)
    public void bottomControlsShown() {
        mPage.runOnUiThreadTo(
                        () ->
                                AddressBarPreference.setToolbarPositionAndSource(
                                        ToolbarPositionAndSource.BOTTOM_SETTINGS))
                .waitFor(
                        BrowserControlsPositionCondition.at(
                                mPage.getActivityElement(), ControlsPosition.BOTTOM),
                        BrowserControlsOffsetCondition.shown(mPage.getActivityElement()));

        mPage.runOnUiThreadTo(
                        () ->
                                AddressBarPreference.setToolbarPositionAndSource(
                                        ToolbarPositionAndSource.TOP_SETTINGS))
                .waitFor(
                        BrowserControlsPositionCondition.at(
                                mPage.getActivityElement(), ControlsPosition.TOP),
                        BrowserControlsOffsetCondition.shown(mPage.getActivityElement()));
    }

    @Test
    @LargeTest
    @Restriction(DeviceFormFactor.PHONE_OR_TABLET)
    @DisabledTest(message = "https://crbug.com/471837397")
    public void topControlsScroll() {
        BrowserControlsFacility.waitForBrowserControlsToBeMoveable(mPage);

        var bottomFacility =
                mPage.topFacility.scrollToBottom(BrowserControlsFacility.scrolledOff());
        bottomFacility.scrollToTop(BrowserControlsFacility.shown());
    }

    @Test
    @LargeTest
    @Restriction(DeviceFormFactor.DESKTOP)
    @EnableFeatures(ChromeFeatureList.LOCK_TOP_CONTROLS_ON_LARGE_TABLETS_V2)
    public void topControlsScrollingDisabled() {
        var lockedControls =
                mPage.noopTo().enterFacility(BrowserControlsFacility.shownAndHeightSynced());
        mPage.topFacility.scrollToBottom();
        mPage.noopTo()
                .exitFacilityAnd(lockedControls)
                .enterFacility(BrowserControlsFacility.shownAndHeightSynced());
    }

    @Test
    @LargeTest
    @Restriction(DeviceFormFactor.PHONE)
    @EnableFeatures(ChromeFeatureList.CLANK_OMNIBOX_MINIMIZATION)
    public void twoStepPullToRefreshWhenTopControlsHiddenAcrossSameOriginNav()
            throws TimeoutException {
        // Extend the page to 400vh and scroll controls off via FullscreenManagerTestUtils rather
        // than BrowserControlsFacility.waitForBrowserControlsToBeMoveable(), whose hardcoded
        // 100px drags can stay below BrowserControlsScrollSnapAnimation's trigger threshold on
        // high-density screens until 150vh hits the bottom exclusion zone.
        JavaScriptUtils.executeJavaScriptAndWaitForResult(
                mPage.webContentsElement.value(), "document.body.style.height = '400vh';");
        FullscreenManagerTestUtils.waitForBrowserControlsToBeMoveable(
                mActivity, /* showControls= */ false);
        mPage.noopTo()
                .exitFacilityAnd(mPage.topFacility)
                .enterFacility(BrowserControlsFacility.scrolledOff());

        // Same-origin link navigation keeps top controls scrolled off on the new page (scrollY==0).
        // Click via DOM rather than touch because edge-to-edge places #bottom_link behind the
        // system gesture navigation bar once bottom controls scroll off.
        String destinationUrl =
                mActivityTestRule.getTestServer().getURL("/chrome/test/data/android/test.html");
        ScrolledOffWebPageStation destination =
                ScrolledOffWebPageStation.newBuilder()
                        .initForLoadingUrlOnSameTab(destinationUrl, mPage)
                        .build();
        mPage.runJsTo("document.getElementById('bottom_link').click()").arriveAt(destination);

        UserActionTester userActionTester = new UserActionTester();
        try {
            // Pull 1 at the top of the page reveals the Omnibox without triggering a reload.
            // Check SwipeRefreshLayout#isRefreshing() (set synchronously on release before the
            // refresh animation fires onRefresh()) alongside loading and user-action state so a
            // pending refresh animation from Pull 1 cannot slip past this assertion.
            WebPageStation revealedStation =
                    WebPageStation.newBuilder().initFrom(destination).build();
            destination
                    .dragDownFromPageTopTo()
                    .arriveAt(revealedStation, BrowserControlsFacility.shown());
            Tab tab = revealedStation.getTab();
            assertFalse(isSwipeRefreshRefreshing(tab));
            assertFalse(ThreadUtils.runOnUiThreadBlocking(tab::isLoading));
            assertEquals(0, userActionTester.getActionCount("MobilePullGestureReload"));

            // Pull 2 with the Omnibox fully visible triggers pull-to-refresh and reloads the page.
            revealedStation
                    .runTo(() -> dragDownFromPageTop(mActivity, tab))
                    .waitFor(new PageLoadCallbackCondition(tab));
            assertEquals(1, userActionTester.getActionCount("MobilePullGestureReload"));
        } finally {
            userActionTester.tearDown();
        }
    }

    private static boolean isSwipeRefreshRefreshing(Tab tab) {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ViewGroup contentView = tab.getContentView();
                    if (contentView == null) return false;
                    for (int i = 0; i < contentView.getChildCount(); i++) {
                        View child = contentView.getChildAt(i);
                        if (child instanceof SwipeRefreshLayout layout && layout.isRefreshing()) {
                            return true;
                        }
                    }
                    return false;
                });
    }
}
