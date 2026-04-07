// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.actions.button;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/** Defines the interactive and visual state of an {@link ActionButtonData}. */
@IntDef({ButtonState.DEFAULT, ButtonState.INVISIBLE_AND_CLICKABLE, ButtonState.UNCLICKABLE})
@Retention(RetentionPolicy.SOURCE)
@Target(ElementType.TYPE_USE)
@NullMarked
public @interface ButtonState {
    /** Default behavior: visible, and enabled/disabled based on callbacks. */
    int DEFAULT = 0;

    /**
     * Invisible but still clickable. Useful for the new background tab animation where two tab
     * switcher buttons are present but only one is seen.
     */
    int INVISIBLE_AND_CLICKABLE = 1;

    /**
     * Visible but unclickable. Prevents interaction during layout transitions without triggering
     * the visual "greyed-out" disabled blink.
     */
    int UNCLICKABLE = 2;
}
