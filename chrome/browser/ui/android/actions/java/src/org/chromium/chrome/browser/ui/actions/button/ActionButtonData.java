// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.actions.button;

import org.chromium.build.annotations.NullMarked;

/** Specialized version of {@link FullButtonData} that includes toggle and other visual states. */
@NullMarked
public interface ActionButtonData extends FullButtonData {
    @Override
    default boolean canPress() {
        return getOnPress() != null && getButtonState() != ButtonState.UNCLICKABLE;
    }

    /**
     * Returns true if the button is in a toggled state. This is needed for the bottom bar, which
     * signals the user which actions are active.
     */
    default boolean isToggled() {
        return false;
    }

    /** Returns the {@link ButtonState} of the button. */
    default @ButtonState int getButtonState() {
        return ButtonState.DEFAULT;
    }

    /**
     * Helper to determine whether the button should be visually enabled. True if there is a
     * Callback, or if the state is UNCLICKABLE or INVISIBLE_AND_CLICKABLE (which keeps it visually
     * enabled/unchanged).
     */
    default boolean isEnabled() {
        return canPress() || getButtonState() != ButtonState.DEFAULT;
    }
}
