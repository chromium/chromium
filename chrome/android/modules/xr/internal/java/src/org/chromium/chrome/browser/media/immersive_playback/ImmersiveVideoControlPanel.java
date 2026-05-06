// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.immersive_playback;

import android.content.Context;
import android.graphics.Color;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.view.Gravity;
import android.view.ViewGroup;
import android.widget.ImageButton;
import android.widget.LinearLayout;
import android.widget.SeekBar;
import android.widget.TextView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;

import java.util.Locale;

/** Manages the media control panel for Picture-in-Picture in an XR environment. */
@NullMarked
public class ImmersiveVideoControlPanel extends LinearLayout {
    private final ImmersiveVideoControlDelegate mVideoControlDelegate;
    private final SeekBar mSeekBar;
    private final TextView mPositionLabel;
    private final ImageButton mPlayPauseButton;
    private final Handler mHandler = new Handler(Looper.getMainLooper());
    private int mDuration;
    private int mStartingPosition;
    private long mLastUpdatedTime;
    private double mPlaybackRate = 1f;
    private boolean mIsPlaying;
    private boolean mIsSeeking;

    private final Runnable mUpdateSeekbarTask =
            new Runnable() {
                @Override
                public void run() {
                    if (mPlaybackRate <= 0) return;
                    long timeDiff = SystemClock.elapsedRealtime() - mLastUpdatedTime;
                    int offset = (int) (timeDiff * mPlaybackRate / 1000);
                    mSeekBar.setProgress(mStartingPosition + offset);
                    mHandler.postDelayed(this, 500);
                }
            };

    /**
     * @param context The context to create views in.
     * @param videoControlDelegate The manager for media actions.
     */
    public ImmersiveVideoControlPanel(
            Context context, ImmersiveVideoControlDelegate videoControlDelegate) {
        super(context);
        mVideoControlDelegate = videoControlDelegate;

        setLayoutParams(
                new ViewGroup.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
        setBackgroundColor(Color.BLACK);
        setOrientation(LinearLayout.HORIZONTAL);
        setGravity(Gravity.CENTER_VERTICAL);
        setPadding(10, 10, 10, 10);

        mPlayPauseButton = createPlayPauseButton();
        mSeekBar = createSeekBarView();
        mPositionLabel = createPositionLabel();

        addView(mPlayPauseButton);
        addView(mSeekBar);
        addView(mPositionLabel);
    }

    /**
     * Updates the seek bar with the current media position.
     *
     * @param durationMs The total duration of the media in milliseconds.
     * @param positionMs The current position of the media in milliseconds.
     * @param playbackRate The current playback rate of the media.
     */
    public void updateMediaPosition(long durationMs, long positionMs, double playbackRate) {
        if (mIsSeeking) return;

        mDuration = (int) (durationMs / 1000);
        mStartingPosition = (int) (positionMs / 1000);
        mPlaybackRate = playbackRate;
        mLastUpdatedTime = SystemClock.elapsedRealtime();

        mSeekBar.setMax(mDuration);
        mHandler.removeCallbacks(mUpdateSeekbarTask);

        if (mPlaybackRate > 0) {
            mHandler.postDelayed(mUpdateSeekbarTask, 500);
        } else {
            mSeekBar.setProgress(mStartingPosition);
        }
    }

    /**
     * Updates the playback state of the control panel.
     *
     * @param isPlaying Whether the media is currently playing.
     */
    public void updatePlaybackState(boolean isPlaying) {
        mIsPlaying = isPlaying;
        updatePlayPauseButtonState(isPlaying);
        if (!isPlaying) {
            mHandler.removeCallbacks(mUpdateSeekbarTask);
        }
    }

    public boolean isPlayingForTesting() {
        return mIsPlaying;
    }

    public SeekBar getSeekBarForTesting() {
        return mSeekBar;
    }

    private ImageButton createPlayPauseButton() {
        ImageButton playPauseBtn = new ImageButton(getContext());
        playPauseBtn.setPadding(8, 8, 8, 8);
        playPauseBtn.setBackgroundColor(Color.TRANSPARENT);
        playPauseBtn.setOnClickListener(v -> mVideoControlDelegate.togglePlayPause(mIsPlaying));
        return playPauseBtn;
    }

    private void updatePlayPauseButtonState(boolean isPlaying) {
        mPlayPauseButton.setImageResource(
                isPlaying ? R.drawable.ic_pause_white_24dp : R.drawable.ic_play_arrow_white_24dp);
        mPlayPauseButton.setContentDescription(
                getResources()
                        .getString(
                                isPlaying
                                        ? R.string.accessibility_pause
                                        : R.string.accessibility_play));
    }

    private SeekBar createSeekBarView() {
        SeekBar seekBar = new SeekBar(getContext());
        LinearLayout.LayoutParams seekParams =
                new LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1.0f);
        seekBar.setLayoutParams(seekParams);
        seekBar.setMax(0);
        seekBar.setProgress(0);
        seekBar.setOnSeekBarChangeListener(
                new SeekBar.OnSeekBarChangeListener() {
                    @Override
                    public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                        if (fromUser) {
                            mVideoControlDelegate.seekTo(progress * 1000L);
                        }
                        mPositionLabel.setText(formatTime(progress, mDuration));
                    }

                    @Override
                    public void onStartTrackingTouch(SeekBar seekBar) {
                        mIsSeeking = true;
                    }

                    @Override
                    public void onStopTrackingTouch(SeekBar seekBar) {
                        mIsSeeking = false;
                    }
                });

        return seekBar;
    }

    private TextView createPositionLabel() {
        TextView positionLabel = new TextView(getContext());
        positionLabel.setPadding(10, 0, 10, 0);
        positionLabel.setTextAppearance(R.style.TextAppearance_TextSmall);
        positionLabel.setText(formatTime(0, 0));
        return positionLabel;
    }

    private String formatTime(int currentSec, int totalSec) {
        return String.format(
                Locale.US,
                "%02d:%02d / %02d:%02d",
                currentSec / 60,
                currentSec % 60,
                totalSec / 60,
                totalSec % 60);
    }
}
