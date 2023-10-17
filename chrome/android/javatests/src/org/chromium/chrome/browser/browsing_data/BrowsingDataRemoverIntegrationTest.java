// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browsing_data;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.verification.ChromeVerificationResultStore;
import org.chromium.chrome.browser.browsing_data.BrowsingDataBridge.OnClearBrowsingDataListener;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.webapps.TestFetchStorageCallback;
import org.chromium.chrome.browser.webapps.WebappRegistry;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.webapps.WebappTestHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.TimeoutException;

/**
 * Integration tests for the native BrowsingDataRemover.
 *
 * <p>BrowsingDataRemover is used to delete data from various data storage backends. However, for
 * those backends that live in the Java code, it is not possible to test whether deletions were
 * successful in its own unit tests. This test can do so.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class BrowsingDataRemoverIntegrationTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    private void registerWebapp(final String webappId, final String webappUrl) throws Exception {
        BrowserServicesIntentDataProvider intentDataProvider =
                WebappTestHelper.createIntentDataProvider(webappId, webappUrl);
        TestFetchStorageCallback callback = new TestFetchStorageCallback();
        WebappRegistry.getInstance().register(webappId, callback);
        callback.waitForCallback(0);
        callback.getStorage().updateFromWebappIntentDataProvider(intentDataProvider);
    }

    /**
     * Tests that web apps are unregistered after clearing with the "cookies and site data" option.
     * TODO(msramek): Expose more granular datatypes to the Java code, so we can directly test
     * BrowsingDataRemover::RemoveDataMask::REMOVE_WEBAPP_DATA instead of BrowsingDataType.COOKIES.
     */
    @Test
    @MediumTest
    public void testUnregisteringWebapps() throws Exception {
        // Register three web apps.
        final HashMap<String, String> apps = new HashMap<String, String>();
        apps.put("webapp1", "https://www.google.com/index.html");
        apps.put("webapp2", "https://www.chrome.com/foo/bar");
        apps.put("webapp3", "http://example.com/");

        for (final Map.Entry<String, String> app : apps.entrySet()) {
            registerWebapp(app.getKey(), app.getValue());
        }
        Assert.assertEquals(apps.keySet(), WebappRegistry.getRegisteredWebappIdsForTesting());

        CallbackHelper dataClearedExcludingDomainHelper = new CallbackHelper();
        // Clear cookies and site data excluding the registrable domain "google.com".
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    BrowsingDataBridge.getInstance()
                            .clearBrowsingDataExcludingDomains(
                                    new OnClearBrowsingDataListener() {
                                        @Override
                                        public void onBrowsingDataCleared() {
                                            dataClearedExcludingDomainHelper.notifyCalled();
                                        }
                                    },
                                    new int[] {BrowsingDataType.COOKIES},
                                    TimePeriod.ALL_TIME,
                                    new String[] {"google.com"},
                                    new int[] {1},
                                    new String[0],
                                    new int[0]);
                });
        dataClearedExcludingDomainHelper.waitForFirst();

        // The last two webapps should have been unregistered.
        Assert.assertEquals(
                new HashSet<String>(Arrays.asList("webapp1")),
                WebappRegistry.getRegisteredWebappIdsForTesting());

        CallbackHelper dataClearedNoUrlFilterHelper = new CallbackHelper();
        // Clear cookies and site data with no url filter.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    BrowsingDataBridge.getInstance()
                            .clearBrowsingData(
                                    new OnClearBrowsingDataListener() {
                                        @Override
                                        public void onBrowsingDataCleared() {
                                            dataClearedNoUrlFilterHelper.notifyCalled();
                                        }
                                    },
                                    new int[] {BrowsingDataType.COOKIES},
                                    TimePeriod.ALL_TIME);
                });
        dataClearedNoUrlFilterHelper.waitForFirst();

        // All webapps should have been unregistered.
        Assert.assertTrue(WebappRegistry.getRegisteredWebappIdsForTesting().isEmpty());
    }

    @Test
    @MediumTest
    public void testClearingTrustedWebActivityVerifications() throws TimeoutException {
        CallbackHelper callbackHelper = new CallbackHelper();

        String relationship = "relationship1";
        Set<String> savedLinks = new HashSet<>();
        savedLinks.add(relationship);

        ChromeVerificationResultStore mStore =
                ChromeVerificationResultStore.getInstanceForTesting();

        mStore.setRelationships(savedLinks);

        Assert.assertTrue(mStore.getRelationships().contains(relationship));

        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    BrowsingDataBridge.getInstance()
                            .clearBrowsingData(
                                    callbackHelper::notifyCalled,
                                    new int[] {BrowsingDataType.HISTORY},
                                    TimePeriod.ALL_TIME);
                });

        callbackHelper.waitForCallback(0);
        Assert.assertTrue(mStore.getRelationships().isEmpty());
    }
}
