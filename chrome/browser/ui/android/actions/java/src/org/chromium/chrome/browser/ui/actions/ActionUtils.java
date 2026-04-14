// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.actions;

import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ui.actions.button.ButtonState;

/** Helper methods for working with Action buttons. */
@NullMarked
public class ActionUtils {
    /**
     * Applies the given {@link ButtonState} to the view. This is currently unrelated to the view's
     * visibility state, and only affects the view's alpha, clickable, and enabled states.
     *
     * @param view The view to apply the state to.
     * @param state The {@link ButtonState} to apply.
     * @param canPress Whether the button can be pressed.
     */
    public static void applyButtonState(View view, @ButtonState int state, boolean canPress) {
        if (state == ButtonState.DEFAULT) {
            view.setAlpha(1.0f);
            view.setClickable(canPress);
            view.setEnabled(canPress);
        } else if (state == ButtonState.INVISIBLE_AND_CLICKABLE) {
            view.setAlpha(0.0f);
            view.setClickable(true);
            view.setEnabled(true);
        } else if (state == ButtonState.UNCLICKABLE) {
            view.setAlpha(1.0f);
            view.setClickable(false);
            view.setEnabled(true);
        }
    }
}
