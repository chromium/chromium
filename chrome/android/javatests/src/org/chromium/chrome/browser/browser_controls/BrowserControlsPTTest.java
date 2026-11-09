// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browser_controls;

import static org.junit.Assert.assertEquals;

import android.view.View;

import androidx.test.filters.LargeTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

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
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.browser_controls.BrowserControlsFacility;
import org.chromium.chrome.test.transit.browser_controls.BrowserControlsOffsetCondition;
import org.chromium.chrome.test.transit.page.BasePageStation;
import org.chromium.chrome.test.transit.page.FrameInfoUpdatedCondition;
import org.chromium.chrome.test.transit.page.PageLoadCallbackCondition;
import org.chromium.chrome.test.transit.page.WebContentsPresentCondition;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.transit.testhtmls.TopBottomLinksPageStation;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.test.util.DeviceRestriction;

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
                    () -> {
                        assertInPhase(Phase.ACTIVE);
                        View contentView = activityTabElement.value().getView();
                        float x = contentView.getWidth() / 2f;
                        // Match WebPageStation#scrollPageUpWithGestureTo()'s exact drag endpoints
                        // so Pull 1 and Pull 2 cover the same distance.
                        float fromY =
                                getActivity().getBrowserControlsManager().getTopControlsHeight()
                                        + 100f;
                        float toY = contentView.getHeight();
                        TouchCommon.performDragNoFling(
                                mActivityElement.value(),
                                x,
                                x,
                                fromY,
                                toY,
                                /* stepCount= */ 50,
                                /* duration= */ 500);
                    });
        }
    }

    @Before
    public void setup() {
        mViewportTestUtils = new ViewportTestUtils(mActivityTestRule);
        mViewportTestUtils.setUpForBrowserControls();

        var blankPage = mActivityTestRule.startOnBlankPage();
        mPage =
                TopBottomLinksPageStation.loadPageNoFacility(
                        mActivityTestRule.getActivityTestRule(), blankPage);
        mActivity = mActivityTestRule.getActivity();
    }

    @Test
    @LargeTest
    @Restriction(DeviceFormFactor.PHONE_OR_TABLET)
    @DisabledTest(message = "https://crbug.com/471837397")
    public void topControlsScroll() {
        BrowserControlsFacility.waitForBrowserControlsToBeMoveable(mPage);

        mPage.scrollToBottom(BrowserControlsFacility.scrolledOff(mPage));
        mPage.scrollToTop(BrowserControlsFacility.shown(mPage));
    }

    @Test
    @LargeTest
    @Restriction(DeviceFormFactor.DESKTOP)
    @EnableFeatures(ChromeFeatureList.LOCK_TOP_CONTROLS_ON_LARGE_TABLETS_V2)
    @DisabledTest(message = "https://crbug.com/471837396")
    public void topControlsScrollingDisabled() {
        mPage.noopTo()
                .waitFor(
                        BrowserControlsOffsetCondition.showAndHeightSynced(
                                mPage.getActivityElement()));
        mPage.scrollToBottom(null);
        mPage.noopTo()
                .waitFor(
                        BrowserControlsOffsetCondition.showAndHeightSynced(
                                mPage.getActivityElement()));
    }

    @Test
    @LargeTest
    @Restriction(DeviceFormFactor.PHONE)
    @DisabledTest(message = "crbug.com/579544393")
    @EnableFeatures(ChromeFeatureList.CLANK_OMNIBOX_MINIMIZATION)
    public void twoStepPullToRefreshWhenTopControlsHiddenAcrossSameOriginNav() {
        BrowserControlsFacility.waitForBrowserControlsToBeMoveable(mPage);

        var bottomFacility = new TopBottomLinksPageStation.BottomFacility();
        mPage.scrollPageDownWithGestureTo()
                .withRetry()
                .enterFacilities(BrowserControlsFacility.scrolledOff(mPage), bottomFacility);

        // Same-origin link navigation keeps top controls scrolled off on the new page (scrollY==0).
        String destinationUrl =
                mActivityTestRule.getTestServer().getURL("/chrome/test/data/android/test.html");
        ScrolledOffWebPageStation destination =
                ScrolledOffWebPageStation.newBuilder()
                        .initForLoadingUrlOnSameTab(destinationUrl, mPage)
                        .build();
        bottomFacility.bottomElement.clickTo().arriveAt(destination);

        UserActionTester userActionTester = new UserActionTester();
        try {
            // Pull 1 at the top of the page reveals the Omnibox without triggering a reload.
            WebPageStation revealedStation =
                    WebPageStation.newBuilder().initFrom(destination).build();
            destination
                    .dragDownFromPageTopTo()
                    .arriveAt(revealedStation, BrowserControlsFacility.shown(revealedStation));
            assertEquals(0, userActionTester.getActionCount("MobilePullGestureReload"));

            // Pull 2 with the Omnibox fully visible triggers pull-to-refresh and reloads the page.
            WebPageStation reloadedStation =
                    WebPageStation.newBuilder().initFrom(revealedStation).build();
            revealedStation
                    .scrollPageUpWithGestureTo()
                    .waitForAnd(new PageLoadCallbackCondition(revealedStation.getTab()))
                    .arriveAt(reloadedStation, BrowserControlsFacility.shown(reloadedStation));
            assertEquals(1, userActionTester.getActionCount("MobilePullGestureReload"));
        } finally {
            userActionTester.tearDown();
        }
    }
}
