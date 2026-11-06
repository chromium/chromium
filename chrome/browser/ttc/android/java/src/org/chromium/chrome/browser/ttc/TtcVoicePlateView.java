// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ttc;

import android.content.res.Resources;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageButton;

import androidx.annotation.StringRes;
import androidx.core.view.ViewCompat;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.widget.floating_pill.FloatingPillContainer;
import org.chromium.components.browser_ui.widget.floating_pill.FloatingPillView;

/**
 * The large-screen TTC voice plate: a floating pill with the session's waveform and a button to
 * pause (mute) the microphone. It's a {@link FloatingPillView} in a {@link FloatingPillContainer},
 * like auto browse's "Take over task" button. Its owner places it on the bottom edge of the
 * toolbar.
 */
@NullMarked
class TtcVoicePlateView {
    private final ViewGroup mRoot;
    private final FloatingPillView mPill;
    private final View mWaveform;
    private final ImageButton mPauseButton;

    /**
     * Inflates the voice plate and adds it to {@code parent}, hidden.
     *
     * @param parent The view to add the plate to, which should cover the activity's content area.
     *     The plate is added as its last child, so {@code parent} should be where that draws it
     *     over the toolbar and the page but under the bottom controls, sheets and the tab switcher.
     */
    static TtcVoicePlateView create(ViewGroup parent) {
        ViewGroup root =
                (ViewGroup)
                        LayoutInflater.from(parent.getContext())
                                .inflate(
                                        R.layout.ttc_voice_plate,
                                        parent,
                                        /* attachToRoot= */ false);
        parent.addView(root);
        return new TtcVoicePlateView(root);
    }

    private TtcVoicePlateView(ViewGroup root) {
        Resources res = root.getResources();

        mRoot = root;
        mPill = root.findViewById(R.id.ttc_voice_plate);
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

    void setVisible(boolean visible) {
        mRoot.setVisibility(visible ? View.VISIBLE : View.GONE);
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

    void setOnPauseClickListener(View.@Nullable OnClickListener listener) {
        mPauseButton.setOnClickListener(listener);
    }

    /** Removes the plate from its parent. */
    void destroy() {
        ViewGroup parent = (ViewGroup) mRoot.getParent();
        if (parent != null) parent.removeView(mRoot);
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
