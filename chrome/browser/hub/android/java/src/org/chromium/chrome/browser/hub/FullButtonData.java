// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Combination of display information and the event handling for pressing the button. */
@NullMarked
public interface FullButtonData extends DisplayButtonData {
    /**
     * Returns the {@link Runnable} that should be invoked when the button is pressed. If this
     * returns null the button will be disabled.
     */
    @Nullable Runnable getOnPressRunnable();

    /**
     * Compares this button data with another for equality based on display-relevant properties.
     *
     * <p>This method exists because UI frameworks need to determine if button data has changed to
     * decide whether to update the display, but {@link Runnable} instances are not comparable and
     * not relevant to visual appearance.
     *
     * <p>Implementations should make a best effort comparison. ResourceButtonData and
     * DrawableButtonData objects might reference the same visual content but may still report
     * inequality due to different underlying implementations.
     *
     * @param o The object to compare with this button data
     * @return true if the display-relevant properties are equal, false otherwise
     */
    boolean buttonDataEquals(Object o);
}
