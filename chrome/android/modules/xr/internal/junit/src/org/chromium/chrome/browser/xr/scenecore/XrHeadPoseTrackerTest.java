// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xr.scenecore;

import static org.junit.Assert.assertFalse;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.xr.scenecore.XrPose;
import org.chromium.ui.xr.scenecore.XrSceneCoreSessionManager;

import java.util.concurrent.TimeUnit;

/** Tests for {@link XrHeadPoseTracker}. */
@RunWith(BaseRobolectricTestRunner.class)
public class XrHeadPoseTrackerTest {
    private static final long CHECK_INTERVAL_MS = 16L;

    @Mock private XrSceneCoreSessionManager mSessionManager;
    @Mock private Callback<XrPose> mCallback;
    @Mock private XrPose mPose1;
    @Mock private XrPose mPose2;

    private XrHeadPoseTracker mTracker;

    @Before
    public void setUp() {
        MockitoAnnotations.openMocks(this);
        when(mSessionManager.isHeadTrackingEnabled()).thenReturn(true);
        when(mSessionManager.getHeadPoseInActivitySpace()).thenReturn(mPose1);
        mTracker = new XrHeadPoseTracker(mSessionManager, mCallback, CHECK_INTERVAL_MS);
    }

    @After
    public void tearDown() {
        if (mTracker != null) {
            mTracker.stop();
        }
    }

    @Test
    public void testStart_WhenHeadTrackingDisabled_ReturnsFalse() {
        when(mSessionManager.isHeadTrackingEnabled()).thenReturn(false);
        assertFalse(mTracker.start());
    }

    @Test
    public void testStart_PollsHeadPoseAndTriggersCallback() {
        when(mSessionManager.getHeadPoseInActivitySpace()).thenReturn(mPose1);

        mTracker.start();
        verify(mCallback).onResult(mPose1);

        // Update pose and advance looper
        when(mSessionManager.getHeadPoseInActivitySpace()).thenReturn(mPose2);

        ShadowLooper.idleMainLooper(CHECK_INTERVAL_MS, TimeUnit.MILLISECONDS);
        verify(mCallback).onResult(mPose2);
    }

    @Test
    public void testStart_DuplicatePose_DoesNotTriggerCallbackAgain() {
        when(mSessionManager.getHeadPoseInActivitySpace()).thenReturn(mPose1);

        mTracker.start();
        verify(mCallback, times(1)).onResult(mPose1);

        // Same pose returned again
        ShadowLooper.idleMainLooper(CHECK_INTERVAL_MS, TimeUnit.MILLISECONDS);
        verify(mCallback, times(1)).onResult(any());
    }

    @Test
    public void testStop_StopsPolling() {
        when(mSessionManager.getHeadPoseInActivitySpace()).thenReturn(mPose1);

        mTracker.start();
        verify(mCallback).onResult(mPose1);

        mTracker.stop();

        // Update pose and advance looper - callback should not be invoked again
        when(mSessionManager.getHeadPoseInActivitySpace()).thenReturn(mPose2);

        ShadowLooper.idleMainLooper(CHECK_INTERVAL_MS, TimeUnit.MILLISECONDS);
        verify(mCallback, never()).onResult(mPose2);
    }
}
