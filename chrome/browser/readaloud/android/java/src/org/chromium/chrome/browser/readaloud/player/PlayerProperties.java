// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.player;

import org.chromium.chrome.browser.readaloud.player.mini.MiniPlayerMediator;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Keys for Read Aloud player model properties. */
public class PlayerProperties {
    // VisibilityState
    public static final WritableObjectPropertyKey<Integer> MINI_PLAYER_VISIBILITY =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<Integer> EXPANDED_PLAYER_VISIBILITY =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<Boolean> MINI_PLAYER_ANIMATE_VISIBILITY_CHANGES =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<String> TITLE = new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<String> PUBLISHER =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<Integer> PLAYBACK_STATE =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<Float> PROGRESS =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<Float> SPEED = new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<InteractionHandler> INTERACTION_HANDLER =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<MiniPlayerMediator> MINI_PLAYER_MEDIATOR =
            new WritableObjectPropertyKey<>();
    public static final PropertyKey[] ALL_KEYS = {
        MINI_PLAYER_VISIBILITY, //
        EXPANDED_PLAYER_VISIBILITY, //
        MINI_PLAYER_ANIMATE_VISIBILITY_CHANGES, //
        TITLE, //
        PUBLISHER, //
        PLAYBACK_STATE, //
        PROGRESS, //
        MINI_PLAYER_MEDIATOR, //
        SPEED, //
        INTERACTION_HANDLER //
    };
}
