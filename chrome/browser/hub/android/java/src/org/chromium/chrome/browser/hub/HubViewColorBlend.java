// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import android.animation.Animator;

import androidx.annotation.ColorInt;

import org.chromium.build.annotations.NullMarked;

/** Helper class to manage Animator object creation for views during a hub color scheme change. */
@NullMarked
public interface HubViewColorBlend {

    interface ColorGetter {
        @ColorInt
        int colorIntFromColorScheme(@HubColorScheme int hubColorScheme);
    }

    interface ColorSetter {
        void setColorInt(@ColorInt int colorInt);
    }

    /** Creates a color blend animation for a hub color scheme change. */
    Animator createAnimationForTransition(
            @HubColorScheme int startScheme, @HubColorScheme int endScheme);
}
