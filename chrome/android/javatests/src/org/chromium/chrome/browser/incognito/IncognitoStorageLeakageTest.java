// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.LargeTest;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.params.ParameterAnnotations.UseMethodParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.chrome.browser.customtabs.IncognitoCustomTabActivityTestRule;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.incognito.IncognitoDataTestUtils.ActivityType;
import org.chromium.chrome.browser.incognito.IncognitoDataTestUtils.TestParams;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabUtils.LoadIfNeededCaller;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.Arrays;
import java.util.List;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * This test class checks various site storage leaks between all
 * different pairs of Activity types with a constraint that one of the
 * interacting activity must be either incognito tab or incognito CCT.
 */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@EnableFeatures({ChromeFeatureList.CCT_INCOGNITO})
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class IncognitoStorageLeakageTest {
    private static final String SITE_DATA_HTML_PATH =
            "/content/test/data/browsing_data/site_data.html";

    private static final List<String> sSiteData = Arrays.asList(
            "LocalStorage", "ServiceWorker", "CacheStorage", "IndexedDb", "FileSystem", "WebSql");

    private String mSiteDataTestPage;
    private EmbeddedTestServer mTestServer;

    @Rule
    public ChromeTabbedActivityTestRule mChromeActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public IncognitoCustomTabActivityTestRule mCustomTabActivityTestRule =
            new IncognitoCustomTabActivityTestRule();

    @Before
    public void setUp() throws TimeoutException {
        mTestServer = EmbeddedTestServer.createAndStartServer(
                ApplicationProvider.getApplicationContext());
        mSiteDataTestPage = mTestServer.getURL(SITE_DATA_HTML_PATH);

        // Ensuring native is initialized before we access the CCT_INCOGNITO feature flag.
        IncognitoDataTestUtils.fireAndWaitForCctWarmup();
        assertTrue(ChromeFeatureList.isEnabled(ChromeFeatureList.CCT_INCOGNITO));
    }

    @After
    public void tearDown() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> IncognitoDataTestUtils.closeTabs(mChromeActivityTestRule));
        mTestServer.stopAndDestroyServer();
    }

    @Test
    @LargeTest
    @UseMethodParameter(TestParams.AllTypesToAllTypes.class)
    public void testSessionStorageDoesNotLeakFromActivityToActivity(
            String activityType1, String activityType2) throws TimeoutException {
        ActivityType activity1 = ActivityType.valueOf(activityType1);
        ActivityType activity2 = ActivityType.valueOf(activityType2);

        Tab tab1 = activity1.launchUrl(
                mChromeActivityTestRule, mCustomTabActivityTestRule, mSiteDataTestPage);
        CriteriaHelper.pollUiThread(
                () -> Criteria.checkThat(tab1.getWebContents(), Matchers.notNullValue()));

        // Sets the session storage in tab1
        assertEquals("true",
                JavaScriptUtils.executeJavaScriptAndWaitForResult(
                        tab1.getWebContents(), "setSessionStorage()"));

        // Checks the sessions storage is set in tab1
        assertEquals("true",
                JavaScriptUtils.executeJavaScriptAndWaitForResult(
                        tab1.getWebContents(), "hasSessionStorage()"));

        Tab tab2 = activity2.launchUrl(
                mChromeActivityTestRule, mCustomTabActivityTestRule, mSiteDataTestPage);
        CriteriaHelper.pollUiThread(
                () -> Criteria.checkThat(tab2.getWebContents(), Matchers.notNullValue()));

        // Checks the session storage in tab2. Session storage set in tab1 should not be accessible.
        // The session storage is per tab basis.
        assertEquals("false",
                JavaScriptUtils.executeJavaScriptAndWaitForResult(
                        tab2.getWebContents(), "hasSessionStorage()"));
    }

    @Test
    @LargeTest
    @DisabledTest(message = "crbug.com/1107600")
    @UseMethodParameter(TestParams.AllTypesToAllTypes.class)
    public void testStorageDoesNotLeakFromActivityToActivity(String activityType1,
            String activityType2) throws ExecutionException, TimeoutException {
        ActivityType activity1 = ActivityType.valueOf(activityType1);
        ActivityType activity2 = ActivityType.valueOf(activityType2);

        Tab tab1 = activity1.launchUrl(
                mChromeActivityTestRule, mCustomTabActivityTestRule, mSiteDataTestPage);

        Tab tab2 = activity2.launchUrl(
                mChromeActivityTestRule, mCustomTabActivityTestRule, mSiteDataTestPage);

        for (String type : sSiteData) {
            String expected = "false";

            // Both activity types are regular and they share storages.
            if (!activity1.incognito && !activity2.incognito) {
                expected = "true";
            }

            // Since the test required launching tab2 right after launching tab1
            // it may happen that these tabs were fired in different Activities.
            // Due to which tab1 could potentially be marked as frozen and invoking
            // getWebContents on it may return null. Please see the javadoc for
            // TabImpl#getWebContents.
            TestThreadUtils.runOnUiThreadBlocking(
                    () -> tab1.loadIfNeeded(LoadIfNeededCaller.OTHER));
            CriteriaHelper.pollUiThread(
                    () -> Criteria.checkThat(tab1.getWebContents(), Matchers.notNullValue()));
            // Set the storage in tab1
            assertEquals("true",
                    JavaScriptUtils.runJavascriptWithAsyncResult(
                            tab1.getWebContents(), "set" + type + "Async()"));
            // Checks the storage is set in tab1
            assertEquals("true",
                    JavaScriptUtils.runJavascriptWithAsyncResult(
                            tab1.getWebContents(), "has" + type + "Async()"));

            TestThreadUtils.runOnUiThreadBlocking(
                    () -> tab2.loadIfNeeded(LoadIfNeededCaller.OTHER));
            CriteriaHelper.pollUiThread(
                    () -> Criteria.checkThat(tab2.getWebContents(), Matchers.notNullValue()));
            // Access the storage from tab2
            assertEquals(expected,
                    JavaScriptUtils.runJavascriptWithAsyncResult(
                            tab2.getWebContents(), "has" + type + "Async()"));
        }
    }
}
