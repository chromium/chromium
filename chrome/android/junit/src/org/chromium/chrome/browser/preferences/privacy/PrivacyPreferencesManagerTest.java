// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.privacy;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

/**
 * junit tests for {@link PrivacyPreferencesManager}'s handling of "Usage and Crash reporting"
 * preferences.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PrivacyPreferencesManagerTest {
    // Parameters to simulate user- and network-permission state.
    private static final boolean CONNECTED = true;
    private static final boolean DISCONNECTED = false;

    private static final boolean METERED = true;
    private static final boolean UNMETERED = false;

    private static final boolean METRICS_REPORTING_ENABLED = true;
    private static final boolean METRICS_REPORTING_DISABLED = false;

    // Used to set test expectations.
    private static final boolean METRICS_UPLOAD_PERMITTED = true;
    private static final boolean METRICS_UPLOAD_NOT_PERMITTED = false;

    private static final boolean CRASH_NETWORK_AVAILABLE = true;
    private static final boolean CRASH_NETWORK_UNAVAILABLE = false;

    @Test
    public void testUsageAndCrashReportingAccessors() {
        // TODO(yolandyan): Use Junit4 parameters to clean up this test structure.
        runTest(CONNECTED, UNMETERED, METRICS_REPORTING_ENABLED, METRICS_UPLOAD_PERMITTED,
                CRASH_NETWORK_AVAILABLE);
        runTest(CONNECTED, METERED, METRICS_REPORTING_ENABLED, METRICS_UPLOAD_PERMITTED,
                CRASH_NETWORK_UNAVAILABLE);
        runTest(DISCONNECTED, UNMETERED, METRICS_REPORTING_ENABLED, METRICS_UPLOAD_NOT_PERMITTED,
                CRASH_NETWORK_UNAVAILABLE);

        runTest(CONNECTED, UNMETERED, METRICS_REPORTING_DISABLED, METRICS_UPLOAD_NOT_PERMITTED,
                CRASH_NETWORK_AVAILABLE);
        runTest(CONNECTED, METERED, METRICS_REPORTING_DISABLED, METRICS_UPLOAD_NOT_PERMITTED,
                CRASH_NETWORK_UNAVAILABLE);
        runTest(DISCONNECTED, UNMETERED, METRICS_REPORTING_DISABLED, METRICS_UPLOAD_NOT_PERMITTED,
                CRASH_NETWORK_UNAVAILABLE);
    }

    private void runTest(boolean isConnected, boolean isMetered, boolean isMetricsReportingEnabled,
            boolean expectedMetricsUploadPermitted,
            boolean expectedNetworkAvailableForCrashUploads) {
        // Mock out the network info accessors.
        NetworkInfo networkInfo = mock(NetworkInfo.class);
        when(networkInfo.isConnected()).thenReturn(isConnected);

        ConnectivityManager connectivityManager = mock(ConnectivityManager.class);
        when(connectivityManager.getActiveNetworkInfo()).thenReturn(networkInfo);
        when(connectivityManager.isActiveNetworkMetered()).thenReturn(isMetered);

        Context context = mock(Context.class);
        when(context.getSystemService(Context.CONNECTIVITY_SERVICE))
                .thenReturn(connectivityManager);

        // Perform other setup.
        PrivacyPreferencesManager preferenceManager = new TestPrivacyPreferencesManager(context);
        preferenceManager.setUsageAndCrashReporting(isMetricsReportingEnabled);

        // Test the usage and crash reporting permission accessors!
        String state = String.format("[connected = %b, metered = %b, reporting = %b]", isConnected,
                isMetered, isMetricsReportingEnabled);
        String msg = String.format(
                "Metrics reporting should be %1$b for %2$s", expectedMetricsUploadPermitted, state);
        assertEquals(
                msg, expectedMetricsUploadPermitted, preferenceManager.isMetricsUploadPermitted());

        msg = String.format("Crash reporting should be %1$b for metered state %2$s",
                expectedNetworkAvailableForCrashUploads, isMetered);
        assertEquals(msg, expectedNetworkAvailableForCrashUploads,
                preferenceManager.isNetworkAvailableForCrashUploads());
    }

    private static class TestPrivacyPreferencesManager extends PrivacyPreferencesManager {
        TestPrivacyPreferencesManager(Context context) {
            super(context);
        }

        // Stub out this call, as it could otherwise call into native code.
        @Override
        public void syncUsageAndCrashReportingPrefs() {}
    }
}
