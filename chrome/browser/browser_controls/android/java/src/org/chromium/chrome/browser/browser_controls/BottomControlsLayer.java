// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browser_controls;

import org.chromium.chrome.browser.browser_controls.BottomControlsStacker.LayerScrollBehavior;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker.LayerType;

/** Interface represented in the bottom controls stack. */
public interface BottomControlsLayer {
    /** Return the type of the layer. This should not change once the layer is created. */
    @LayerType
    int getType();

    /**
     * Return the behavior when browser controls are scrolling. Can change after the layer is
     * created; call {@link BottomControlsStacker#requestLayerUpdate} to trigger an update.
     */
    @LayerScrollBehavior
    int getScrollBehavior();

    /**
     * Return the current height of the layer. Can change after the layer is created; call {@link
     * BottomControlsStacker#requestLayerUpdate} to trigger an update.
     */
    int getHeight();

    /**
     * Whether the layer is visible in the UI. Can change after the layer is created; call {@link
     * BottomControlsStacker#requestLayerUpdate} to trigger an update.
     */
    boolean isVisible();
}
