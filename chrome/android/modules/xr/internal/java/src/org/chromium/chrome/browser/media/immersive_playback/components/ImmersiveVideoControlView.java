// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.immersive_playback.components;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageButton;
import android.widget.TextView;

import com.google.android.material.slider.Slider;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.modules.xr.R;

/**
 * Passive view for the media control panel in an XR environment. Forwards user interactions to a
 * listener and exposes methods to update UI state.
 */
@NullMarked
public class ImmersiveVideoControlView extends ImmersiveVideoHoverLayout {
    private static final String DEFAULT_TIME_TEXT = "--:--";

    /** Listener for user interactions on the control panel. */
    public interface UserInteractionListener {
        /** Called when the play button is clicked. */
        void onPlayClicked();

        /** Called when the pause button is clicked. */
        void onPauseClicked();

        /** Called when the format button is clicked. */
        void onFormatClicked();

        /** Called when the exit button is clicked. */
        void onExitFullscreenClicked();

        /**
         * Called when the user seeks to a specific position.
         *
         * @param progressMs The position to seek to, in milliseconds.
         */
        void onSeekTo(int progressMs);

        /** Called when the user starts dragging the seek bar. */
        void onStartTrackingTouch();

        /** Called when the user stops dragging the seek bar. */
        void onStopTrackingTouch();
    }

    private final Slider mSeekBar;
    private final TextView mPositionLabel;
    private final TextView mDurationLabel;
    private final ImageButton mPlayButton;
    private final ImageButton mPauseButton;
    private final ImageButton mFormatButton;
    private final ImageButton mExitButton;

    /**
     * Creates a new {@link ImmersiveVideoControlView}.
     *
     * @param context The {@link Context}.
     * @param listener The {@link UserInteractionListener}.
     */
    public ImmersiveVideoControlView(Context context, UserInteractionListener listener) {
        super(context);

        LayoutInflater.from(context).inflate(R.layout.immersive_video_control_view, this, true);

        mPlayButton = findViewById(R.id.play_button);
        mPauseButton = findViewById(R.id.pause_button);
        mPositionLabel = findViewById(R.id.position_label);
        mSeekBar = findViewById(R.id.seek_bar);
        mDurationLabel = findViewById(R.id.duration_label);
        mFormatButton = findViewById(R.id.format_button);
        mExitButton = findViewById(R.id.exit_button);

        mPlayButton.setOnClickListener(v -> listener.onPlayClicked());
        mPauseButton.setOnClickListener(v -> listener.onPauseClicked());
        mFormatButton.setOnClickListener(v -> listener.onFormatClicked());
        mExitButton.setOnClickListener(v -> listener.onExitFullscreenClicked());

        mPlayButton.setContentDescription(
                context.getString(org.chromium.chrome.R.string.accessibility_play));
        mPauseButton.setContentDescription(
                context.getString(org.chromium.chrome.R.string.accessibility_pause));

        mPositionLabel.setText(DEFAULT_TIME_TEXT);
        mDurationLabel.setText(DEFAULT_TIME_TEXT);

        mSeekBar.addOnChangeListener(
                (slider, value, fromUser) -> {
                    if (fromUser) {
                        listener.onSeekTo((int) value);
                    }
                });

        mSeekBar.addOnSliderTouchListener(
                new Slider.OnSliderTouchListener() {
                    @Override
                    public void onStartTrackingTouch(Slider slider) {
                        listener.onStartTrackingTouch();
                    }

                    @Override
                    public void onStopTrackingTouch(Slider slider) {
                        listener.onStopTrackingTouch();
                    }
                });
    }

    /** Sets the text for the current position label. */
    public void setPositionText(String text) {
        mPositionLabel.setText(text);
    }

    /** Sets the text for the total duration label. */
    public void setDurationText(String text) {
        mDurationLabel.setText(text);
    }

    /** Sets the current progress on the seek bar. */
    public void setProgress(int progress) {
        mSeekBar.setValue(Math.min(progress, mSeekBar.getValueTo()));
    }

    /** Sets the maximum value for the seek bar. */
    public void setMaxProgress(int maxProgress) {
        mSeekBar.setValueTo(Math.max(1f, maxProgress));
    }

    /**
     * Updates the visibility of play/pause buttons based on playback state.
     *
     * @param isPlaying True if playing, false otherwise.
     */
    public void setPlaybackState(boolean isPlaying) {
        if (isPlaying) {
            mPlayButton.setVisibility(View.GONE);
            mPauseButton.setVisibility(View.VISIBLE);
        } else {
            mPlayButton.setVisibility(View.VISIBLE);
            mPauseButton.setVisibility(View.GONE);
        }
    }

    /** Sets the selected state of the format button. */
    public void setFormatButtonSelected(boolean selected) {
        mFormatButton.setSelected(selected);
    }

    public Slider getSeekBarForTesting() {
        return mSeekBar;
    }

    public boolean isPlayingForTesting() {
        return mPauseButton.getVisibility() == View.VISIBLE;
    }

    public boolean isFormatButtonSelectedForTesting() {
        return mFormatButton.isSelected();
    }
}
