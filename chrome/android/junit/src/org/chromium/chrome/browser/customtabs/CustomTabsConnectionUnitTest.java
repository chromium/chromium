// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.content.Intent;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.metrics.UmaSessionStats;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;

import java.util.ArrayList;
import java.util.List;

/** Tests for some parts of {@link CustomTabsConnection}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {CustomTabsConnectionUnitTest.ShadowUmaSessionStats.class})
public class CustomTabsConnectionUnitTest {
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    private static final ArrayList<String> REALTIME_SIGNALS_AND_BRANDING =
            new ArrayList<String>(List.of(ChromeFeatureList.CCT_REAL_TIME_ENGAGEMENT_SIGNALS,
                    ChromeFeatureList.CCT_BRAND_TRANSPARENCY));
    private CustomTabsConnection mConnection;

    @Implements(UmaSessionStats.class)
    public static class ShadowUmaSessionStats {
        public ShadowUmaSessionStats() {}

        @Implementation
        public static boolean isMetricsServiceAvailable() {
            return false;
        }

        @Implementation
        public static void registerSyntheticFieldTrial(String trialName, String groupName) {}
    }

    @Before
    public void setup() {
        mConnection = CustomTabsConnection.getInstance();
        mConnection.setIsDynamicFeaturesEnabled(true);
    }

    @After
    public void tearDown() {
        CustomTabsConnection.setInstanceForTesting(null);
    }

    @Test
    @DisableFeatures({ChromeFeatureList.CCT_BRAND_TRANSPARENCY,
            ChromeFeatureList.CCT_REAL_TIME_ENGAGEMENT_SIGNALS})
    public void resetDynamicFeatures() {
        Intent intent = new Intent().putStringArrayListExtra(
                CustomTabIntentDataProvider.EXPERIMENTS_DISABLE, REALTIME_SIGNALS_AND_BRANDING);
        mConnection.setupDynamicFeaturesInternal(intent);
        mConnection.resetDynamicFeatures();
        assertFalse(mConnection.isDynamicFeatureEnabled(ChromeFeatureList.CCT_BRAND_TRANSPARENCY));
        assertFalse(mConnection.isDynamicFeatureEnabled(
                ChromeFeatureList.CCT_REAL_TIME_ENGAGEMENT_SIGNALS));
    }

    @Test
    @DisableFeatures({ChromeFeatureList.CCT_BRAND_TRANSPARENCY,
            ChromeFeatureList.CCT_REAL_TIME_ENGAGEMENT_SIGNALS})
    public void agaEnableCase() {
        Intent intent = new Intent().putStringArrayListExtra(
                CustomTabIntentDataProvider.EXPERIMENTS_ENABLE, REALTIME_SIGNALS_AND_BRANDING);
        mConnection.resetDynamicFeatures();
        assertTrue(mConnection.setupDynamicFeaturesInternal(intent));
        assertTrue(mConnection.isDynamicFeatureEnabled(REALTIME_SIGNALS_AND_BRANDING.get(0)));
        assertTrue(mConnection.isDynamicFeatureEnabled(REALTIME_SIGNALS_AND_BRANDING.get(1)));
    }

    @Test
    @DisableFeatures({ChromeFeatureList.CCT_BRAND_TRANSPARENCY,
            ChromeFeatureList.CCT_REAL_TIME_ENGAGEMENT_SIGNALS})
    public void canOverrideByEnabling() {
        assertFalse(ChromeFeatureList.isEnabled(REALTIME_SIGNALS_AND_BRANDING.get(0)));
        assertFalse(ChromeFeatureList.isEnabled(REALTIME_SIGNALS_AND_BRANDING.get(1)));
        assertFalse(mConnection.isDynamicFeatureEnabled(REALTIME_SIGNALS_AND_BRANDING.get(0)));
        assertFalse(mConnection.isDynamicFeatureEnabled(REALTIME_SIGNALS_AND_BRANDING.get(1)));
        Intent intent = new Intent().putStringArrayListExtra(
                CustomTabIntentDataProvider.EXPERIMENTS_ENABLE, REALTIME_SIGNALS_AND_BRANDING);
        assertTrue(mConnection.setupDynamicFeaturesInternal(intent));
        assertTrue(mConnection.isDynamicFeatureEnabled(REALTIME_SIGNALS_AND_BRANDING.get(0)));
        assertTrue(mConnection.isDynamicFeatureEnabled(REALTIME_SIGNALS_AND_BRANDING.get(1)));
    }

    @Test
    @EnableFeatures({ChromeFeatureList.CCT_BRAND_TRANSPARENCY,
            ChromeFeatureList.CCT_REAL_TIME_ENGAGEMENT_SIGNALS})
    public void canOverrideByDisabling() {
        assertTrue(ChromeFeatureList.isEnabled(REALTIME_SIGNALS_AND_BRANDING.get(0)));
        assertTrue(ChromeFeatureList.isEnabled(REALTIME_SIGNALS_AND_BRANDING.get(1)));
        assertTrue(mConnection.isDynamicFeatureEnabled(REALTIME_SIGNALS_AND_BRANDING.get(0)));
        assertTrue(mConnection.isDynamicFeatureEnabled(REALTIME_SIGNALS_AND_BRANDING.get(1)));
        Intent intent = new Intent().putStringArrayListExtra(
                CustomTabIntentDataProvider.EXPERIMENTS_DISABLE, REALTIME_SIGNALS_AND_BRANDING);
        mConnection.resetDynamicFeatures();
        assertTrue(mConnection.setupDynamicFeaturesInternal(intent));
        assertFalse(mConnection.isDynamicFeatureEnabled(REALTIME_SIGNALS_AND_BRANDING.get(0)));
        assertFalse(mConnection.isDynamicFeatureEnabled(REALTIME_SIGNALS_AND_BRANDING.get(1)));
    }

    @Test
    @DisableFeatures({ChromeFeatureList.CCT_BRAND_TRANSPARENCY,
            ChromeFeatureList.CCT_REAL_TIME_ENGAGEMENT_SIGNALS})
    public void setIsDynamicFeaturesEnabled() {
        mConnection.setIsDynamicFeaturesEnabled(false);
        // Same pattern as #canOverrideByEnabling, except now we've turned off that ability.
        assertFalse(ChromeFeatureList.isEnabled(REALTIME_SIGNALS_AND_BRANDING.get(0)));
        assertFalse(ChromeFeatureList.isEnabled(REALTIME_SIGNALS_AND_BRANDING.get(1)));
        assertFalse(mConnection.isDynamicFeatureEnabled(REALTIME_SIGNALS_AND_BRANDING.get(0)));
        assertFalse(mConnection.isDynamicFeatureEnabled(REALTIME_SIGNALS_AND_BRANDING.get(1)));
        Intent intent = new Intent().putStringArrayListExtra(
                CustomTabIntentDataProvider.EXPERIMENTS_ENABLE, REALTIME_SIGNALS_AND_BRANDING);
        mConnection.resetDynamicFeatures();
        assertTrue(mConnection.setupDynamicFeaturesInternal(intent));
        assertFalse(mConnection.isDynamicFeatureEnabled(REALTIME_SIGNALS_AND_BRANDING.get(0)));
        assertFalse(mConnection.isDynamicFeatureEnabled(REALTIME_SIGNALS_AND_BRANDING.get(1)));
    }

    @Test
    @DisableFeatures({ChromeFeatureList.CCT_BRAND_TRANSPARENCY,
            ChromeFeatureList.CCT_REAL_TIME_ENGAGEMENT_SIGNALS,
            ChromeFeatureList.CCT_INTENT_FEATURE_OVERRIDES})
    public void setIsDynamicFeaturesEnabled_FalseDisablesOverrides() {
        mConnection.setIsDynamicFeaturesEnabled(false);
        assertFalse(mConnection.setupDynamicFeatures(null));
        assertFalse(mConnection.setupDynamicFeatures(new Intent()));
        assertFalse(mConnection.setupDynamicFeatures(new Intent().putStringArrayListExtra(
                CustomTabIntentDataProvider.EXPERIMENTS_ENABLE, REALTIME_SIGNALS_AND_BRANDING)));
        assertFalse(mConnection.isDynamicFeatureEnabled(REALTIME_SIGNALS_AND_BRANDING.get(0)));
        assertFalse(mConnection.isDynamicFeatureEnabled(REALTIME_SIGNALS_AND_BRANDING.get(1)));
    }

    @Test
    @EnableFeatures({ChromeFeatureList.CCT_BRAND_TRANSPARENCY,
            ChromeFeatureList.CCT_REAL_TIME_ENGAGEMENT_SIGNALS})
    public void featuresEnabledWithoutOverrides() {
        assertTrue(mConnection.isDynamicFeatureEnabled(REALTIME_SIGNALS_AND_BRANDING.get(0)));
        assertTrue(mConnection.isDynamicFeatureEnabled(REALTIME_SIGNALS_AND_BRANDING.get(1)));
    }

    @Test
    @DisableFeatures({ChromeFeatureList.CCT_BRAND_TRANSPARENCY,
            ChromeFeatureList.CCT_REAL_TIME_ENGAGEMENT_SIGNALS})
    public void featuresDisabledWithoutOverrides() {
        assertFalse(mConnection.isDynamicFeatureEnabled(REALTIME_SIGNALS_AND_BRANDING.get(0)));
        assertFalse(mConnection.isDynamicFeatureEnabled(REALTIME_SIGNALS_AND_BRANDING.get(1)));
    }

    @Test
    public void areExperimentsSupported_NullInputs() {
        assertFalse(mConnection.areExperimentsSupported(null, null));
    }

    @Test
    public void areExperimentsSupported_AgaInputs() {
        assertTrue(mConnection.areExperimentsSupported(REALTIME_SIGNALS_AND_BRANDING, null));
        assertTrue(mConnection.areExperimentsSupported(null, REALTIME_SIGNALS_AND_BRANDING));
    }

    // TODO(https://crrev.com/c/4118209) Add more tests for Feature enabling/disabling.
}
