// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.actions;

import org.chromium.build.annotations.NullMarked;

/** Specialized version of {@link FullButtonData} that includes toggle and transparent states. */
@NullMarked
public interface ActionButtonData extends FullButtonData {
    /**
     * Returns true if the button should be transparent (alpha 0) instead of View.INVISIBLE. This
     * allows the button to remain clickable even when it is not visually seen. Useful for the new
     * background tab animation where two tab switcher buttons are present but only one is seen.
     */
    default boolean isTransparent() {
        return false;
    }

    /**
     * Returns true if the button is in a toggled state. This is needed for the bottom bar, which
     * signals the user which actions are active.
     */
    default boolean isToggled() {
        return false;
    }
}
