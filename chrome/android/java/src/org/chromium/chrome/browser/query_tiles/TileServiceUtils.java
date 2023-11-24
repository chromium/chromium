// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.query_tiles;

import org.jni_zero.CalledByNative;

import org.chromium.chrome.browser.AppHooks;

/** Class for providing helper method for corresponding native class. */
public class TileServiceUtils {
    /**
     * @return Default server URL for getting query tiles.
     */
    @CalledByNative
    private static String getDefaultServerUrl() {
        return AppHooks.get().getDefaultQueryTilesServerUrl();
    }
}
