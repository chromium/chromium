// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ttc;

import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableFloatPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Properties for the TTC session UI: the phone pill, or the voice plate on large screens. */
@NullMarked
class TtcSessionProperties {
    /** Whether the pill is shown. True whenever a session is in progress. */
    static final WritableBooleanPropertyKey VISIBLE = new WritableBooleanPropertyKey();

    /** String resource describing the session state (connecting, listening, error). */
    static final WritableIntPropertyKey STATUS_TEXT_RES_ID = new WritableIntPropertyKey();

    /** The user's microphone level in [0, 1], used to animate the microphone icon. */
    static final WritableFloatPropertyKey AUDIO_LEVEL = new WritableFloatPropertyKey();

    /** Click listener for the pill, set by the mediator. Tapping the pill ends the session. */
    static final WritableObjectPropertyKey<View.OnClickListener> ON_CLICK_LISTENER =
            new WritableObjectPropertyKey<>();

    /** Whether the session's microphone is muted. Only the voice plate shows it. */
    static final WritableBooleanPropertyKey MIC_MUTED = new WritableBooleanPropertyKey();

    /**
     * Click listener for the voice plate's pause button, set by the mediator. It mutes the
     * microphone, or unmutes it while muted.
     */
    static final WritableObjectPropertyKey<View.OnClickListener> ON_PAUSE_CLICK_LISTENER =
            new WritableObjectPropertyKey<>();

    /**
     * Whether the voice plate's pause button can be used: only while the session is live, since the
     * microphone can't be muted before.
     */
    static final WritableBooleanPropertyKey PAUSE_ENABLED = new WritableBooleanPropertyKey();

    /**
     * The {@link TtcWaveformView.Mode} of the voice plate's waveform: bars following {@link
     * #AUDIO_LEVEL} while the session is listening, resting dots otherwise.
     */
    static final WritableIntPropertyKey WAVEFORM_MODE = new WritableIntPropertyKey();

    static final PropertyKey[] ALL_KEYS = {
        VISIBLE,
        STATUS_TEXT_RES_ID,
        AUDIO_LEVEL,
        ON_CLICK_LISTENER,
        MIC_MUTED,
        ON_PAUSE_CLICK_LISTENER,
        PAUSE_ENABLED,
        WAVEFORM_MODE
    };
}
