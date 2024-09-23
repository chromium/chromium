// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static androidx.browser.customtabs.CustomTabsCallback.ACTIVITY_LAYOUT_STATE_BOTTOM_SHEET;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.refEq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import static org.chromium.chrome.browser.customtabs.CustomTabsConnection.ON_ACTIVITY_LAYOUT_BOTTOM_EXTRA;
import static org.chromium.chrome.browser.customtabs.CustomTabsConnection.ON_ACTIVITY_LAYOUT_CALLBACK;
import static org.chromium.chrome.browser.customtabs.CustomTabsConnection.ON_ACTIVITY_LAYOUT_LEFT_EXTRA;
import static org.chromium.chrome.browser.customtabs.CustomTabsConnection.ON_ACTIVITY_LAYOUT_RIGHT_EXTRA;
import static org.chromium.chrome.browser.customtabs.CustomTabsConnection.ON_ACTIVITY_LAYOUT_STATE_EXTRA;
import static org.chromium.chrome.browser.customtabs.CustomTabsConnection.ON_ACTIVITY_LAYOUT_TOP_EXTRA;

import android.app.PendingIntent;
import android.os.Bundle;

import androidx.browser.customtabs.CustomTabsCallback;
import androidx.browser.customtabs.CustomTabsSessionToken;
import androidx.browser.customtabs.EngagementSignalsCallback;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.shadows.ShadowProcess;

import org.chromium.base.task.TaskTraits;
import org.chromium.base.task.test.ShadowPostTask;
import org.chromium.base.task.test.ShadowPostTask.TestImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.ChromeApplicationImpl;
import org.chromium.chrome.browser.browserservices.SessionHandler;
import org.chromium.chrome.browser.customtabs.content.EngagementSignalsHandler;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.metrics.UmaSessionStats;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManagerImpl;

/** Tests for some parts of {@link CustomTabsConnection}. */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.UNIT_TESTS)
@Config(shadows = {CustomTabsConnectionUnitTest.ShadowUmaSessionStats.class, ShadowPostTask.class})
public class CustomTabsConnectionUnitTest {

