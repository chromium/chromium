// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xr.scenecore;

import android.os.Handler;
import android.os.Looper;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.xr.scenecore.XrPose;
import org.chromium.ui.xr.scenecore.XrSceneCoreSessionManager;

import java.util.Objects;

/** Tracks the user's head pose in real time and triggers a callback when head pose changes. */
@NullMarked
class XrHeadPoseTracker {
    private static final long DEFAULT_CHECK_INTERVAL_MS = 16L;

    private final XrSceneCoreSessionManager mSessionManager;
    private final Callback<XrPose> mOnHeadPoseChangedCallback;
    private final long mCheckIntervalMs;
    private final Handler mHandler = new Handler(Looper.getMainLooper());
    private final Runnable mCheckRunnable = this::checkHeadPose;
    private boolean mIsPolling;
    private @Nullable XrPose mLastPose;

    public XrHeadPoseTracker(
            XrSceneCoreSessionManager sessionManager, Callback<XrPose> onHeadPoseChangedCallback) {
        this(sessionManager, onHeadPoseChangedCallback, DEFAULT_CHECK_INTERVAL_MS);
    }

    public XrHeadPoseTracker(
            XrSceneCoreSessionManager sessionManager,
            Callback<XrPose> onHeadPoseChangedCallback,
            long checkIntervalMs) {
        mSessionManager = sessionManager;
        mOnHeadPoseChangedCallback = onHeadPoseChangedCallback;
        mCheckIntervalMs = checkIntervalMs;
    }

    /**
     * Starts polling head pose.
     *
     * @return True if polling was started or is already running, false if head tracking is
     *     disabled.
     */
    public boolean start() {
        if (mIsPolling) return true;
        if (!mSessionManager.isHeadTrackingEnabled()) {
            return false;
        }
        mIsPolling = true;
        mLastPose = null;
        checkHeadPose();
        return true;
    }

    /** Stops polling head pose. */
    public void stop() {
        if (!mIsPolling) return;
        mIsPolling = false;
        mHandler.removeCallbacks(mCheckRunnable);
    }

    private void checkHeadPose() {
        if (!mIsPolling) return;
        XrPose pose = mSessionManager.getHeadPoseInActivitySpace();
        if (pose == null) {
            pose = mLastPose != null ? mLastPose : XrPose.getIdentity();
        }

        if (!Objects.equals(mLastPose, pose)) {
            mLastPose = pose;
            mOnHeadPoseChangedCallback.onResult(pose);
        }

        mHandler.postDelayed(mCheckRunnable, mCheckIntervalMs);
    }
}
