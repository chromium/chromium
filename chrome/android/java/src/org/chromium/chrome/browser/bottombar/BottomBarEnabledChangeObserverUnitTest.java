// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bottombar;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.app.Activity;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.Robolectric;
import org.robolectric.android.controller.ActivityController;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.DeviceInfo;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.ui.bottombar.BottomBarConfigUtils;

import java.util.concurrent.TimeUnit;

/** Unit tests for {@link BottomBarEnabledChangeObserver}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(qualifiers = "sw300dp")
@EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
public class BottomBarEnabledChangeObserverUnitTest {
    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock private ActivityLifecycleDispatcher mLifecycleDispatcher;
    @Mock private Runnable mRecreateCallback;

    private ActivityController<Activity> mActivityController;
    private Activity mActivity;
    private BottomBarEnabledChangeObserver mObserver;

    @Before
    public void setUp() {
        mActivityController = Robolectric.buildActivity(Activity.class).create();
        mActivity = mActivityController.get();
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.RESUMED);
        createObserver(/* initialEnabledState= */ true);
    }

    @After
    public void tearDown() {
        mObserver.onDestroy();
        ChromeSharedPreferences.getInstance().removeKey(ChromePreferenceKeys.BOTTOM_BAR_ENABLED);
        ChromeSharedPreferences.getInstance()
                .removeKey(ChromePreferenceKeys.BOTTOM_BAR_GLIC_BUTTON_ENABLED);
    }

    @Test
    public void testRecreatesImmediatelyWhenVisibleAndPrefFlipsToFalse() {
        BottomBarConfigUtils.setBottomBarUserEnabled(false);

        verify(mRecreateCallback).run();
    }

    @Test
    public void testRecreatesImmediatelyWhenVisibleAndPrefFlipsToTrue() {
        BottomBarConfigUtils.setBottomBarUserEnabled(false);
        createObserver(/* initialEnabledState= */ false);

        BottomBarConfigUtils.setBottomBarUserEnabled(true);

        verify(mRecreateCallback).run();
    }

    @Test
    public void testRecreatesImmediatelyWhenPausedOrStarted() {
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.PAUSED);
        BottomBarConfigUtils.setBottomBarUserEnabled(false);
        verify(mRecreateCallback).run();

        createObserver(/* initialEnabledState= */ false);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STARTED);
        BottomBarConfigUtils.setBottomBarUserEnabled(true);
        verify(mRecreateCallback).run();
    }

    @Test
    public void testRecreatesImmediatelyOnConstructionWhenStateAlreadyDiverged() {
        BottomBarConfigUtils.setBottomBarUserEnabled(false);

        createObserver(/* initialEnabledState= */ true);

        verify(mRecreateCallback).run();
    }

    @Test
    public void testDelaysRecreateWhenStoppedUntilSettleDelayElapses() {
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STOPPED);

        BottomBarConfigUtils.setBottomBarUserEnabled(false);
        verify(mRecreateCallback, never()).run();

        ShadowLooper.idleMainLooper(
                BottomBarEnabledChangeObserver.RECREATE_SETTLE_DELAY_MS, TimeUnit.MILLISECONDS);
        verify(mRecreateCallback).run();
    }

    @Test
    public void testStoppedToggleRestoredBeforeDelayDoesNotRecreate() {
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STOPPED);

        BottomBarConfigUtils.setBottomBarUserEnabled(false);
        BottomBarConfigUtils.setBottomBarUserEnabled(true);
        ShadowLooper.idleMainLooper(
                BottomBarEnabledChangeObserver.RECREATE_SETTLE_DELAY_MS, TimeUnit.MILLISECONDS);

        assertFalse(mObserver.hasEnabledStateChanged());
        verify(mRecreateCallback, never()).run();
    }

    @Test
    public void testRapidStoppedTogglesResetSettleDelay() {
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STOPPED);

        BottomBarConfigUtils.setBottomBarUserEnabled(false);
        ShadowLooper.idleMainLooper(200, TimeUnit.MILLISECONDS);
        BottomBarConfigUtils.setBottomBarUserEnabled(true);
        ShadowLooper.idleMainLooper(200, TimeUnit.MILLISECONDS);
        BottomBarConfigUtils.setBottomBarUserEnabled(false);

        ShadowLooper.idleMainLooper(200, TimeUnit.MILLISECONDS);
        verify(mRecreateCallback, never()).run();

        ShadowLooper.idleMainLooper(300, TimeUnit.MILLISECONDS);
        verify(mRecreateCallback).run();
    }

    @Test
    public void testOnStartWithNativeRecreatesBeforeDelayAndCancelsTimer() {
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STOPPED);
        BottomBarConfigUtils.setBottomBarUserEnabled(false);

        mObserver.onStartWithNative();
        verify(mRecreateCallback).run();

        clearInvocations(mRecreateCallback);
        ShadowLooper.idleMainLooper(
                BottomBarEnabledChangeObserver.RECREATE_SETTLE_DELAY_MS, TimeUnit.MILLISECONDS);
        verify(mRecreateCallback, never()).run();
    }

    @Test
    public void testRecreateUnregistersAndRunsAtMostOnce() {
        BottomBarConfigUtils.setBottomBarUserEnabled(false);
        verify(mRecreateCallback).run();
        verify(mLifecycleDispatcher).unregister(mObserver);

        clearInvocations(mRecreateCallback);
        BottomBarConfigUtils.setBottomBarUserEnabled(true);
        BottomBarConfigUtils.setBottomBarUserEnabled(false);
        verify(mRecreateCallback, never()).run();
    }

    @Test
    public void testDoesNotRecreateWhenPrefWrittenWithSameState() {
        BottomBarConfigUtils.setBottomBarUserEnabled(true);

        verify(mRecreateCallback, never()).run();
    }

    @Test
    public void testIgnoresOtherKeys() {
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.BOTTOM_BAR_GLIC_BUTTON_ENABLED, false);

        verify(mRecreateCallback, never()).run();
    }

    @Test
    public void testIgnoresEligibilityChange() {
        DeviceInfo.setIsAutomotiveForTesting(true);

        mObserver.onStartWithNative();

        assertFalse(mObserver.hasEnabledStateChanged());
        verify(mRecreateCallback, never()).run();
    }

    @Test
    public void testDoesNotRecreateWhenActivityFinishing() {
        mActivity.finish();

        BottomBarConfigUtils.setBottomBarUserEnabled(false);

        assertTrue(mObserver.hasEnabledStateChanged());
        verify(mRecreateCallback, never()).run();
    }

    @Test
    public void testDoesNotRecreateWhenActivityDestroyed() {
        mActivityController.destroy();

        BottomBarConfigUtils.setBottomBarUserEnabled(false);

        assertTrue(mObserver.hasEnabledStateChanged());
        verify(mRecreateCallback, never()).run();
    }

    @Test
    public void testOnDestroyUnregistersAndCancelsPendingRecreate() {
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STOPPED);
        BottomBarConfigUtils.setBottomBarUserEnabled(false);

        mObserver.onDestroy();
        ShadowLooper.idleMainLooper(
                BottomBarEnabledChangeObserver.RECREATE_SETTLE_DELAY_MS, TimeUnit.MILLISECONDS);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.RESUMED);
        BottomBarConfigUtils.setBottomBarUserEnabled(true);
        BottomBarConfigUtils.setBottomBarUserEnabled(false);

        verify(mLifecycleDispatcher).unregister(mObserver);
        verify(mRecreateCallback, never()).run();
    }

    private void createObserver(boolean initialEnabledState) {
        if (mObserver != null) {
            mObserver.onDestroy();
        }
        clearInvocations(mRecreateCallback);
        mObserver =
                new BottomBarEnabledChangeObserver(
                        mActivity, mLifecycleDispatcher, initialEnabledState, mRecreateCallback);
    }
}
