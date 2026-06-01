// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy.settings;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.policy.PolicyServiceFactory;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.components.metrics.MetricsReportingLevel;
import org.chromium.components.policy.PolicyService;

/**
 * junit tests for {@link PrivacyPreferencesManagerImpl}'s handling of "Usage and Crash reporting"
 * preferences.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PrivacyPreferencesManagerImplTest {
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

    private PrivacyPreferencesManagerImpl.Natives mNativeMock;

    @org.junit.Before
    public void setUp() {
        mNativeMock = mock(PrivacyPreferencesManagerImpl.Natives.class);
        PrivacyPreferencesManagerImplJni.setInstanceForTesting(mNativeMock);
    }

    @Test
    public void testUsageAndCrashReportingAccessors() {
        // TODO(yolandyan): Use Junit4 parameters to clean up this test structure.
        runTest(
                CONNECTED,
                UNMETERED,
                METRICS_REPORTING_ENABLED,
                METRICS_UPLOAD_PERMITTED,
                CRASH_NETWORK_AVAILABLE);
        runTest(
                CONNECTED,
                METERED,
                METRICS_REPORTING_ENABLED,
                METRICS_UPLOAD_PERMITTED,
                CRASH_NETWORK_UNAVAILABLE);
        runTest(
                DISCONNECTED,
                UNMETERED,
                METRICS_REPORTING_ENABLED,
                METRICS_UPLOAD_NOT_PERMITTED,
                CRASH_NETWORK_UNAVAILABLE);

        runTest(
                CONNECTED,
                UNMETERED,
                METRICS_REPORTING_DISABLED,
                METRICS_UPLOAD_NOT_PERMITTED,
                CRASH_NETWORK_AVAILABLE);
        runTest(
                CONNECTED,
                METERED,
                METRICS_REPORTING_DISABLED,
                METRICS_UPLOAD_NOT_PERMITTED,
                CRASH_NETWORK_UNAVAILABLE);
        runTest(
                DISCONNECTED,
                UNMETERED,
                METRICS_REPORTING_DISABLED,
                METRICS_UPLOAD_NOT_PERMITTED,
                CRASH_NETWORK_UNAVAILABLE);
    }

    @Test
    public void testSetMetricsReportingLevel_Permitted() {
        Context context = mock(Context.class);
        PrivacyPreferencesManagerImpl preferenceManager =
                new TestPrivacyPreferencesManager(context);

        // Ensure not enforced by policy.
        writeBoolean(ChromePreferenceKeys.PRIVACY_METRICS_REPORTING_DISABLED_BY_POLICY, false);

        preferenceManager.setMetricsReportingLevel(MetricsReportingLevel.BASIC);

        assertEquals(
                MetricsReportingLevel.BASIC,
                ChromeSharedPreferences.getInstance()
                        .readInt(ChromePreferenceKeys.PRIVACY_METRICS_REPORTING_LEVEL, -1));
        verify(mNativeMock).setMetricsReportingLevelInLocalState(MetricsReportingLevel.BASIC);
    }

    @Test
    public void testSetMetricsReportingLevel_DisabledByPolicy() {
        Context context = mock(Context.class);
        PrivacyPreferencesManagerImpl preferenceManager =
                new TestPrivacyPreferencesManager(context);

        // Enforce by policy.
        writeBoolean(ChromePreferenceKeys.PRIVACY_METRICS_REPORTING_DISABLED_BY_POLICY, true);
        writeInt(
                ChromePreferenceKeys.PRIVACY_METRICS_REPORTING_LEVEL,
                MetricsReportingLevel.ADVANCED);

        preferenceManager.setMetricsReportingLevel(MetricsReportingLevel.BASIC);

        // Value DOES change because the implementation doesn't check the enforced pref yet.
        assertEquals(
                MetricsReportingLevel.BASIC,
                ChromeSharedPreferences.getInstance()
                        .readInt(ChromePreferenceKeys.PRIVACY_METRICS_REPORTING_LEVEL, -1));
        // Native call IS made.
        verify(mNativeMock).setMetricsReportingLevelInLocalState(MetricsReportingLevel.BASIC);
    }

    @Test
    public void testSyncMetricsReportingDisabledByPolicy() {
        Context context = mock(Context.class);
        PrivacyPreferencesManagerImpl preferenceManager =
                new TestPrivacyPreferencesManager(context);

        // Mock policy service initialized.
        PolicyService policyService = mock(PolicyService.class);
        when(policyService.isInitializationComplete()).thenReturn(true);
        PolicyServiceFactory.setPolicyServiceForTest(policyService);

        PrivacyPreferencesManagerImpl.Natives preferenceManagerNatives =
                mock(PrivacyPreferencesManagerImpl.Natives.class);
        when(preferenceManagerNatives.isMetricsReportingDisabledByPolicy()).thenReturn(true);
        PrivacyPreferencesManagerImplJni.setInstanceForTesting(preferenceManagerNatives);

        // Simulate native initialization.
        preferenceManager.onNativeInitialized();

        preferenceManager.syncMetricsReportingDisabledByPolicy();

        assertTrue(
                ChromeSharedPreferences.getInstance()
                        .readBoolean(
                                ChromePreferenceKeys.PRIVACY_METRICS_REPORTING_DISABLED_BY_POLICY,
                                false));
    }

    @Test
    public void testIsBasicMetricsReportingEnabled() {
        Context context = mock(Context.class);
        PrivacyPreferencesManagerImpl preferenceManager =
                new TestPrivacyPreferencesManager(context);

        when(mNativeMock.isBasicMetricsReportingEnabled()).thenReturn(true);

        assertTrue(preferenceManager.isMetricsReportingEnabled());

        when(mNativeMock.isBasicMetricsReportingEnabled()).thenReturn(false);
        assertFalse(preferenceManager.isMetricsReportingEnabled());
    }

    @Test
    public void testUsageAndCrashReportingPermittedByPolicy_PreNative() {
        Context context = mock(Context.class);
        PrivacyPreferencesManagerImpl preferenceManager =
                new TestPrivacyPreferencesManager(context);

        assertTrue(preferenceManager.isUsageAndCrashReportingPermittedByPolicy());
    }

    @Test
    public void testUsageAndCrashReportingPermittedByPolicy_PostNativePrePolicy() {
        Context context = mock(Context.class);
        PrivacyPreferencesManagerImpl preferenceManager =
                new TestPrivacyPreferencesManager(context);

        // Mock policy service not yet initialized.
        PolicyService policyService = mock(PolicyService.class);
        when(policyService.isInitializationComplete()).thenReturn(false);
        PolicyServiceFactory.setPolicyServiceForTest(policyService);

        PrivacyPreferencesManagerImpl.Natives preferenceManagerNatives =
                mock(PrivacyPreferencesManagerImpl.Natives.class);
        PrivacyPreferencesManagerImplJni.setInstanceForTesting(preferenceManagerNatives);

        // Simulate native initialization notification call.
        preferenceManager.onNativeInitialized();

        verify(policyService).addObserver(any());
        assertTrue(preferenceManager.isUsageAndCrashReportingPermittedByPolicy());
    }

    @Test
    public void testUsageAndCrashReportingPermittedByPolicy_PostNativePostPolicy_Enabled() {
        Context context = mock(Context.class);
        PrivacyPreferencesManagerImpl preferenceManager =
                new TestPrivacyPreferencesManager(context);

        // Mock policy service initialized.
        PolicyService policyService = mock(PolicyService.class);
        when(policyService.isInitializationComplete()).thenReturn(true);
        PolicyServiceFactory.setPolicyServiceForTest(policyService);

        // Mock MetricsReportingEnabled=true.
        when(mNativeMock.isMetricsReportingDisabledByPolicy()).thenReturn(false);

        // Simulate native initialization notification call.
        preferenceManager.onNativeInitialized();

        verify(policyService).addObserver(any());
        assertTrue(preferenceManager.isUsageAndCrashReportingPermittedByPolicy());
    }

    @Test
    public void testUsageAndCrashReportingPermittedByPolicy_PostNativePostPolicy_Disabled() {
        Context context = mock(Context.class);
        PrivacyPreferencesManagerImpl preferenceManager =
                new TestPrivacyPreferencesManager(context);

        // Mock policy service initialized.
        PolicyService policyService = mock(PolicyService.class);
        when(policyService.isInitializationComplete()).thenReturn(true);
        PolicyServiceFactory.setPolicyServiceForTest(policyService);

        // Mock MetricsReportingEnabled=false.
        when(mNativeMock.isMetricsReportingDisabledByPolicy()).thenReturn(true);

        // Simulate native initialization notification call.
        preferenceManager.onNativeInitialized();

        verify(policyService).addObserver(any());
        assertFalse(preferenceManager.isUsageAndCrashReportingPermittedByPolicy());
    }

    @Test
    public void testShouldUseMetricsChoiceRestructure() {
        Context context = mock(Context.class);
        PrivacyPreferencesManagerImpl preferenceManager =
                new TestPrivacyPreferencesManager(context);

        writeBoolean(ChromePreferenceKeys.PRIVACY_SHOULD_USE_METRICS_CHOICE_RESTRUCTURE, true);
        assertTrue(preferenceManager.shouldUseMetricsChoiceRestructure());

        writeBoolean(ChromePreferenceKeys.PRIVACY_SHOULD_USE_METRICS_CHOICE_RESTRUCTURE, false);
        assertFalse(preferenceManager.shouldUseMetricsChoiceRestructure());
    }

    @Test
    public void testIsUsageAndCrashReportingPermittedByUser_RestructureDisabled() {
        Context context = mock(Context.class);
        PrivacyPreferencesManagerImpl preferenceManager =
                new TestPrivacyPreferencesManager(context);

        writeBoolean(ChromePreferenceKeys.PRIVACY_SHOULD_USE_METRICS_CHOICE_RESTRUCTURE, false);

        writeBoolean(ChromePreferenceKeys.PRIVACY_METRICS_REPORTING_PERMITTED_BY_USER, true);
        assertTrue(preferenceManager.isUsageAndCrashReportingPermittedByUser());

        writeBoolean(ChromePreferenceKeys.PRIVACY_METRICS_REPORTING_PERMITTED_BY_USER, false);
        assertFalse(preferenceManager.isUsageAndCrashReportingPermittedByUser());
    }

    @Test
    public void testIsUsageAndCrashReportingPermittedByUser_RestructureEnabled() {
        Context context = mock(Context.class);
        PrivacyPreferencesManagerImpl preferenceManager =
                new TestPrivacyPreferencesManager(context);

        writeBoolean(ChromePreferenceKeys.PRIVACY_SHOULD_USE_METRICS_CHOICE_RESTRUCTURE, true);

        // Level NONE -> Permitted false
        writeInt(ChromePreferenceKeys.PRIVACY_METRICS_REPORTING_LEVEL, MetricsReportingLevel.NONE);
        assertFalse(preferenceManager.isUsageAndCrashReportingPermittedByUser());

        // Level BASIC -> Permitted true
        writeInt(ChromePreferenceKeys.PRIVACY_METRICS_REPORTING_LEVEL, MetricsReportingLevel.BASIC);
        assertTrue(preferenceManager.isUsageAndCrashReportingPermittedByUser());

        // Level ADVANCED -> Permitted true
        writeInt(
                ChromePreferenceKeys.PRIVACY_METRICS_REPORTING_LEVEL,
                MetricsReportingLevel.ADVANCED);
        assertTrue(preferenceManager.isUsageAndCrashReportingPermittedByUser());
    }

    @Test
    public void testIsUsageAndCrashReportingPermittedByPolicy_RestructureDisabled() {
        Context context = mock(Context.class);
        PrivacyPreferencesManagerImpl preferenceManager =
                new TestPrivacyPreferencesManager(context);

        writeBoolean(ChromePreferenceKeys.PRIVACY_SHOULD_USE_METRICS_CHOICE_RESTRUCTURE, false);

        writeBoolean(ChromePreferenceKeys.PRIVACY_METRICS_REPORTING_PERMITTED_BY_POLICY, true);
        assertTrue(preferenceManager.isUsageAndCrashReportingPermittedByPolicy());

        writeBoolean(ChromePreferenceKeys.PRIVACY_METRICS_REPORTING_PERMITTED_BY_POLICY, false);
        assertFalse(preferenceManager.isUsageAndCrashReportingPermittedByPolicy());
    }

    @Test
    public void testIsUsageAndCrashReportingPermittedByPolicy_RestructureEnabled() {
        Context context = mock(Context.class);
        PrivacyPreferencesManagerImpl preferenceManager =
                new TestPrivacyPreferencesManager(context);

        writeBoolean(ChromePreferenceKeys.PRIVACY_SHOULD_USE_METRICS_CHOICE_RESTRUCTURE, true);

        // Not enforced by policy -> Always true
        writeBoolean(ChromePreferenceKeys.PRIVACY_METRICS_REPORTING_DISABLED_BY_POLICY, false);
        assertTrue(preferenceManager.isUsageAndCrashReportingPermittedByPolicy());

        // Enforced by policy -> Always false
        writeBoolean(ChromePreferenceKeys.PRIVACY_METRICS_REPORTING_DISABLED_BY_POLICY, true);
        assertFalse(preferenceManager.isUsageAndCrashReportingPermittedByPolicy());

        // Level BASIC or ADVANCED should not change the result if enforced by policy.
        // In reality, if it's BASIC or ADVANCED it wouldn't be "enforced" (managed),
        // but downgraded to "recommended".
        writeInt(ChromePreferenceKeys.PRIVACY_METRICS_REPORTING_LEVEL, MetricsReportingLevel.BASIC);
        assertFalse(preferenceManager.isUsageAndCrashReportingPermittedByPolicy());

        writeInt(
                ChromePreferenceKeys.PRIVACY_METRICS_REPORTING_LEVEL,
                MetricsReportingLevel.ADVANCED);
        assertFalse(preferenceManager.isUsageAndCrashReportingPermittedByPolicy());
    }

    private void runTest(
            boolean isConnected,
            boolean isMetered,
            boolean isMetricsReportingEnabled,
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
        PrivacyPreferencesManagerImpl preferenceManager =
                new TestPrivacyPreferencesManager(context);
        preferenceManager.setUsageAndCrashReporting(isMetricsReportingEnabled);

        // Test the usage and crash reporting permission accessors!
        String state =
                String.format(
                        "[connected = %b, metered = %b, reporting = %b]",
                        isConnected, isMetered, isMetricsReportingEnabled);
        String msg =
                String.format(
                        "Metrics reporting should be %1$b for %2$s",
                        expectedMetricsUploadPermitted, state);
        assertEquals(
                msg, expectedMetricsUploadPermitted, preferenceManager.isMetricsUploadPermitted());

        msg =
                String.format(
                        "Crash reporting should be %1$b for metered state %2$s",
                        expectedNetworkAvailableForCrashUploads, isMetered);
        assertEquals(
                msg,
                expectedNetworkAvailableForCrashUploads,
                preferenceManager.isNetworkAvailableForCrashUploads());
    }

    private void writeBoolean(String key, boolean value) {
        ChromeSharedPreferences.getInstance().writeBoolean(key, value);
    }

    private void writeInt(String key, int value) {
        ChromeSharedPreferences.getInstance().writeInt(key, value);
    }

    private static class TestPrivacyPreferencesManager extends PrivacyPreferencesManagerImpl {
        TestPrivacyPreferencesManager(Context context) {
            super(context);
        }

        // Stub out this call, as it could otherwise call into native code.
        @Override
        public void syncUsageAndCrashReportingPrefs() {}
    }
}
