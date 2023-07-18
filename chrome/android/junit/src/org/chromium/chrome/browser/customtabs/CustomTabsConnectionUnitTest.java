// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.refEq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import static org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider.ACTIVITY_LAYOUT_STATE_BOTTOM_SHEET;
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
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
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
import org.chromium.chrome.browser.ChromeApplicationImpl;
import org.chromium.chrome.browser.browserservices.SessionHandler;
import org.chromium.chrome.browser.customtabs.content.EngagementSignalsHandler;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.metrics.UmaSessionStats;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManagerImpl;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;

/** Tests for some parts of {@link CustomTabsConnection}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {CustomTabsConnectionUnitTest.ShadowUmaSessionStats.class, ShadowPostTask.class})
public class CustomTabsConnectionUnitTest {
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    @Mock
    private SessionHandler mSessionHandler;
    @Mock
    private CustomTabsSessionToken mSession;
    @Mock
    private CustomTabsCallback mCallback;
    @Mock
    private PrivacyPreferencesManagerImpl mPrivacyPreferencesManager;
    @Mock
    private EngagementSignalsCallback mEngagementSignalsCallback;

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
        ShadowPostTask.setTestImpl(new TestImpl() {
            @Override
            public void postDelayedTask(@TaskTraits int taskTraits, Runnable task, long delay) {
                task.run();
            }
        });
        CustomTabsConnection.setInstanceForTesting(null);
        mConnection = CustomTabsConnection.getInstance();
        mConnection.setIsDynamicFeaturesEnabled(true);
        when(mSession.getCallback()).thenReturn(mCallback);
        when(mSessionHandler.getSession()).thenReturn(mSession);
        ChromeApplicationImpl.getComponent().resolveSessionDataHolder().setActiveHandler(
                mSessionHandler);
        PrivacyPreferencesManagerImpl.setInstanceForTesting(mPrivacyPreferencesManager);
    }

    @After
    public void tearDown() {
        ChromeApplicationImpl.getComponent().resolveSessionDataHolder().removeActiveHandler(
                mSessionHandler);
        ShadowPostTask.reset();
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
    @DisableFeatures({ChromeFeatureList.CCT_BOTTOM_BAR_SWIPE_UP_GESTURE})
    public void updateVisuals_BottomBarSwipeUpGesture_FeatureDisabled() {
        var bundle = new Bundle();
        var pendingIntent = mock(PendingIntent.class);
        bundle.putParcelable(
                CustomTabIntentDataProvider.EXTRA_SECONDARY_TOOLBAR_SWIPE_UP_ACTION, pendingIntent);
        mConnection.updateVisuals(mSession, bundle);
        verify(mSessionHandler, never())
                .updateSecondaryToolbarSwipeUpPendingIntent(eq(pendingIntent));
    }

    @Test
    @EnableFeatures({ChromeFeatureList.CCT_RESIZABLE_SIDE_SHEET})
    public void onActivityLayout_CallbackIsCalledForNamedMethod() {
        int uid = 123;
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
    @EnableFeatures({ChromeFeatureList.CCT_BRAND_TRANSPARENCY,
            ChromeFeatureList.CCT_REAL_TIME_ENGAGEMENT_SIGNALS})
    public void
    setEngagementSignalsCallback_Available() {
        initSession();
        when(mPrivacyPreferencesManager.isUsageAndCrashReportingPermitted()).thenReturn(true);
        assertTrue(mConnection.setEngagementSignalsCallback(
                mSession, mEngagementSignalsCallback, Bundle.EMPTY));
        assertEquals(mEngagementSignalsCallback,
                mConnection.mClientManager.getEngagementSignalsCallbackForSession(mSession));
    }

    @Test
    @EnableFeatures({ChromeFeatureList.CCT_BRAND_TRANSPARENCY,
            ChromeFeatureList.CCT_REAL_TIME_ENGAGEMENT_SIGNALS})
    public void
    setEngagementSignalsCallback_NotAvailable() {
        initSession();
        when(mPrivacyPreferencesManager.isUsageAndCrashReportingPermitted()).thenReturn(false);
        assertFalse(mConnection.setEngagementSignalsCallback(
                mSession, mEngagementSignalsCallback, Bundle.EMPTY));
        assertNull(mConnection.mClientManager.getEngagementSignalsCallbackForSession(mSession));
    }

    private void initSession() {
        int uid = 111;
        ShadowProcess.setUid(uid);
        shadowOf(RuntimeEnvironment.getApplication().getApplicationContext().getPackageManager())
                .setPackagesForUid(uid, "test.package.name");
        var handler = new EngagementSignalsHandler(mConnection, mSession);
        mConnection.mClientManager.newSession(mSession, uid, null, null, null, handler);
    }

    // TODO(https://crrev.com/c/4118209) Add more tests for Feature enabling/disabling.
}
