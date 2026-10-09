// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ttc;

import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableFloatPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;

/** Properties for the floating TTC session pill. */
@NullMarked
class TtcSessionProperties {
    /** Whether the pill is shown. True whenever a session is in progress. */
    static final WritableBooleanPropertyKey VISIBLE = new WritableBooleanPropertyKey();

    /** String resource describing the session state (connecting, listening, error). */
    static final WritableIntPropertyKey STATUS_TEXT_RES_ID = new WritableIntPropertyKey();

    /** The user's microphone level in [0, 1], used to animate the microphone icon. */
    static final WritableFloatPropertyKey AUDIO_LEVEL = new WritableFloatPropertyKey();

    /** Click listener for the pill. Tapping the pill ends the session. */
    static final ReadableObjectPropertyKey<View.OnClickListener> ON_CLICK_LISTENER =
            new ReadableObjectPropertyKey<>();

    static final PropertyKey[] ALL_KEYS = {
        VISIBLE, STATUS_TEXT_RES_ID, AUDIO_LEVEL, ON_CLICK_LISTENER
    };
}
