// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.privacy;

import android.content.Context;
import android.content.SharedPreferences;
import android.support.test.InstrumentationRegistry;
import android.support.test.annotation.UiThreadTest;
import android.support.test.filters.SmallTest;
import android.support.test.rule.UiThreadTestRule;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.AdvancedMockContext;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

/**
 *  Tests "Usage and Crash reporting" settings screen.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class PrivacyPreferencesManagerNativeTest {
    @Rule
    public final RuleChain mChain =
            RuleChain.outerRule(new ChromeBrowserTestRule()).around(new UiThreadTestRule());

    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"Android-AppBase"})
    public void testSyncUsageAndCrashReporting() {
        PermissionContext context =
                new PermissionContext(InstrumentationRegistry.getTargetContext());
        PrefServiceBridge prefBridge = PrefServiceBridge.getInstance();
        SharedPreferences pref = ContextUtils.getAppSharedPreferences();
        PrivacyPreferencesManager preferenceManager = new PrivacyPreferencesManager(context);

        // Setup prefs to be out of sync.
        PrivacyPreferencesManager.getInstance().setMetricsReportingEnabled(false);
        pref.edit().putBoolean(PrivacyPreferencesManager.PREF_METRICS_REPORTING, true).apply();

        preferenceManager.syncUsageAndCrashReportingPrefs();
        Assert.assertTrue("Native preference should be True ",
                PrivacyPreferencesManager.getInstance().isMetricsReportingEnabled());
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    @UiThreadTest
    @DisabledTest(message = "crbug.com/700500")
    public void testSetUsageAndCrashReporting() {
        PermissionContext context =
                new PermissionContext(InstrumentationRegistry.getTargetContext());
        PrefServiceBridge prefBridge = PrefServiceBridge.getInstance();
        SharedPreferences pref = ContextUtils.getAppSharedPreferences();
        PrivacyPreferencesManager preferenceManager = new PrivacyPreferencesManager(context);

        preferenceManager.setUsageAndCrashReporting(true);
        Assert.assertTrue(pref.getBoolean(PrivacyPreferencesManager.PREF_METRICS_REPORTING, false));
        Assert.assertTrue("Native preference should be True ",
                PrivacyPreferencesManager.getInstance().isMetricsReportingEnabled());

        preferenceManager.setUsageAndCrashReporting(false);
        Assert.assertFalse(
                pref.getBoolean(PrivacyPreferencesManager.PREF_METRICS_REPORTING, false));
        Assert.assertFalse("Native preference should be False ",
                PrivacyPreferencesManager.getInstance().isMetricsReportingEnabled());
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
