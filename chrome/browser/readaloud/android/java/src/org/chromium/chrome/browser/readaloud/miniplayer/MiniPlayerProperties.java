// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.miniplayer;

import android.view.View;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Keys for Read Aloud mini player model properties. */
public class MiniPlayerProperties {
    public static final WritableObjectPropertyKey<Integer> PLAYER_STATE_KEY =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<Integer> VIEW_VISIBILITY_KEY =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<Boolean> ANIMATE_KEY =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<View.OnClickListener> ON_CLOSE_CLICK_KEY =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<View.OnClickListener> ON_EXPAND_CLICK_KEY =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<String> TITLE_KEY =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<String> PUBLISHER_KEY =
            new WritableObjectPropertyKey<>();
    public static final PropertyKey[] ALL_KEYS = {PLAYER_STATE_KEY, VIEW_VISIBILITY_KEY,
            ANIMATE_KEY, ON_CLOSE_CLICK_KEY, ON_EXPAND_CLICK_KEY, TITLE_KEY, PUBLISHER_KEY};
}
