// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.metrics;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;

import android.annotation.SuppressLint;
import android.content.ComponentName;
import android.content.Intent;
import android.net.Uri;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.app.bookmarks.BookmarkActivity;
import org.chromium.chrome.browser.app.download.home.DownloadActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.history.HistoryActivity;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.chrome.test.util.MenuUtils;
import org.chromium.components.browser_ui.settings.PlaceholderSettingsForTest;
import org.chromium.ui.base.DeviceFormFactor;

/** Tests the metrics recording for main intent behaviours. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@SuppressLint({"ApplySharedPref", "CommitPrefEdits"})
public class MainIntentBehaviorMetricsIntegrationTest {
    private static final long HOURS_IN_MS = 60 * 60 * 1000L;

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public SettingsActivityTestRule<PlaceholderSettingsForTest> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(PlaceholderSettingsForTest.class);

    private UserActionTester mActionTester;

    @After
    public void tearDown() {
        if (mActionTester != null) mActionTester.tearDown();
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
        SharedPreferencesManager prefs = ChromeSharedPreferences.getInstance();
        prefs.writeLongSync(ChromePreferenceKeys.METRICS_MAIN_INTENT_LAUNCH_TIMESTAMP, timestamp);
        prefs.writeIntSync(ChromePreferenceKeys.METRICS_MAIN_INTENT_LAUNCH_COUNT, 10);
        mActivityTestRule.startMainActivityFromLauncher();

        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "MobileStartup.DailyLaunchCount", 10));

        assertEquals(1, prefs.readInt(ChromePreferenceKeys.METRICS_MAIN_INTENT_LAUNCH_COUNT, 0));

        long newTimestamp =
                prefs.readLong(ChromePreferenceKeys.METRICS_MAIN_INTENT_LAUNCH_TIMESTAMP, 0);
        assertNotEquals(timestamp, newTimestamp);
        assertNotEquals(0, newTimestamp);
    }

    @MediumTest
    @Test
    public void testLaunch_Duration_LessThan_1Day() {
        long timestamp = System.currentTimeMillis() - 12 * HOURS_IN_MS;
        SharedPreferencesManager prefs = ChromeSharedPreferences.getInstance();
        prefs.writeLongSync(ChromePreferenceKeys.METRICS_MAIN_INTENT_LAUNCH_TIMESTAMP, timestamp);
        prefs.writeIntSync(ChromePreferenceKeys.METRICS_MAIN_INTENT_LAUNCH_COUNT, 1);
        mActivityTestRule.startMainActivityFromLauncher();

        assertEquals(
                0,
                RecordHistogram.getHistogramValueCountForTesting(
                        "MobileStartup.DailyLaunchCount", 1));

        assertEquals(2, prefs.readInt(ChromePreferenceKeys.METRICS_MAIN_INTENT_LAUNCH_COUNT, 0));

        assertEquals(
                timestamp,
                prefs.readLong(ChromePreferenceKeys.METRICS_MAIN_INTENT_LAUNCH_TIMESTAMP, 0));
    }

    @MediumTest
    @Test
    @DisableIf.Device(DeviceFormFactor.TABLET) // https://crbug.com/338974184
    public void testLaunch_From_InAppActivities() {
        try {
            MainIntentBehaviorMetrics.setTimeoutDurationMsForTesting(0);
            long timestamp = System.currentTimeMillis() - 12 * HOURS_IN_MS;
            SharedPreferencesManager prefs = ChromeSharedPreferences.getInstance();
            prefs.writeLongSync(
                    ChromePreferenceKeys.METRICS_MAIN_INTENT_LAUNCH_TIMESTAMP, timestamp);

            mActivityTestRule.startMainActivityFromLauncher();

            SettingsActivity settingsActivity = mSettingsActivityTestRule.startSettingsActivity();
            settingsActivity.finish();
            ChromeActivityTestRule.waitForActivityNativeInitializationComplete(
                    ChromeActivityTestRule.waitFor(ChromeTabbedActivity.class));

            BookmarkActivity bookmarkActivity =
                    ActivityTestUtils.waitForActivity(
                            InstrumentationRegistry.getInstrumentation(),
                            BookmarkActivity.class,
                            new MenuUtils.MenuActivityTrigger(
                                    InstrumentationRegistry.getInstrumentation(),
                                    mActivityTestRule.getActivity(),
                                    R.id.all_bookmarks_menu_id));
            bookmarkActivity.finish();
            ChromeActivityTestRule.waitForActivityNativeInitializationComplete(
                    ChromeActivityTestRule.waitFor(ChromeTabbedActivity.class));

            DownloadActivity downloadActivity =
                    ActivityTestUtils.waitForActivity(
                            InstrumentationRegistry.getInstrumentation(),
                            DownloadActivity.class,
                            new MenuUtils.MenuActivityTrigger(
                                    InstrumentationRegistry.getInstrumentation(),
                                    mActivityTestRule.getActivity(),
                                    R.id.downloads_menu_id));
            downloadActivity.finish();
            ChromeActivityTestRule.waitForActivityNativeInitializationComplete(
                    ChromeActivityTestRule.waitFor(ChromeTabbedActivity.class));

            HistoryActivity historyActivity =
                    ActivityTestUtils.waitForActivity(
                            InstrumentationRegistry.getInstrumentation(),
                            HistoryActivity.class,
                            new MenuUtils.MenuActivityTrigger(
                                    InstrumentationRegistry.getInstrumentation(),
                                    mActivityTestRule.getActivity(),
                                    R.id.open_history_menu_id));
            historyActivity.finish();

            assertEquals(
                    1, prefs.readInt(ChromePreferenceKeys.METRICS_MAIN_INTENT_LAUNCH_COUNT, 0));
        } finally {
            MainIntentBehaviorMetrics.setTimeoutDurationMsForTesting(
                    MainIntentBehaviorMetrics.TIMEOUT_DURATION_MS);
        }
    }

    private void assertBackgroundDurationLogged(long duration, String expectedMetric) {
        startActivity(false);
        mActionTester = new UserActionTester();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivityTestRule
                            .getActivity()
                            .getInactivityTrackerForTesting()
                            .setLastBackgroundedTimeInPrefs(System.currentTimeMillis() - duration);
                });

        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.addCategory(Intent.CATEGORY_LAUNCHER);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivityTestRule.getActivity().onNewIntent(intent);
                });

        assertThat(
                mActionTester.toString(),
                mActionTester.getActions(),
                Matchers.hasItem("MobileStartup.MainIntentReceived"));
        if (expectedMetric != null) {
            assertThat(
                    mActionTester.toString(),
                    mActionTester.getActions(),
                    Matchers.hasItem(expectedMetric));
        }
    }

    private void startActivityWithAboutBlank(boolean addLauncherCategory) {
        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.setData(Uri.parse("about:blank"));
        if (addLauncherCategory) intent.addCategory(Intent.CATEGORY_LAUNCHER);
        intent.setComponent(
                new ComponentName(
                        ApplicationProvider.getApplicationContext(), ChromeTabbedActivity.class));

        mActivityTestRule.startActivityCompletely(intent);
        mActivityTestRule.waitForActivityNativeInitializationComplete();
    }

    private void startActivity(boolean addLauncherCategory) {
        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        if (addLauncherCategory) intent.addCategory(Intent.CATEGORY_LAUNCHER);
        intent.setComponent(
                new ComponentName(
                        ApplicationProvider.getApplicationContext(), ChromeTabbedActivity.class));

        mActivityTestRule.startActivityCompletely(intent);
        mActivityTestRule.waitForActivityNativeInitializationComplete();
    }
}
