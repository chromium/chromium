// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy.settings;

import android.content.Context;
import android.content.SharedPreferences;

import androidx.test.annotation.UiThreadTest;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.AdvancedMockContext;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.metrics.MetricsReportingLevel;

/** Tests "Usage and Crash reporting" settings screen. */
@RunWith(ChromeJUnit4ClassRunner.class)
public class PrivacyPreferencesManagerImplNativeTest {
    @Rule public final ChromeBrowserTestRule mChromeBrowserTestRule = new ChromeBrowserTestRule();

    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"Android-AppBase"})
    public void testSetMetricsReportingLevel() {
        PermissionContext context =
                new PermissionContext(ApplicationProvider.getApplicationContext());
        PrivacyPreferencesManagerImpl preferenceManager =
                new PrivacyPreferencesManagerImpl(context);

        // This test assumes not enforced by policy, which is true for a fresh test environment.
        preferenceManager.setMetricsReportingLevel(MetricsReportingLevel.BASIC);
        Assert.assertEquals(
                MetricsReportingLevel.BASIC,
                ChromeSharedPreferences.getInstance()
                        .readInt(ChromePreferenceKeys.PRIVACY_METRICS_REPORTING_LEVEL, -1));

        preferenceManager.setMetricsReportingLevel(MetricsReportingLevel.ADVANCED);
        Assert.assertEquals(
                MetricsReportingLevel.ADVANCED,
                ChromeSharedPreferences.getInstance()
                        .readInt(ChromePreferenceKeys.PRIVACY_METRICS_REPORTING_LEVEL, -1));
    }

    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"Android-AppBase"})
    public void testIsMetricsReportingDisabledByPolicy() {
        PermissionContext context =
                new PermissionContext(ApplicationProvider.getApplicationContext());
        PrivacyPreferencesManagerImpl preferenceManager =
                new PrivacyPreferencesManagerImpl(context);

        // Fresh test environment should have no policy enforcement.
        Assert.assertFalse(
                ChromeSharedPreferences.getInstance()
                        .readBoolean(
                                ChromePreferenceKeys.PRIVACY_METRICS_REPORTING_DISABLED_BY_POLICY,
                                false));
    }

    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"Android-AppBase"})
    public void testSyncMetricsReportingDisabledByPolicy() {
        PermissionContext context =
                new PermissionContext(ApplicationProvider.getApplicationContext());
        PrivacyPreferencesManagerImpl preferenceManager =
                new PrivacyPreferencesManagerImpl(context);

        // Ensure native is initialized for the manager.
        preferenceManager.onNativeInitialized();

        // Clear the Java-side preference to ensure it gets re-synced.
        ChromeSharedPreferences.getInstance()
                .removeKey(ChromePreferenceKeys.PRIVACY_METRICS_REPORTING_DISABLED_BY_POLICY);

        preferenceManager.syncMetricsReportingDisabledByPolicy();

        // Since it's a fresh environment, it should be false (not enforced).
        Assert.assertFalse(
                ChromeSharedPreferences.getInstance()
                        .readBoolean(
                                ChromePreferenceKeys.PRIVACY_METRICS_REPORTING_DISABLED_BY_POLICY,
                                true));
    }

    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"Android-AppBase"})
    public void testSyncUsageAndCrashReporting() {
        PermissionContext context =
                new PermissionContext(ApplicationProvider.getApplicationContext());
        SharedPreferences pref = ContextUtils.getAppSharedPreferences();
        PrivacyPreferencesManagerImpl preferenceManager =
                new PrivacyPreferencesManagerImpl(context);

        // Setup prefs to be out of sync.
        PrivacyPreferencesManagerImpl.getInstance().setMetricsReportingEnabled(false);
        pref.edit()
                .putBoolean(ChromePreferenceKeys.PRIVACY_METRICS_REPORTING_PERMITTED_BY_USER, true)
                .apply();

        preferenceManager.syncUsageAndCrashReportingPrefs();
        Assert.assertTrue(
                "Native preference should be True ",
                PrivacyPreferencesManagerImpl.getInstance().isMetricsReportingEnabled());
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    @UiThreadTest
    public void testSetUsageAndCrashReporting() {
        PermissionContext context =
                new PermissionContext(ApplicationProvider.getApplicationContext());
        SharedPreferences pref = ContextUtils.getAppSharedPreferences();
        PrivacyPreferencesManagerImpl preferenceManager =
                new PrivacyPreferencesManagerImpl(context);

        preferenceManager.setUsageAndCrashReporting(true);
        Assert.assertTrue(
                pref.getBoolean(
                        ChromePreferenceKeys.PRIVACY_METRICS_REPORTING_PERMITTED_BY_USER, false));
        Assert.assertTrue(
                "Native preference should be True ",
                PrivacyPreferencesManagerImpl.getInstance().isMetricsReportingEnabled());

        preferenceManager.setUsageAndCrashReporting(false);
        Assert.assertFalse(
                pref.getBoolean(
                        ChromePreferenceKeys.PRIVACY_METRICS_REPORTING_PERMITTED_BY_USER, false));
        Assert.assertFalse(
                "Native preference should be False ",
                PrivacyPreferencesManagerImpl.getInstance().isMetricsReportingEnabled());
    }

    private static class PermissionContext extends AdvancedMockContext {
        public PermissionContext(Context targetContext) {
            super(targetContext);
        }

        @Override
        public Object getSystemService(String name) {
            if (Context.CONNECTIVITY_SERVICE.equals(name)) {
                return null;
            }
            Assert.fail("Should not ask for any other service than the ConnectionManager.");
            return super.getSystemService(name);
        }
    }
}
