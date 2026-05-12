// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.send_tab_to_self;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.hardware.Sensor;
import android.hardware.SensorManager;

import androidx.lifecycle.Lifecycle;
import androidx.lifecycle.LifecycleOwner;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/** Tests for SendTabToSelfGestureDetector */
@RunWith(BaseRobolectricTestRunner.class)
public class SendTabToSelfGestureDetectorTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Context mContext;
    @Mock private SensorManager mSensorManager;
    @Mock private Sensor mSensor;
    @Mock private Tab mTab;
    @Mock private Profile mProfile;
    @Mock private WebContents mWebContents;
    @Mock private SendTabToSelfAndroidBridge.Natives mNativeMock;
    @Mock private LifecycleOwner mLifecycleOwner;
    @Mock private Lifecycle mLifecycle;

    private SendTabToSelfGestureDetector mDetector;

    @Before
    public void setUp() {
        when(mLifecycleOwner.getLifecycle()).thenReturn(mLifecycle);
        when(mContext.getSystemService(Context.SENSOR_SERVICE)).thenReturn(mSensorManager);
        when(mSensorManager.getDefaultSensor(Sensor.TYPE_LINEAR_ACCELERATION)).thenReturn(mSensor);

        SendTabToSelfAndroidBridgeJni.setInstanceForTesting(mNativeMock);

        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mTab.getUrl()).thenReturn(new GURL("https://www.example.com"));
        when(mTab.getTitle()).thenReturn("Example Page");

        // Set up mock target devices so the bridge check passes
        List<TargetDeviceInfo> mockDevices = new ArrayList<>();
        mockDevices.add(new TargetDeviceInfo("device_name", "cache_guid", 1, "1000"));
        when(mNativeMock.getAllTargetDeviceInfos(any())).thenReturn(mockDevices);

        // Default to offering the feature, which implies the user is signed in and has devices.
        when(mNativeMock.getEntryPointDisplayReason(any(), any()))
                .thenReturn(EntryPointDisplayReason.OFFER_FEATURE);

        mDetector =
                new SendTabToSelfGestureDetector(
                        mContext, mLifecycleOwner, () -> mTab, () -> mProfile);
    }

    @Test
    @SmallTest
    public void testSingleSpikeDoesNotTrigger() {
        // A single shake spike should not be enough to trigger.
        mDetector.onSensorValuesChanged(new float[] {0f, 0f, 20f}, 1000L);
        verify(mNativeMock, never())
                .sendTabToDevice(any(), any(), anyString(), anyString(), anyString(), any());
    }

    @Test
    @SmallTest
    public void testDoubleSpikeTriggers() {
        // Two shake spikes within a valid time window should trigger a tab share.
        mDetector.onSensorValuesChanged(new float[] {0f, 0f, 20f}, 1000L);
        mDetector.onSensorValuesChanged(new float[] {0f, 0f, 20f}, 1300L);
        verify(mNativeMock, times(1))
                .sendTabToDevice(
                        eq(mProfile),
                        eq(mWebContents),
                        eq("cache_guid"),
                        eq("https://www.example.com/"),
                        eq("Example Page"),
                        any());
    }

    @Test
    @SmallTest
    public void testTooFastSpikesDoNotTrigger() {
        // Spikes happening too quickly (e.g. within 50ms) are likely noise and should not trigger.
        mDetector.onSensorValuesChanged(new float[] {0f, 0f, 20f}, 1000L);
        mDetector.onSensorValuesChanged(new float[] {0f, 0f, 20f}, 1050L);
        verify(mNativeMock, never())
                .sendTabToDevice(any(), any(), anyString(), anyString(), anyString(), any());
    }

    @Test
    @SmallTest
    public void testTooSlowSpikesDoNotTrigger() {
        // Spikes happening too far apart (e.g. 600ms) should not be considered a single gesture.
        mDetector.onSensorValuesChanged(new float[] {0f, 0f, 20f}, 1000L);
        mDetector.onSensorValuesChanged(new float[] {0f, 0f, 20f}, 1600L);
        verify(mNativeMock, never())
                .sendTabToDevice(any(), any(), anyString(), anyString(), anyString(), any());
    }

    @Test
    @SmallTest
    public void testResetAfterTrigger() {
        // Trigger the first gesture.
        mDetector.onSensorValuesChanged(new float[] {0f, 0f, 20f}, 1000L);
        mDetector.onSensorValuesChanged(new float[] {0f, 0f, 20f}, 1300L);
        verify(mNativeMock, times(1))
                .sendTabToDevice(any(), any(), anyString(), anyString(), anyString(), any());

        // A third spike soon after should not trigger again immediately (it's part of the reset).
        mDetector.onSensorValuesChanged(new float[] {0f, 0f, 20f}, 1400L);
        verify(mNativeMock, times(1))
                .sendTabToDevice(any(), any(), anyString(), anyString(), anyString(), any());

        // A fourth spike after another valid interval should trigger a second time.
        mDetector.onSensorValuesChanged(new float[] {0f, 0f, 20f}, 1700L);
        verify(mNativeMock, times(2))
                .sendTabToDevice(any(), any(), anyString(), anyString(), anyString(), any());
    }

    @Test
    @SmallTest
    public void testValidGesture_ModelNotReady_DoesNotTrigger() {
        // Simulate the model not being ready yet.
        when(mNativeMock.getEntryPointDisplayReason(any(), any())).thenReturn(null);

        // Perform a valid gesture
        mDetector.onSensorValuesChanged(new float[] {0f, 0f, 20f}, 1000L);
        mDetector.onSensorValuesChanged(new float[] {0f, 0f, 20f}, 1300L);

        verify(mNativeMock, never())
                .sendTabToDevice(any(), any(), anyString(), anyString(), anyString(), any());
    }

    @Test
    @SmallTest
    public void testValidGesture_UserNotSignedIn_DoesNotTrigger() {
        // Simulate the user not being signed in.
        when(mNativeMock.getEntryPointDisplayReason(any(), any()))
                .thenReturn(EntryPointDisplayReason.OFFER_SIGN_IN);

        // Perform a valid gesture
        mDetector.onSensorValuesChanged(new float[] {0f, 0f, 20f}, 1000L);
        mDetector.onSensorValuesChanged(new float[] {0f, 0f, 20f}, 1300L);

        verify(mNativeMock, never())
                .sendTabToDevice(any(), any(), anyString(), anyString(), anyString(), any());
    }

    @Test
    @SmallTest
    public void testValidGesture_NoTargetDevice_DoesNotTrigger() {
        // Simulate the user having no target devices.
        when(mNativeMock.getEntryPointDisplayReason(any(), any()))
                .thenReturn(EntryPointDisplayReason.INFORM_NO_TARGET_DEVICE);

        // Perform a valid gesture
        mDetector.onSensorValuesChanged(new float[] {0f, 0f, 20f}, 1000L);
        mDetector.onSensorValuesChanged(new float[] {0f, 0f, 20f}, 1300L);

        verify(mNativeMock, never())
                .sendTabToDevice(any(), any(), anyString(), anyString(), anyString(), any());
    }

    @Test
    @SmallTest
    public void testValidGesture_AllPreconditionsMet_Triggers() {
        // Simulate the user being signed in and having target devices.
        when(mNativeMock.getEntryPointDisplayReason(any(), any()))
                .thenReturn(EntryPointDisplayReason.OFFER_FEATURE);

        // Perform a valid gesture
        mDetector.onSensorValuesChanged(new float[] {0f, 0f, 20f}, 1000L);
        mDetector.onSensorValuesChanged(new float[] {0f, 0f, 20f}, 1300L);

        verify(mNativeMock, times(1))
                .sendTabToDevice(any(), any(), anyString(), anyString(), anyString(), any());
    }

    @Test
    @SmallTest
    public void testStartRegistersListener() {
        mDetector.onStart(mLifecycleOwner);
        verify(mSensorManager).registerListener(eq(mDetector), eq(mSensor), anyInt());
    }

    @Test
    @SmallTest
    public void testStopUnregistersListener() {
        mDetector.onStart(mLifecycleOwner);
        mDetector.onStop(mLifecycleOwner);
        verify(mSensorManager).unregisterListener(mDetector);
    }

    @Test
    @SmallTest
    public void testStartDoesNotRegisterIfSensorMissing() {
        when(mSensorManager.getDefaultSensor(Sensor.TYPE_LINEAR_ACCELERATION)).thenReturn(null);
        SendTabToSelfGestureDetector detector =
                new SendTabToSelfGestureDetector(
                        mContext, mLifecycleOwner, () -> mTab, () -> mProfile);

        detector.onStart(mLifecycleOwner);
        verify(mSensorManager, never()).registerListener(any(), any(), anyInt());
    }

    @Test
    @SmallTest
    public void testStopDoesNothingIfNotStarted() {
        mDetector.onStop(mLifecycleOwner);
        verify(mSensorManager, never()).unregisterListener(eq(mDetector));
    }
}
