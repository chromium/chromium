// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.player;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Keys for Read Aloud player model properties. */
public class PlayerProperties {
    public static final WritableObjectPropertyKey<Integer> EXPANDED_PLAYER_VISIBILITY =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<String> TITLE = new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<String> PUBLISHER =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<Integer> PLAYBACK_STATE =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<Float> PROGRESS =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<Float> SPEED = new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<Long> ELAPSED_NANOS =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<Long> DURATION_NANOS =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<InteractionHandler> INTERACTION_HANDLER =
            new WritableObjectPropertyKey<>();
    public static final PropertyKey[] ALL_KEYS = {
        EXPANDED_PLAYER_VISIBILITY, //
        TITLE, //
        PUBLISHER, //
        PLAYBACK_STATE, //
        PROGRESS, //
        SPEED, //
        ELAPSED_NANOS, //
        DURATION_NANOS, //
        INTERACTION_HANDLER //
    };
}
