// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.player;

import org.chromium.chrome.modules.readaloud.PlaybackArgs.PlaybackVoice;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableFloatPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableLongPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.util.List;

/** Keys for Read Aloud player model properties. */
public class PlayerProperties {
    public static final WritableIntPropertyKey EXPANDED_PLAYER_VISIBILITY =
            new WritableIntPropertyKey();
    public static final WritableObjectPropertyKey<String> TITLE = new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<String> PUBLISHER =
            new WritableObjectPropertyKey<>();
    public static final WritableIntPropertyKey PLAYBACK_STATE = new WritableIntPropertyKey();
    public static final WritableFloatPropertyKey PROGRESS = new WritableFloatPropertyKey();
    public static final WritableFloatPropertyKey SPEED = new WritableFloatPropertyKey();
    public static final WritableLongPropertyKey ELAPSED_NANOS = new WritableLongPropertyKey();
    public static final WritableLongPropertyKey DURATION_NANOS = new WritableLongPropertyKey();
    public static final WritableObjectPropertyKey<InteractionHandler> INTERACTION_HANDLER =
            new WritableObjectPropertyKey<>();
    public static final WritableBooleanPropertyKey HIGHLIGHTING_SUPPORTED =
            new WritableBooleanPropertyKey();
    public static final WritableBooleanPropertyKey HIGHLIGHTING_ENABLED =
            new WritableBooleanPropertyKey();
    public static final WritableObjectPropertyKey<List<PlaybackVoice>> VOICES_LIST =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<String> SELECTED_VOICE_ID =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<String> PREVIEWING_VOICE_ID =
            new WritableObjectPropertyKey<>();
    public static final WritableIntPropertyKey VOICE_PREVIEW_PLAYBACK_STATE =
            new WritableIntPropertyKey();
    public static final WritableBooleanPropertyKey SHOW_MINI_PLAYER_ON_DISMISS =
            new WritableBooleanPropertyKey();
    public static final WritableBooleanPropertyKey HIDDEN_AND_PLAYING =
            new WritableBooleanPropertyKey();
    public static final WritableBooleanPropertyKey RESTORABLE_PLAYBACK =
            new WritableBooleanPropertyKey();
    public static final PropertyKey[] ALL_KEYS = {
        EXPANDED_PLAYER_VISIBILITY,
        TITLE,
        PUBLISHER,
        PLAYBACK_STATE,
        PROGRESS,
        SPEED,
        ELAPSED_NANOS,
        DURATION_NANOS,
        INTERACTION_HANDLER,
        HIGHLIGHTING_SUPPORTED,
        HIGHLIGHTING_ENABLED,
        VOICES_LIST,
        SELECTED_VOICE_ID,
        PREVIEWING_VOICE_ID,
        VOICE_PREVIEW_PLAYBACK_STATE,
        SHOW_MINI_PLAYER_ON_DISMISS,
        HIDDEN_AND_PLAYING,
        RESTORABLE_PLAYBACK,
    };
}
