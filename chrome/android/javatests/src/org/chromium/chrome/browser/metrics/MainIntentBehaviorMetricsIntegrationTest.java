// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.metrics;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertThat;

import android.annotation.SuppressLint;
import android.content.ComponentName;
import android.content.Intent;
import android.net.Uri;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.bookmarks.BookmarkActivity;
import org.chromium.chrome.browser.download.DownloadActivity;
import org.chromium.chrome.browser.history.HistoryActivity;
import org.chromium.chrome.browser.omnibox.UrlBar;
import org.chromium.chrome.browser.preferences.Preferences;
import org.chromium.chrome.browser.tabmodel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ActivityUtils;
import org.chromium.chrome.test.util.MenuUtils;
import org.chromium.chrome.test.util.OmniboxTestUtils;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.common.ContentUrlConstants;

import java.util.concurrent.Callable;

/**
 * Tests the metrics recording for main intent behaviours.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@SuppressLint({"ApplySharedPref", "CommitPrefEdits"})
public class MainIntentBehaviorMetricsIntegrationTest {
    private static final long HOURS_IN_MS = 60 * 60 * 1000L;

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private UserActionTester mActionTester;

    @Before
    public void setUp() {
        MainIntentBehaviorMetrics.setShouldTrackBehaviorSourceForTesting(true);
    }

    @After
    public void tearDown() {
        MainIntentBehaviorMetrics.setShouldTrackBehaviorSourceForTesting(false);
        if (mActionTester != null) mActionTester.tearDown();
    }

    @MediumTest
    @Test
    public void testFocusOmnibox() {
        startActivity(true);
        assertMainIntentBehavior(null);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            UrlBar urlBar = (UrlBar) mActivityTestRule.getActivity().findViewById(R.id.url_bar);
            OmniboxTestUtils.toggleUrlBarFocus(urlBar, true);
        });
        assertMainIntentBehavior(MainIntentBehaviorMetrics.MainIntentActionType.FOCUS_OMNIBOX);
    }

    @MediumTest
    @Test
    @DisabledTest(message = "crbug.com/972759")
    public void testSwitchTabs() {
        startActivity(true);
        assertMainIntentBehavior(null);
        TestThreadUtils.runOnUiThreadBlocking(
            (Runnable) () -> mActivityTestRule.getActivity().getTabCreator(false).createNewTab(
                                new LoadUrlParams(ContentUrlConstants.ABOUT_BLANK_URL),
                                TabLaunchType.FROM_RESTORE, null));
        CriteriaHelper.pollUiThread(Criteria.equals(2, new Callable<Integer>() {
            @Override
            public Integer call() {
                return mActivityTestRule.getActivity().getTabModelSelector().getTotalTabCount();
            }
        }));
        assertMainIntentBehavior(null);

        TestThreadUtils.runOnUiThreadBlocking(
                () -> TabModelUtils.setIndex(
                                mActivityTestRule.getActivity().getCurrentTabModel(), 1));
        assertMainIntentBehavior(MainIntentBehaviorMetrics.MainIntentActionType.SWITCH_TABS);
    }

    @MediumTest
    @Test
    public void testBackgrounded() {
        startActivity(true);
        assertMainIntentBehavior(null);
        TestThreadUtils.runOnUiThreadBlocking(() -> mActivityTestRule.getActivity().finish());
        assertMainIntentBehavior(MainIntentBehaviorMetrics.MainIntentActionType.BACKGROUNDED);
    }

    @MediumTest
    @Test
    public void testCreateNtp() {
        // startActivity(true) creates a NTP which is problematical for this test if
        // ChromeTabbedActivity.setupCompositorContent runs before that NTP is created because
        // that creates a SimpleAnimationLayout which tries to hide the page resulting in a
        // MainIntentActionType.SWITCH_TABS. Starting from about:blank avoids this confusion.
        startActivityWithAboutBlank(true);
        assertMainIntentBehavior(null);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mActivityTestRule.getActivity().getTabCreator(false).launchNTP());
        assertMainIntentBehavior(MainIntentBehaviorMetrics.MainIntentActionType.NTP_CREATED);
    }

    @MediumTest
    @Test
    public void testContinuation() {
        try {
            MainIntentBehaviorMetrics.setTimeoutDurationMsForTesting(500);
            startActivity(true);
            assertMainIntentBehavior(MainIntentBehaviorMetrics.MainIntentActionType.CONTINUATION);
        } finally {
            MainIntentBehaviorMetrics.setTimeoutDurationMsForTesting(
                    MainIntentBehaviorMetrics.TIMEOUT_DURATION_MS);
        }
    }

    @MediumTest
    @Test
    public void testMainIntentWithoutLauncherCategory() {
        startActivity(false);
        assertMainIntentBehavior(null);
        Assert.assertFalse(mActivityTestRule.getActivity().getMainIntentBehaviorMetricsForTesting()
                .getPendingActionRecordForMainIntent());
    }

    @MediumTest
    @Test
    public void testBackgroundDuration_24hrs() {
        assertBackgroundDurationLogged(
                24 * HOURS_IN_MS, "MobileStartup.MainIntentReceived.After24Hours");
    }

    @MediumTest
    @Test
    public void testBackgroundDuration_12hrs() {
        assertBackgroundDurationLogged(
                12 * HOURS_IN_MS, "MobileStartup.MainIntentReceived.After12Hours");
    }

    @MediumTest
    @Test
    public void testBackgroundDuration_6hrs() {
        assertBackgroundDurationLogged(
                6 * HOURS_IN_MS, "MobileStartup.MainIntentReceived.After6Hours");
    }

    @MediumTest
    @Test
    public void testBackgroundDuration_1hr() {
        assertBackgroundDurationLogged(HOURS_IN_MS, "MobileStartup.MainIntentReceived.After1Hour");
    }

    @MediumTest
    @Test
    public void testBackgroundDuration_0hr() {
        assertBackgroundDurationLogged(0, null);
        for (String action : mActionTester.getActions()) {
            if (action.startsWith("MobileStartup.MainIntentReceived.After")) {
                Assert.fail("Unexpected background duration logged: " + action);
            }
        }
    }

    @MediumTest
    @Test
    public void testLaunch_Duration_MoreThan_1Day() {
        long timestamp = System.currentTimeMillis() - 25 * HOURS_IN_MS;
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putLong(MainIntentBehaviorMetrics.LAUNCH_TIMESTAMP_PREF, timestamp)
                .commit();
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putInt(MainIntentBehaviorMetrics.LAUNCH_COUNT_PREF, 10)
                .commit();
        mActivityTestRule.startMainActivityFromLauncher();

        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "MobileStartup.DailyLaunchCount", 10));

        assertEquals(1,
                ContextUtils.getAppSharedPreferences().getInt(
                        MainIntentBehaviorMetrics.LAUNCH_COUNT_PREF, 0));

        long newTimestamp = ContextUtils.getAppSharedPreferences().getLong(
                MainIntentBehaviorMetrics.LAUNCH_TIMESTAMP_PREF, 0);
        assertNotEquals(timestamp, newTimestamp);
        assertNotEquals(0, newTimestamp);
    }

    @MediumTest
    @Test
    public void testLaunch_Duration_LessThan_1Day() {
        long timestamp = System.currentTimeMillis() - 12 * HOURS_IN_MS;
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putLong(MainIntentBehaviorMetrics.LAUNCH_TIMESTAMP_PREF, timestamp)
                .commit();
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putInt(MainIntentBehaviorMetrics.LAUNCH_COUNT_PREF, 1)
                .commit();
        mActivityTestRule.startMainActivityFromLauncher();

        assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(
                        "MobileStartup.DailyLaunchCount", 1));

        assertEquals(2,
                ContextUtils.getAppSharedPreferences().getInt(
                        MainIntentBehaviorMetrics.LAUNCH_COUNT_PREF, 0));

        assertEquals(timestamp,
                ContextUtils.getAppSharedPreferences().getLong(
                        MainIntentBehaviorMetrics.LAUNCH_TIMESTAMP_PREF, 0));
    }

    @MediumTest
    @DisabledTest(message = "crbug.com/879165")
    @Test
    public void testLaunch_From_InAppActivities() {
        try {
            MainIntentBehaviorMetrics.setTimeoutDurationMsForTesting(0);
            long timestamp = System.currentTimeMillis() - 12 * HOURS_IN_MS;
            ContextUtils.getAppSharedPreferences()
                    .edit()
                    .putLong(MainIntentBehaviorMetrics.LAUNCH_TIMESTAMP_PREF, timestamp)
                    .commit();

            mActivityTestRule.startMainActivityFromLauncher();

            Preferences preferences = mActivityTestRule.startPreferences(null);
            preferences.finish();
            ChromeActivityTestRule.waitForActivityNativeInitializationComplete(
                    ChromeActivityTestRule.waitFor(ChromeTabbedActivity.class));

            BookmarkActivity bookmarkActivity = ActivityUtils.waitForActivity(
                    InstrumentationRegistry.getInstrumentation(), BookmarkActivity.class,
                    new MenuUtils.MenuActivityTrigger(InstrumentationRegistry.getInstrumentation(),
                            mActivityTestRule.getActivity(), R.id.all_bookmarks_menu_id));
            bookmarkActivity.finish();
            ChromeActivityTestRule.waitForActivityNativeInitializationComplete(
                    ChromeActivityTestRule.waitFor(ChromeTabbedActivity.class));

            DownloadActivity downloadActivity = ActivityUtils.waitForActivity(
                    InstrumentationRegistry.getInstrumentation(), DownloadActivity.class,
                    new MenuUtils.MenuActivityTrigger(InstrumentationRegistry.getInstrumentation(),
                            mActivityTestRule.getActivity(), R.id.downloads_menu_id));
            downloadActivity.finish();
            ChromeActivityTestRule.waitForActivityNativeInitializationComplete(
                    ChromeActivityTestRule.waitFor(ChromeTabbedActivity.class));

            HistoryActivity historyActivity = ActivityUtils.waitForActivity(
                    InstrumentationRegistry.getInstrumentation(), HistoryActivity.class,
                    new MenuUtils.MenuActivityTrigger(InstrumentationRegistry.getInstrumentation(),
                            mActivityTestRule.getActivity(), R.id.open_history_menu_id));
            historyActivity.finish();

            assertEquals(1,
                    ContextUtils.getAppSharedPreferences().getInt(
                            MainIntentBehaviorMetrics.LAUNCH_COUNT_PREF, 0));
        } finally {
            MainIntentBehaviorMetrics.setTimeoutDurationMsForTesting(
                    MainIntentBehaviorMetrics.TIMEOUT_DURATION_MS);
        }
    }

    private void assertBackgroundDurationLogged(long duration, String expectedMetric) {
        startActivity(false);
        mActionTester = new UserActionTester();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mActivityTestRule.getActivity()
                    .getInactivityTrackerForTesting()
                    .setLastBackgroundedTimeInPrefs(System.currentTimeMillis() - duration);
        });

        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.addCategory(Intent.CATEGORY_LAUNCHER);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mActivityTestRule.getActivity().onNewIntent(intent); });

        assertThat(mActionTester.toString(), mActionTester.getActions(),
                Matchers.hasItem("MobileStartup.MainIntentReceived"));
        if (expectedMetric != null) {
            assertThat(mActionTester.toString(), mActionTester.getActions(),
                    Matchers.hasItem(expectedMetric));
        }
    }

    private void startActivityWithAboutBlank(boolean addLauncherCategory) {
        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.setData(Uri.parse("about:blank"));
        if (addLauncherCategory) intent.addCategory(Intent.CATEGORY_LAUNCHER);
        intent.setComponent(new ComponentName(
                InstrumentationRegistry.getTargetContext(), ChromeTabbedActivity.class));

        mActivityTestRule.startActivityCompletely(intent);
        mActivityTestRule.waitForActivityNativeInitializationComplete();
    }

    private void startActivity(boolean addLauncherCategory) {
        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        if (addLauncherCategory) intent.addCategory(Intent.CATEGORY_LAUNCHER);
        intent.setComponent(new ComponentName(
                InstrumentationRegistry.getTargetContext(), ChromeTabbedActivity.class));

        mActivityTestRule.startActivityCompletely(intent);
        mActivityTestRule.waitForActivityNativeInitializationComplete();
    }

    private void assertMainIntentBehavior(Integer expected) {
        CriteriaHelper.pollUiThread(Criteria.equals(expected, new Callable<Integer>() {
            @Override
            public Integer call() {
                MainIntentBehaviorMetrics behaviorMetrics =
                        mActivityTestRule.getActivity().getMainIntentBehaviorMetricsForTesting();
                Integer actual = behaviorMetrics.getLastMainIntentBehaviorForTesting();
                if (actual != null && !actual.equals(expected)) {
                    IllegalStateException ex = new IllegalStateException(
                            "Expected main behavior: " + expected + ", actual: " + actual);
                    ex.setStackTrace(behaviorMetrics.getMainIntentBehaviorSourceForTesting());
                    throw ex;
                }
                return actual;
            }
        }));
    }
}
