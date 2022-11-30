// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.query_tiles;

import org.chromium.base.Callback;

/**
 * Spins up a test server and starts serving query tiles in native.
 */
public class QueryTileFakeServer {
    /**
     * Spins up a test server and starts serving query tiles in native. Should be invoked after
     * native initialization and before rest of the chrome and omnibox is loaded.
     * @param levels The number of levels of query tiles.
     * @param tilesPerLevel The number of tiles in each level.
     * @param onFetchCompletedCallback The callback to be invoked after server fetch is complete.
     */
    public static void setupFakeServer(
            int levels, int tilesPerLevel, Callback<Boolean> onFetchCompletedCallback) {
        nativeSetupFakeServer(onFetchCompletedCallback, levels, tilesPerLevel);
    }

    static native void nativeSetupFakeServer(
            Callback<Boolean> onFetchCompletedCallback, int levels, int tilesPerLevel);
}