// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.quick_delete;

import static org.junit.Assert.assertEquals;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.browsing_data.BrowsingDataBridge;
import org.chromium.chrome.browser.browsing_data.BrowsingDataType;
import org.chromium.chrome.browser.browsing_data.TimePeriod;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.page.WebPageStation;

import java.util.List;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/** Tests for {@link QuickDeleteBridge}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class QuickDeleteBridgeTest {
    private static final List<String> URLS =
            List.of(
                    "https://www.google.com/",
                    "https://www.example.com/",
                    "https://www.google.com/");

    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    private WebPageStation mPage;
    private QuickDeleteBridge mQuickDeleteBridge;
    private DomainVisitsCallback mDomainVisitsCallback;

    private static class DomainVisitsCallback implements QuickDeleteBridge.DomainVisitsCallback {
        private final CallbackHelper mCallbackHelper = new CallbackHelper();
        private String mLastVisitedDomain;
        private int mDomainCount;

        @Override
        public void onLastVisitedDomainAndUniqueDomainCountReady(
                String lastVisitedDomain, int domainCount) {
            mLastVisitedDomain = lastVisitedDomain;
            mDomainCount = domainCount;
            mCallbackHelper.notifyCalled();
        }
    }

    @Before
    public void setUp() throws ExecutionException {
        mPage = mActivityTestRule.startOnBlankPage();
        mDomainVisitsCallback = new DomainVisitsCallback();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Profile profile =
                            mActivityTestRule.getActivity().getCurrentTabModel().getProfile();
                    mQuickDeleteBridge = new QuickDeleteBridge(profile, mDomainVisitsCallback);
                });
    }

    @After
    public void tearDown() throws TimeoutException {
        CallbackHelper callbackHelper = new CallbackHelper();

        // Clear history.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    BrowsingDataBridge.getForProfile(ProfileManager.getLastUsedRegularProfile())
                            .clearBrowsingData(
                                    callbackHelper::notifyCalled,
                                    new int[] {BrowsingDataType.HISTORY},
                                    TimePeriod.ALL_TIME);
                });

        callbackHelper.waitForCallback(0);
    }

    private void visitUrls() {
        URLS.forEach(url -> mActivityTestRule.loadUrl(url));
    }

    @Test
    @MediumTest
    public void testRestartCounterForTimePeriod_WhenNoVisits() throws TimeoutException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> mQuickDeleteBridge.restartCounterForTimePeriod(TimePeriod.LAST_15_MINUTES));

        mDomainVisitsCallback.mCallbackHelper.waitForCallback(0);

        assertEquals("", mDomainVisitsCallback.mLastVisitedDomain);
        assertEquals(0, mDomainVisitsCallback.mDomainCount);
    }

    @Test
    @MediumTest
    @Restriction(Restriction.RESTRICTION_TYPE_INTERNET)
    public void testRestartCounterForTimePeriod_WhenVisitsExistInRange() throws TimeoutException {
        visitUrls();

        ThreadUtils.runOnUiThreadBlocking(
                () -> mQuickDeleteBridge.restartCounterForTimePeriod(TimePeriod.LAST_15_MINUTES));

        mDomainVisitsCallback.mCallbackHelper.waitForCallback(0);

        assertEquals("google.com", mDomainVisitsCallback.mLastVisitedDomain);
        assertEquals(2, mDomainVisitsCallback.mDomainCount);
    }
}
