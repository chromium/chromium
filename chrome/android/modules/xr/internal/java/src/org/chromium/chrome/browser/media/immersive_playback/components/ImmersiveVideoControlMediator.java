// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.immersive_playback.components;

import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.Locale;

/**
 * Mediator for the media control panel in immersive video playback. Handles state, timer for
 * seekbar polling, and user interactions.
 */
@NullMarked
public class ImmersiveVideoControlMediator
        implements ImmersiveVideoControlView.UserInteractionListener {
    private final PropertyModel mModel;
    private final ImmersiveVideoControlCoordinator.Delegate mDelegate;
    private final Handler mHandler = new Handler(Looper.getMainLooper());

    private int mDurationMs;
    private int mStartingPositionMs;
    private long mLastUpdatedTimeMs;
    private double mPlaybackRate = 1.0;
    private boolean mIsPlaying;
    private boolean mIsSeeking;

    private final Runnable mUpdateSeekbarTask =
            new Runnable() {
                @Override
                public void run() {
                    if (mPlaybackRate <= 0 || mIsSeeking) return;

                    long timeDiff = SystemClock.elapsedRealtime() - mLastUpdatedTimeMs;
                    int offset = (int) (timeDiff * mPlaybackRate);
                    int currentPosMs = mStartingPositionMs + offset;

                    mModel.set(
                            ImmersiveVideoControlProperties.PROGRESS,
                            Math.min(currentPosMs, mDurationMs));
                    mModel.set(
                            ImmersiveVideoControlProperties.POSITION_TEXT,
                            formatTime(currentPosMs / 1000));

                    mHandler.postDelayed(this, 50);
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
        mModel = model;
        mDelegate = delegate;
    }

    /**
     * Updates the media position and starts/stops the seekbar polling timer.
     *
     * @param durationMs The total duration in milliseconds.
     * @param positionMs The current position in milliseconds.
     * @param playbackRate The current playback rate.
     */
    public void updateMediaPosition(long durationMs, long positionMs, double playbackRate) {
        if (mIsSeeking) return;

        mDurationMs = (int) Math.min(durationMs, Integer.MAX_VALUE);
        mStartingPositionMs = (int) Math.min(positionMs, Integer.MAX_VALUE);
        mPlaybackRate = playbackRate;
        mLastUpdatedTimeMs = SystemClock.elapsedRealtime();

        mModel.set(ImmersiveVideoControlProperties.MAX_PROGRESS, Math.max(1, mDurationMs));
        mModel.set(ImmersiveVideoControlProperties.DURATION_TEXT, formatTime(mDurationMs / 1000));

        mHandler.removeCallbacks(mUpdateSeekbarTask);

        if (mPlaybackRate > 0 && mIsPlaying) {
            mHandler.post(mUpdateSeekbarTask);
        } else {
            mModel.set(ImmersiveVideoControlProperties.PROGRESS, mStartingPositionMs);
            mModel.set(
                    ImmersiveVideoControlProperties.POSITION_TEXT,
                    formatTime(mStartingPositionMs / 1000));
        }
    }

    /**
     * Updates the playback state and starts/stops the seekbar polling timer.
     *
     * @param isPlaying True if playing, false otherwise.
     */
    public void updatePlaybackState(boolean isPlaying) {
        mIsPlaying = isPlaying;
        mModel.set(ImmersiveVideoControlProperties.IS_PLAYING, isPlaying);

        if (isPlaying) {
            mLastUpdatedTimeMs = SystemClock.elapsedRealtime();
            mHandler.removeCallbacks(mUpdateSeekbarTask);
            mHandler.post(mUpdateSeekbarTask);
        } else {
            mHandler.removeCallbacks(mUpdateSeekbarTask);
            Integer currentProgress = mModel.get(ImmersiveVideoControlProperties.PROGRESS);
            if (currentProgress != null) {
                mStartingPositionMs = currentProgress;
            }
        }
    }

    /**
     * Sets the selected state of the format button in the model.
     *
     * @param selected True if selected, false otherwise.
     */
    public void setFormatButtonSelected(boolean selected) {
        mModel.set(ImmersiveVideoControlProperties.FORMAT_BUTTON_SELECTED, selected);
    }

    /**
     * Sets whether the control panel is movable in the model.
     *
     * @param isMovable True if movable, false otherwise.
     */
    public void setMovable(boolean isMovable) {
        mModel.set(ImmersiveVideoControlProperties.IS_MOVABLE, isMovable);
    }

    /**
     * Updates the pose translation and rotation in the model.
     *
     * @param translation The translation from the parent {@link XrSpace}.
     * @param rotation The rotation from the parent {@link XrSpace}.
     */
    public void updatePose(float[] translation, float[] rotation) {
        mModel.set(ImmersiveVideoControlProperties.POSE_TRANSLATION, translation);
        mModel.set(ImmersiveVideoControlProperties.POSE_ROTATION, rotation);
    }

    // UserInteractionListener implementation

    @Override
    public void onPlayClicked() {
        mDelegate.togglePlayPause(false);
    }

    @Override
    public void onPauseClicked() {
        mDelegate.togglePlayPause(true);
    }

    @Override
    public void onFormatClicked() {
        mDelegate.onFormatClicked();
    }

    @Override
    public void onExitFullscreenClicked() {
        mDelegate.onExitImmersivePlayback();
    }

    @Override
    public void onSeekTo(int progressMs) {
        mDelegate.seekTo(progressMs);
        mStartingPositionMs = progressMs;
        mLastUpdatedTimeMs = SystemClock.elapsedRealtime();
        mModel.set(ImmersiveVideoControlProperties.POSITION_TEXT, formatTime(progressMs / 1000));
    }

    @Override
    public void onStartTrackingTouch() {
        mIsSeeking = true;
        mHandler.removeCallbacks(mUpdateSeekbarTask);
    }

    @Override
    public void onStopTrackingTouch() {
        mIsSeeking = false;
        if (mIsPlaying && mPlaybackRate > 0) {
            mHandler.post(mUpdateSeekbarTask);
        }
    }

    private String formatTime(int seconds) {
        return String.format(Locale.US, "%02d:%02d", seconds / 60, seconds % 60);
    }
}
