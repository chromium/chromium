// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.bottom;

/**
 * Allows querying bottom toolbar related configurations.
 */
public class BottomToolbarConfiguration {
    /**
     * @return Whether or not the bottom toolbar is enabled.
     */
    public static boolean isBottomToolbarEnabled() {
        return false;
    }

    /**
     * @return Whether or not the adaptive toolbar is enabled.
     */
    public static boolean isAdaptiveToolbarEnabled() {
        return false;
    }

    /**
     * @return Whether or not the labeled bottom toolbar is enabled.
     */
    public static boolean isLabeledBottomToolbarEnabled() {
        return false;
    }
}
