// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import android.content.Intent;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.LargeTest;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.params.ParameterAnnotations.UseMethodParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterProvider;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.customtabs.CustomTabsIntentTestUtils;
import org.chromium.chrome.browser.customtabs.IncognitoCustomTabActivityTestRule;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.history.BrowsingHistoryBridge;
import org.chromium.chrome.browser.history.HistoryItem;
import org.chromium.chrome.browser.history.TestBrowsingHistoryObserver;
import org.chromium.chrome.browser.incognito.IncognitoDataTestUtils.ActivityType;
import org.chromium.chrome.browser.incognito.IncognitoDataTestUtils.TestParams;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.content_public.browser.NavigationEntry;
import org.chromium.content_public.browser.NavigationHistory;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeoutException;

/**
 * Testing browsing and navigation history leaks. Browsing history leaks are checked from incognito
 * to regular mode. Navigation history leaks are checked across all tabbed and CCT activity types.
 */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class IncognitoHistoryLeakageTest {
    private static final String TEST_PAGE_1 = "/chrome/test/data/android/google.html";
    private static final String TEST_PAGE_2 = "/chrome/test/data/android/test.html";

    private EmbeddedTestServer mTestServer;
    private String mTestPage1;
    private String mTestPage2;

    @Rule
    public ChromeTabbedActivityTestRule mChromeActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public IncognitoCustomTabActivityTestRule mCustomTabActivityTestRule =
            new IncognitoCustomTabActivityTestRule();

    @Before
    public void setUp() throws TimeoutException {
        mTestServer =
                EmbeddedTestServer.createAndStartServer(
                        ApplicationProvider.getApplicationContext());
        mTestPage1 = mTestServer.getURL(TEST_PAGE_1);
        mTestPage2 = mTestServer.getURL(TEST_PAGE_2);
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> IncognitoDataTestUtils.closeTabs(mChromeActivityTestRule));
    }

    /**
     * Returns browsing history for the profile related to |tab|. If |tab| is null, the regular
     * profile is used.
     */
    private static List<HistoryItem> getBrowsingHistory(Tab tab) throws TimeoutException {
        final TestBrowsingHistoryObserver historyObserver = new TestBrowsingHistoryObserver();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Profile profile =
                            (tab == null)
                                    ? ProfileManager.getLastUsedRegularProfile()
                                    : tab.getProfile();
                    BrowsingHistoryBridge historyService = new BrowsingHistoryBridge(profile);
                    historyService.setObserver(historyObserver);
                    String historyQueryFilter = "";
                    historyService.queryHistory(historyQueryFilter, null);
                });
        historyObserver.getQueryCallback().waitForCallback(0);
        return historyObserver.getHistoryQueryResults();
    }

    /**
     * A general class providing test parameters encapsulating different Activity type pairs spliced
     * on Regular and Incognito mode between whom we want to test leakage.
     */
    public static class AllTypesToAllTypes implements ParameterProvider {
        @Override
        public List<ParameterSet> getParameters() {
            List<ParameterSet> result = new ArrayList<>();
            result.addAll(new TestParams.IncognitoToIncognito().getParameters());
            result.addAll(new TestParams.IncognitoToRegular().getParameters());
            result.addAll(new TestParams.RegularToIncognito().getParameters());
            return result;
        }
    }

    @Test
    @LargeTest
    public void testBrowsingHistoryDoNotLeakFromIncognitoTabbedActivity() throws TimeoutException {
        mChromeActivityTestRule.startMainActivityOnBlankPage();
        mChromeActivityTestRule.loadUrlInNewTab(mTestPage1, /* incognito= */ true);
        List<HistoryItem> historyEntriesOfIncognitoMode =
                getBrowsingHistory(mChromeActivityTestRule.getActivity().getActivityTab());
        assertTrue(historyEntriesOfIncognitoMode.isEmpty());
    }

    @Test
    @LargeTest
    public void testBrowsingHistoryDoNotLeakFromIncognitoCustomTabActivity()
            throws TimeoutException {
        Intent intent =
                CustomTabsIntentTestUtils.createMinimalIncognitoCustomTabIntent(
                        ApplicationProvider.getApplicationContext(), mTestPage1);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        List<HistoryItem> historyEntriesOfIncognitoMode =
                getBrowsingHistory(mCustomTabActivityTestRule.getActivity().getActivityTab());
        assertTrue(historyEntriesOfIncognitoMode.isEmpty());
    }

    @Test
    @LargeTest
    @UseMethodParameter(AllTypesToAllTypes.class)
    public void testTabNavigationHistoryDoNotLeakBetweenActivities(
            String activityType1, String activityType2) throws TimeoutException {
        ActivityType activity1 = ActivityType.valueOf(activityType1);
        ActivityType activity2 = ActivityType.valueOf(activityType2);

        Tab tab1 =
                activity1.launchUrl(
                        mChromeActivityTestRule, mCustomTabActivityTestRule, mTestPage1);
        CriteriaHelper.pollUiThread(
                () -> Criteria.checkThat(tab1.getWebContents(), Matchers.notNullValue()));
        NavigationHistory navigationHistory1 =
                tab1.getWebContents().getNavigationController().getNavigationHistory();

        Tab tab2 =
                activity2.launchUrl(
                        mChromeActivityTestRule, mCustomTabActivityTestRule, mTestPage2);
        CriteriaHelper.pollUiThread(
                () -> Criteria.checkThat(tab2.getWebContents(), Matchers.notNullValue()));
        NavigationHistory navigationHistory2 =
                tab2.getWebContents().getNavigationController().getNavigationHistory();

        assertEquals(1, navigationHistory1.getEntryCount());
        assertEquals(1, navigationHistory2.getEntryCount());

        NavigationEntry entry1 = navigationHistory1.getEntryAtIndex(0);
        NavigationEntry entry2 = navigationHistory2.getEntryAtIndex(0);

        assertEquals(mTestPage1, entry1.getOriginalUrl().getSpec());
        assertEquals(mTestPage2, entry2.getOriginalUrl().getSpec());
    }
}