    @Mock private SessionHandler mSessionHandler;
    @Mock private CustomTabsSessionToken mSession;
    @Mock private CustomTabsCallback mCallback;
    @Mock private PrivacyPreferencesManagerImpl mPrivacyPreferencesManager;
    @Mock private EngagementSignalsCallback mEngagementSignalsCallback;

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
        MockitoAnnotations.initMocks(this);
        ShadowPostTask.setTestImpl(
                new TestImpl() {
                    @Override
                    public void postDelayedTask(
                            @TaskTraits int taskTraits, Runnable task, long delay) {
                        task.run();
                    }
                });
        CustomTabsConnection.setInstanceForTesting(null);
        mConnection = CustomTabsConnection.getInstance();
        mConnection.setIsDynamicFeaturesEnabled(true);
        when(mSession.getCallback()).thenReturn(mCallback);
        when(mSessionHandler.getSession()).thenReturn(mSession);
        ChromeApplicationImpl.getComponent()
                .resolveSessionDataHolder()
                .setActiveHandler(mSessionHandler);
        PrivacyPreferencesManagerImpl.setInstanceForTesting(mPrivacyPreferencesManager);
    }

    @After
    public void tearDown() {
        ChromeApplicationImpl.getComponent()
                .resolveSessionDataHolder()
                .removeActiveHandler(mSessionHandler);
    }

    @Test
    public void areExperimentsSupported_NullInputs() {
        assertFalse(mConnection.areExperimentsSupported(null, null));
    }

    @Test
    public void updateVisuals_BottomBarSwipeUpGesture() {
        var bundle = new Bundle();
        var pendingIntent = mock(PendingIntent.class);
        bundle.putParcelable(
                CustomTabIntentDataProvider.EXTRA_SECONDARY_TOOLBAR_SWIPE_UP_ACTION, pendingIntent);
        mConnection.updateVisuals(mSession, bundle);
        verify(mSessionHandler).updateSecondaryToolbarSwipeUpPendingIntent(eq(pendingIntent));
    }

    @Test
    public void onActivityLayout_CallbackIsCalledForNamedMethod() {
        int left = 0;
        int top = 0;
        int right = 100;
        int bottom = 200;

        Bundle bundle = new Bundle();
        bundle.putInt(ON_ACTIVITY_LAYOUT_LEFT_EXTRA, left);
        bundle.putInt(ON_ACTIVITY_LAYOUT_TOP_EXTRA, top);
        bundle.putInt(ON_ACTIVITY_LAYOUT_RIGHT_EXTRA, right);
        bundle.putInt(ON_ACTIVITY_LAYOUT_BOTTOM_EXTRA, bottom);
        bundle.putInt(ON_ACTIVITY_LAYOUT_STATE_EXTRA, ACTIVITY_LAYOUT_STATE_BOTTOM_SHEET);

        initSession();
        mConnection.onActivityLayout(
                mSession, left, top, right, bottom, ACTIVITY_LAYOUT_STATE_BOTTOM_SHEET);

        verify(mCallback).extraCallback(eq(ON_ACTIVITY_LAYOUT_CALLBACK), refEq(bundle));
    }

    @Test
    public void isEngagementSignalsApiAvailable_SupplierSet() {
        initSession();
        when(mPrivacyPreferencesManager.isUsageAndCrashReportingPermitted()).thenReturn(true);
        // Test the supplier takes precedence.
        mConnection.setEngagementSignalsAvailableSupplier(mSession, () -> true);
        assertTrue(mConnection.isEngagementSignalsApiAvailable(mSession, Bundle.EMPTY));
        mConnection.setEngagementSignalsAvailableSupplier(mSession, () -> false);
        assertFalse(mConnection.isEngagementSignalsApiAvailable(mSession, Bundle.EMPTY));
    }

    @Test
    public void isEngagementSignalsApiAvailable_Fallback() {
        initSession();
        when(mPrivacyPreferencesManager.isUsageAndCrashReportingPermitted()).thenReturn(true);
        assertTrue(mConnection.isEngagementSignalsApiAvailable(mSession, Bundle.EMPTY));
        when(mPrivacyPreferencesManager.isUsageAndCrashReportingPermitted()).thenReturn(false);
        assertFalse(mConnection.isEngagementSignalsApiAvailable(mSession, Bundle.EMPTY));
    }

    @Test
    public void setEngagementSignalsCallback_Available() {
        initSession();
        when(mPrivacyPreferencesManager.isUsageAndCrashReportingPermitted()).thenReturn(true);
        assertTrue(
                mConnection.setEngagementSignalsCallback(
                        mSession, mEngagementSignalsCallback, Bundle.EMPTY));
        assertEquals(
                mEngagementSignalsCallback,
                mConnection.mClientManager.getEngagementSignalsCallbackForSession(mSession));
    }

    @Test
    public void setEngagementSignalsCallback_NotAvailable() {
        initSession();
        when(mPrivacyPreferencesManager.isUsageAndCrashReportingPermitted()).thenReturn(false);
        assertFalse(
                mConnection.setEngagementSignalsCallback(
                        mSession, mEngagementSignalsCallback, Bundle.EMPTY));
        assertNull(mConnection.mClientManager.getEngagementSignalsCallbackForSession(mSession));
    }

    @Test
    public void testOnMinimized() {
        initSession();
        mConnection.onMinimized(mSession);
        verify(mCallback).onMinimized(any(Bundle.class));
    }

    @Test
    public void testOnUnminimized() {
        initSession();
        mConnection.onUnminimized(mSession);
        verify(mCallback).onUnminimized(any(Bundle.class));
    }

    private void initSession() {
        int uid = 111;
        ShadowProcess.setUid(uid);
        shadowOf(RuntimeEnvironment.getApplication().getApplicationContext().getPackageManager())
                .setPackagesForUid(uid, "test.package.name");
        var handler = new EngagementSignalsHandler(mConnection, mSession);
        mConnection.mClientManager.newSession(mSession, uid, null, null, null, handler);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.SEARCH_IN_CCT)
    public void shouldEnableOmniboxForIntent_featureDisabled() {
        // The logic is currently expected to not even peek in the intent.
        assertFalse(mConnection.shouldEnableOmniboxForIntent(null));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SEARCH_IN_CCT)
    public void shouldEnableOmniboxForIntent_featureEnabled() {
        // The logic is currently expected to not even peek in the intent.
        // Omnibox must remain disabled even if the feature flag is on.
        assertFalse(mConnection.shouldEnableOmniboxForIntent(null));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CCT_EPHEMERAL_MODE)
    public void isEphemeralBrowsingSupported_featureEnabled() {
        Bundle bundle =
                mConnection.extraCommand(
                        CustomTabsConnection.IS_EPHEMERAL_BROWSING_SUPPORTED, null);
        assertNotNull(bundle);
        assertTrue(bundle.containsKey(CustomTabsConnection.EPHEMERAL_BROWSING_SUPPORTED_KEY));
        assertTrue(bundle.getBoolean(CustomTabsConnection.EPHEMERAL_BROWSING_SUPPORTED_KEY));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.CCT_EPHEMERAL_MODE)
    public void isEphemeralBrowsingSupported_featureDisabled() {
        Bundle bundle =
                mConnection.extraCommand(
                        CustomTabsConnection.IS_EPHEMERAL_BROWSING_SUPPORTED, null);
        assertNotNull(bundle);
        assertTrue(bundle.containsKey(CustomTabsConnection.EPHEMERAL_BROWSING_SUPPORTED_KEY));
        assertFalse(bundle.getBoolean(CustomTabsConnection.EPHEMERAL_BROWSING_SUPPORTED_KEY));
    }

    // TODO(https://crrev.com/c/4118209) Add more tests for Feature enabling/disabling.
}
