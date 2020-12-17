// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.v2;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManager;
import org.chromium.chrome.test.util.browser.Features;

/**
 * Tests for FeedProcessScopeDependencyProvider.
 */
@RunWith(BaseRobolectricTestRunner.class)
public final class FeedProcessScopeDependencyProviderTest {
    @Rule
    public TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();

    FeedProcessScopeDependencyProvider mProvider;
    boolean mMetricsReportingEnabled;

    private PrivacyPreferencesManager mStubPrivacyPrefsManager = new PrivacyPreferencesManager() {
        @Override
        public boolean isMetricsReportingEnabled() {
            return mMetricsReportingEnabled;
        }

        // Boilerplate.
        @Override
        public boolean shouldPrerender() {
            return false;
        }
        @Override
        public void setUsageAndCrashReporting(boolean enabled) {}
        @Override
        public void syncUsageAndCrashReportingPrefs() {}
        @Override
        public void setClientInMetricsSample(boolean inSample) {}
        @Override
        public boolean isClientInMetricsSample() {
            return true;
        }
        @Override
        public boolean isNetworkAvailableForCrashUploads() {
            return true;
        }
        @Override
        public boolean isUsageAndCrashReportingPermittedByUser() {
            return true;
        }
        @Override
        public boolean isUploadEnabledForTests() {
            return true;
        }
        @Override
        public boolean isMetricsUploadPermitted() {
            return false;
        }
        @Override
        public void setMetricsReportingEnabled(boolean enabled) {}
        @Override
        public boolean isMetricsReportingManaged() {
            return false;
        }
        @Override
        public boolean getNetworkPredictionEnabled() {
            return false;
        }
        @Override
        public void setNetworkPredictionEnabled(boolean enabled) {}
        @Override
        public boolean isNetworkPredictionManaged() {
            return false;
        }
    };

    @Before
    public void setUp() {
        FeedProcessScopeDependencyProvider.sPrivacyPreferencesManagerForTest =
                mStubPrivacyPrefsManager;
        mProvider = new FeedProcessScopeDependencyProvider();
    }

    @Test
    @Features.DisableFeatures({ChromeFeatureList.XSURFACE_METRICS_REPORTING})
    public void usageAndCrashReporting_featureDisabled() {
        mMetricsReportingEnabled = false;
        assertFalse(mProvider.isXsurfaceUsageAndCrashReportingEnabled());

        mMetricsReportingEnabled = true;
        assertFalse(mProvider.isXsurfaceUsageAndCrashReportingEnabled());
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.XSURFACE_METRICS_REPORTING})
    public void usageAndCrashReporting_featureEnabled() {
        mMetricsReportingEnabled = false;
        assertFalse(mProvider.isXsurfaceUsageAndCrashReportingEnabled());

        mMetricsReportingEnabled = true;
        assertTrue(mProvider.isXsurfaceUsageAndCrashReportingEnabled());
    }
}
