// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ttc;

import android.content.Context;
import android.content.res.Resources;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageButton;

import androidx.annotation.StringRes;
import androidx.core.content.res.ResourcesCompat;
import androidx.core.view.ViewCompat;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ttc.TtcWaveformView.Mode;
import org.chromium.components.browser_ui.widget.floating_pill.FloatingPillView;

/**
 * The large-screen TTC voice plate: a floating pill with the session's waveform and a button to
 * pause (mute) the microphone. It's a {@link FloatingPillView}, which its owner shows in the
 * window's floating pill container on the bottom edge of the toolbar, next to e.g. auto browse's
 * "Take over task" button.
 */
@NullMarked
class TtcVoicePlateView {
    private final FloatingPillView mPill;
    private final TtcWaveformView mWaveform;
    private final ImageButton mPauseButton;

    /** Inflates the voice plate, hidden and without a parent. */
    static TtcVoicePlateView create(Context context) {
        return new TtcVoicePlateView(
                (FloatingPillView)
                        LayoutInflater.from(context).inflate(R.layout.ttc_voice_plate, null));
    }

    private TtcVoicePlateView(FloatingPillView pill) {
        Resources res = pill.getResources();

        mPill = pill;
        mWaveform = mPill.findViewById(R.id.ttc_voice_plate_waveform);
        String title = res.getString(R.string.ttc_voice_plate_title);
        // Announced when the plate appears or goes away.
        ViewCompat.setAccessibilityPaneTitle(mPill, title);
        // Lets screen readers focus the waveform, which shows the status, and read the status,
        // which is announced as it changes, see setStatusText(). Only the waveform is a live
        // region, so changes to the pause button aren't announced as status changes.
        mWaveform.setContentDescription(title);
        ViewCompat.setScreenReaderFocusable(mWaveform, true);
        ViewCompat.setAccessibilityLiveRegion(
                mWaveform, ViewCompat.ACCESSIBILITY_LIVE_REGION_POLITE);

        mPauseButton = mPill.findViewById(R.id.ttc_voice_plate_pause_button);
        setMicMuted(false);
    }

    /** Returns the plate's view, to add it to the window's floating pill container. */
    View getView() {
        return mPill;
    }

    void setVisible(boolean visible) {
        mPill.setVisibility(visible ? View.VISIBLE : View.GONE);
    }

    /** Sets whether the waveform shows bars that follow the microphone level, or resting dots. */
    void setWaveformMode(@Mode int mode) {
        mWaveform.setMode(mode);
    }

    /** Reports the microphone level, in [0, 1]. Ignored while the waveform is resting. */
    void setAudioLevel(float level) {
        mWaveform.setAudioLevel(level);
    }

    /**
     * Sets the session status (e.g. connecting or listening) read out by accessibility services,
     * which the plate otherwise only conveys visually.
     */
    void setStatusText(@StringRes int statusResId) {
        ViewCompat.setStateDescription(mWaveform, mWaveform.getResources().getString(statusResId));
    }

    /** Shows the pause button as a resume button while the microphone is muted. */
    void setMicMuted(boolean muted) {
        mPauseButton.setImageResource(
                muted ? R.drawable.ic_play_arrow_white_24dp : R.drawable.ic_pause_white_24dp);
        String description =
                mPauseButton
                        .getResources()
                        .getString(
                                muted
                                        ? R.string.ttc_voice_plate_resume
                                        : R.string.ttc_voice_plate_pause);
        mPauseButton.setContentDescription(description);
        mPauseButton.setTooltipText(description);
    }

    /** Enables the pause button, or disables and dims it, e.g. until muting works. */
    void setPauseEnabled(boolean enabled) {
        mPauseButton.setEnabled(enabled);
        mPauseButton.setAlpha(
                enabled
                        ? 1f
                        : ResourcesCompat.getFloat(
                                mPauseButton.getResources(),
                                org.chromium.ui.R.dimen.default_disabled_alpha));
    }

    void setOnPauseClickListener(View.@Nullable OnClickListener listener) {
        mPauseButton.setOnClickListener(listener);
    }

    FloatingPillView getPillForTesting() {
        return mPill;
    }

    View getWaveformForTesting() {
        return mWaveform;
    }

    ImageButton getPauseButtonForTesting() {
        return mPauseButton;
    }
}
