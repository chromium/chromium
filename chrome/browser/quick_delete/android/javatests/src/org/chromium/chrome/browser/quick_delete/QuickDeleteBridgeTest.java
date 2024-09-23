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
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;

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

    private QuickDeleteBridge mQuickDeleteBridge;

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

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
        mActivityTestRule.startMainActivityOnBlankPage();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Profile profile =
                            mActivityTestRule.getActivity().getCurrentTabModel().getProfile();
                    mQuickDeleteBridge = new QuickDeleteBridge(profile);
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
    public void testLastVisitedDomainAndUniqueDomains_WhenNoVisits() throws TimeoutException {
        DomainVisitsCallback callback = new DomainVisitsCallback();
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mQuickDeleteBridge.getLastVisitedDomainAndUniqueDomainCount(
                                TimePeriod.LAST_15_MINUTES, callback));

        callback.mCallbackHelper.waitForCallback(0);

        assertEquals("", callback.mLastVisitedDomain);
        assertEquals(0, callback.mDomainCount);
    }

    @Test
    @MediumTest
    @Restriction(Restriction.RESTRICTION_TYPE_INTERNET)
    public void testLastVisitedDomainAndUniqueDomains_WhenVisitsExistInRange()
            throws TimeoutException {
        visitUrls();

        DomainVisitsCallback callback = new DomainVisitsCallback();
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mQuickDeleteBridge.getLastVisitedDomainAndUniqueDomainCount(
                                TimePeriod.LAST_15_MINUTES, callback));

        callback.mCallbackHelper.waitForCallback(0);

        assertEquals("google.com", callback.mLastVisitedDomain);
        assertEquals(2, callback.mDomainCount);
    }
}
