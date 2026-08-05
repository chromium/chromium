// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.immersive_playback.components;

import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;

import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.xr.scenecore.XrPose;

import java.util.Locale;

/**
 * Mediator for the media control panel in immersive video playback. Handles state, timer for
 * seekbar polling, and user interactions.
 */
@NullMarked
public class ImmersiveVideoControlMediator
        implements ImmersiveVideoControlView.UserInteractionListener {
    private static final long SEEKBAR_UPDATE_INTERVAL_MS = 50L;

    private final PropertyModel mModel;
    private final ImmersiveVideoControlCoordinator.Delegate mDelegate;
    private final Handler mHandler;

    private int mDurationMs;
    private int mStartingPositionMs;
    private long mLastUpdatedTimeMs;
    private double mPlaybackRate = 1.0;
    private boolean mIsPlaying;
    private boolean mIsSeeking;
    private boolean mIsVisible;
    private boolean mIsDestroyed;

    private final Runnable mUpdateSeekbarTask =
            new Runnable() {
                @Override
                public void run() {
                    if (!shouldScheduleSeekbarUpdate()) return;

                    updateDisplayedPosition();
                    mHandler.postDelayed(this, SEEKBAR_UPDATE_INTERVAL_MS);
                }
            };

    /**
     * Creates a new {@link ImmersiveVideoControlMediator}.
     *
     * @param model The {@link PropertyModel} to update.
     * @param delegate The {@link ImmersiveVideoControlCoordinator.Delegate} to handle user
     *     interactions.
     */
    public ImmersiveVideoControlMediator(
            PropertyModel model, ImmersiveVideoControlCoordinator.Delegate delegate) {
        this(model, delegate, new Handler(Looper.getMainLooper()));
    }

    @VisibleForTesting
    ImmersiveVideoControlMediator(
            PropertyModel model,
            ImmersiveVideoControlCoordinator.Delegate delegate,
            Handler handler) {
        mModel = model;
        mDelegate = delegate;
        mHandler = handler;
    }

    /**
     * Updates the media position and starts/stops the seekbar polling timer.
     *
     * @param durationMs The total duration in milliseconds.
     * @param positionMs The current position in milliseconds.
     * @param playbackRate The current playback rate.
     */
    public void updateMediaPosition(long durationMs, long positionMs, double playbackRate) {
        if (mIsDestroyed) return;

        int updatedDurationMs = (int) Math.max(0, Math.min(durationMs, Integer.MAX_VALUE));
        int updatedStartingPositionMs;
        if (mIsSeeking) {
            // Preserve elapsed seek time using the previous playback anchor and rate.
            updatedStartingPositionMs = getCurrentPositionMs();
        } else {
            updatedStartingPositionMs =
                    (int) Math.max(0, Math.min(positionMs, updatedDurationMs));
        }

        mDurationMs = updatedDurationMs;
        mPlaybackRate = playbackRate;
        mStartingPositionMs = updatedStartingPositionMs;
        mLastUpdatedTimeMs = SystemClock.elapsedRealtime();

        if (mIsVisible && !mIsSeeking) {
            updateTimingProperties();
        }
        updateSeekbarTimer();
    }

    /**
     * Updates the playback state and starts/stops the seekbar polling timer.
     *
     * @param isPlaying True if playing, false otherwise.
     */
    public void updatePlaybackState(boolean isPlaying) {
        if (mIsDestroyed) return;

        mStartingPositionMs = getCurrentPositionMs();
        mLastUpdatedTimeMs = SystemClock.elapsedRealtime();
        mIsPlaying = isPlaying;

        if (mIsVisible) {
            mModel.set(ImmersiveVideoControlProperties.IS_PLAYING, isPlaying);
            if (!mIsSeeking) {
                updateDisplayedPosition();
            }
        }
        updateSeekbarTimer();
    }

    /** Sets whether the control panel is visible. */
    public void setVisible(boolean visible) {
        if (mIsDestroyed || mIsVisible == visible) return;

        mIsVisible = visible;
        if (visible) {
            mModel.set(ImmersiveVideoControlProperties.IS_PLAYING, mIsPlaying);
            updateTimingProperties();
        } else {
            mIsSeeking = false;
        }
        updateSeekbarTimer();
    }

    /** Stops all callbacks and releases this mediator from further lifecycle work. */
    public void destroy() {
        if (mIsDestroyed) return;

        mIsDestroyed = true;
        mIsVisible = false;
        mHandler.removeCallbacks(mUpdateSeekbarTask);
    }

    /**
     * Sets the selected state of the format button in the model.
     *
     * @param selected True if selected, false otherwise.
     */
    public void setFormatButtonSelected(boolean selected) {
        if (mIsDestroyed) return;
        mModel.set(ImmersiveVideoControlProperties.FORMAT_BUTTON_SELECTED, selected);
    }

    /**
     * Sets whether the control panel is movable in the model.
     *
     * @param isMovable True if movable, false otherwise.
     */
    public void setMovable(boolean isMovable) {
        if (mIsDestroyed) return;
        mModel.set(ImmersiveVideoControlProperties.IS_MOVABLE, isMovable);
    }

    /**
     * Updates the pose translation and rotation in the model.
     *
     * @param pose The pose from the parent {@link XrSpace}.
     */
    public void updatePose(XrPose pose) {
        if (mIsDestroyed) return;
        mModel.set(ImmersiveVideoControlProperties.POSE, pose);
    }

    // UserInteractionListener implementation

    @Override
    public void onPlayClicked() {
        if (mIsDestroyed) return;
        mDelegate.togglePlayPause(false);
    }

    @Override
    public void onPauseClicked() {
        if (mIsDestroyed) return;
        mDelegate.togglePlayPause(true);
    }

    @Override
    public void onFormatClicked() {
        if (mIsDestroyed) return;
        mDelegate.onFormatClicked();
    }

    @Override
    public void onExitFullscreenClicked() {
        if (mIsDestroyed) return;
        mDelegate.onExitImmersivePlayback();
    }

    @Override
    public void onSeekTo(int progressMs) {
        if (mIsDestroyed) return;

        int clampedProgressMs = Math.max(0, Math.min(progressMs, mDurationMs));
        mDelegate.seekTo(clampedProgressMs);
        mStartingPositionMs = clampedProgressMs;
        mLastUpdatedTimeMs = SystemClock.elapsedRealtime();
        if (mIsVisible) {
            updateDisplayedPosition();
        }
    }

    @Override
    public void onStartTrackingTouch() {
        if (mIsDestroyed) return;

        mStartingPositionMs = getCurrentPositionMs();
        mLastUpdatedTimeMs = SystemClock.elapsedRealtime();
        mIsSeeking = true;
        updateSeekbarTimer();
    }

    @Override
    public void onStopTrackingTouch() {
        if (mIsDestroyed || !mIsSeeking) return;

        mStartingPositionMs = getCurrentPositionMs();
        mIsSeeking = false;
        mLastUpdatedTimeMs = SystemClock.elapsedRealtime();
        if (mIsVisible) {
            updateTimingProperties();
        }
        updateSeekbarTimer();
    }

    private boolean shouldScheduleSeekbarUpdate() {
        return !mIsDestroyed
                && mIsVisible
                && mIsPlaying
                && mPlaybackRate > 0
                && !mIsSeeking;
    }

    private void updateSeekbarTimer() {
        mHandler.removeCallbacks(mUpdateSeekbarTask);
        if (shouldScheduleSeekbarUpdate()) {
            mHandler.post(mUpdateSeekbarTask);
        }
    }

    private void updateDisplayedPosition() {
        int currentPositionMs = getCurrentPositionMs();
        mModel.set(ImmersiveVideoControlProperties.PROGRESS, currentPositionMs);
        mModel.set(
                ImmersiveVideoControlProperties.POSITION_TEXT,
                formatTime(currentPositionMs / 1000));
    }

    private void updateTimingProperties() {
        int maxProgress = Math.max(1, mDurationMs);
        Integer previousMaxProgress = mModel.get(ImmersiveVideoControlProperties.MAX_PROGRESS);
        boolean isGrowing = previousMaxProgress == null || maxProgress >= previousMaxProgress;

        if (isGrowing) {
            mModel.set(ImmersiveVideoControlProperties.MAX_PROGRESS, maxProgress);
        }
        updateDisplayedPosition();
        if (!isGrowing) {
            mModel.set(ImmersiveVideoControlProperties.MAX_PROGRESS, maxProgress);
        }
        mModel.set(ImmersiveVideoControlProperties.DURATION_TEXT, formatTime(mDurationMs / 1000));
    }

    private int getCurrentPositionMs() {
        long currentPositionMs = mStartingPositionMs;
        if (mIsPlaying && mPlaybackRate > 0) {
            long elapsedTimeMs = SystemClock.elapsedRealtime() - mLastUpdatedTimeMs;
            currentPositionMs += (long) (elapsedTimeMs * mPlaybackRate);
        }
        return (int) Math.max(0, Math.min(currentPositionMs, mDurationMs));
    }

    @VisibleForTesting
    boolean hasPendingSeekbarUpdateForTesting() {
        return mHandler.hasCallbacks(mUpdateSeekbarTask);
    }

    private String formatTime(int seconds) {
        return String.format(Locale.US, "%02d:%02d", seconds / 60, seconds % 60);
    }
}
