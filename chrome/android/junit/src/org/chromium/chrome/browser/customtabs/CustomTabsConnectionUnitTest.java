// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.junit.Assert.assertFalse;
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
import android.content.Intent;
import android.os.Bundle;
import android.os.Process;

import androidx.browser.customtabs.CustomTabsCallback;
import androidx.browser.customtabs.CustomTabsSessionToken;

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
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.metrics.UmaSessionStats;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;

import java.util.ArrayList;
import java.util.List;

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

    private static final ArrayList<String> REALTIME_SIGNALS_AND_BRANDING =
            new ArrayList<String>(List.of(ChromeFeatureList.CCT_REAL_TIME_ENGAGEMENT_SIGNALS,
                    ChromeFeatureList.CCT_BRAND_TRANSPARENCY));
    private CustomTabsConnection mConnection;
    private int mUid = Process.myUid();

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
        mConnection = CustomTabsConnection.getInstance();
        mConnection.setIsDynamicFeaturesEnabled(true);
        when(mSessionHandler.getSession()).thenReturn(mSession);
        ChromeApplicationImpl.getComponent().resolveSessionDataHolder().setActiveHandler(
                mSessionHandler);
    }

    @After
    public void tearDown() {
        CustomTabsConnection.setInstanceForTesting(null);
        ChromeApplicationImpl.getComponent().resolveSessionDataHolder().removeActiveHandler(
                mSessionHandler);
        ShadowPostTask.reset();
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

        when(mSession.getCallback()).thenReturn(mCallback);
        ShadowProcess.setUid(uid);

        Bundle bundle = new Bundle();
        bundle.putInt(ON_ACTIVITY_LAYOUT_LEFT_EXTRA, left);
        bundle.putInt(ON_ACTIVITY_LAYOUT_TOP_EXTRA, top);
        bundle.putInt(ON_ACTIVITY_LAYOUT_RIGHT_EXTRA, right);
        bundle.putInt(ON_ACTIVITY_LAYOUT_BOTTOM_EXTRA, bottom);
        bundle.putInt(ON_ACTIVITY_LAYOUT_STATE_EXTRA, ACTIVITY_LAYOUT_STATE_BOTTOM_SHEET);

        shadowOf(RuntimeEnvironment.getApplication().getApplicationContext().getPackageManager())
                .setPackagesForUid(uid, "test.package.name");
        mConnection.mClientManager.newSession(mSession, uid, null, null, null);
        mConnection.onActivityLayout(
                mSession, left, top, right, bottom, ACTIVITY_LAYOUT_STATE_BOTTOM_SHEET);

        verify(mCallback).extraCallback(eq(ON_ACTIVITY_LAYOUT_CALLBACK), refEq(bundle));
    }

    // TODO(https://crrev.com/c/4118209) Add more tests for Feature enabling/disabling.
}
