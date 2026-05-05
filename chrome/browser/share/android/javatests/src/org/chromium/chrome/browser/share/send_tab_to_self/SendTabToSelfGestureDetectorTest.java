// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.send_tab_to_self;

import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.hardware.SensorManager;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Tests for SendTabToSelfGestureDetector */
@RunWith(BaseRobolectricTestRunner.class)
public class SendTabToSelfGestureDetectorTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Runnable mCallback;
    @Mock private Context mContext;
    @Mock private SensorManager mSensorManager;

    private SendTabToSelfGestureDetector mDetector;

    @Before
    public void setUp() {
        when(mContext.getSystemService(Context.SENSOR_SERVICE)).thenReturn(mSensorManager);
        mDetector = new SendTabToSelfGestureDetector(mContext);
        mDetector.setGestureDetectedCallbackForTesting(mCallback);
    }

    @Test
    @SmallTest
    public void testSingleSpikeDoesNotTrigger() {
        // A single shake spike should not be enough to trigger.
        mDetector.onSensorValuesChanged(new float[] {0f, 0f, 20f}, 1000L);
        verify(mCallback, never()).run();
    }

    @Test
    @SmallTest
    public void testDoubleSpikeTriggers() {
        // Two shake spikes within a valid time window should trigger a tab share.
        mDetector.onSensorValuesChanged(new float[] {0f, 0f, 20f}, 1000L);
        verify(mCallback, never()).run();

        mDetector.onSensorValuesChanged(new float[] {0f, 0f, 20f}, 1300L);
        verify(mCallback, times(1)).run();
    }

    @Test
    @SmallTest
    public void testTooFastSpikesDoNotTrigger() {
        // Spikes happening too quickly (e.g. within 50ms) are likely noise and should not trigger.
        mDetector.onSensorValuesChanged(new float[] {0f, 0f, 20f}, 1000L);
        mDetector.onSensorValuesChanged(new float[] {0f, 0f, 20f}, 1050L);
        verify(mCallback, never()).run();
    }

    @Test
    @SmallTest
    public void testTooSlowSpikesDoNotTrigger() {
        // Spikes happening too far apart (e.g. 600ms) should not be considered a single gesture.
        mDetector.onSensorValuesChanged(new float[] {0f, 0f, 20f}, 1000L);
        mDetector.onSensorValuesChanged(new float[] {0f, 0f, 20f}, 1600L);
        verify(mCallback, never()).run();
    }

    @Test
    @SmallTest
    public void testResetAfterTrigger() {
        // Trigger the first gesture.
        mDetector.onSensorValuesChanged(new float[] {0f, 0f, 20f}, 1000L);
        mDetector.onSensorValuesChanged(new float[] {0f, 0f, 20f}, 1300L);
        verify(mCallback, times(1)).run();

        // A third spike soon after should not trigger again immediately (it's part of the reset).
        mDetector.onSensorValuesChanged(new float[] {0f, 0f, 20f}, 1400L);
        verify(mCallback, times(1)).run(); // Still 1 total execution

        // A fourth spike after another valid interval should trigger a second time.
        mDetector.onSensorValuesChanged(new float[] {0f, 0f, 20f}, 1700L);
        verify(mCallback, times(2)).run();
    }
}
