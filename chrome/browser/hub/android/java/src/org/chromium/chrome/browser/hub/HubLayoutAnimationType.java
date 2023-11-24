// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** The type of animation the {@link HubLayout} is playing. */
@IntDef({
    HubLayoutAnimationType.NONE,
    HubLayoutAnimationType.FADE_IN,
    HubLayoutAnimationType.FADE_OUT,
    HubLayoutAnimationType.SHRINK_TAB,
    HubLayoutAnimationType.EXPAND_TAB,
    HubLayoutAnimationType.TRANSLATE_UP,
    HubLayoutAnimationType.TRANSLATE_DOWN,
    HubLayoutAnimationType.EXPAND_NEW_TAB,
    HubLayoutAnimationType.COUNT
})
@Retention(RetentionPolicy.SOURCE)
public @interface HubLayoutAnimationType {
    /** Show the Hub immediately with no transition. */
    int NONE = 0;

    /** Show the Hub via a crossfade. */
    int FADE_IN = 1;

    /** Hide the Hub via a crossfade out. */
    int FADE_OUT = 2;

    /** Shrink from a tab to the tab switcher pane. */
    int SHRINK_TAB = 3;

    /** Expand from a tab switcher tab card out to the current tab. */
    int EXPAND_TAB = 4;

    /** Translate the entire Hub Android view upward. */
    int TRANSLATE_UP = 5;

    /** Translate the entire Hub Android view downward. */
    int TRANSLATE_DOWN = 6;

    /** Hide by playing an animation expanding to show a new Tab. */
    int EXPAND_NEW_TAB = 7;

    /** Must be last. */
    int COUNT = 8;
}
