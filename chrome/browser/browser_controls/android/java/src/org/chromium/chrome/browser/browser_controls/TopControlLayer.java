// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browser_controls;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.browser_controls.TopControlsStacker.TopControlType;
import org.chromium.chrome.browser.browser_controls.TopControlsStacker.TopControlVisibility;

/** Interface definition for any View registering itself as a top control. */
@NullMarked
public interface TopControlLayer {
    /** Return the type of the layer. This should not change once the layer is created. */
    @TopControlType
    int getTopControlType();

    /** Return the current height of the layer. */
    int getTopControlHeight();

    /** Whether the layer is visible in the UI. */
    @TopControlVisibility
    int getTopControlVisibility();

    /**
     * Return true if the layer should contribute to the total height, which a view may not if it
     * draws over other views, for example the progress bar. Returns true by default since most of
     * the Top Controls will always contribute to the total height.
     */
    default boolean contributesToTotalHeight() {
        return true;
    }
}
