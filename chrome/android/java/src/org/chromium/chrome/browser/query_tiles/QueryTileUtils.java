// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.query_tiles;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.chrome.browser.flags.ChromeFeatureList;

/** Handles various feature utility functions for query tiles. */
@JNINamespace("query_tiles")
public class QueryTileUtils {
    private static Boolean sShowQueryTilesOnNtp;
    private static Boolean sShowQueryTilesOnStartSurface;

    /**
     * Whether query tiles is enabled and should be shown on NTP.
     *
     * @return Whether the query tile feature is enabled on NTP.
     */
    public static boolean isQueryTilesEnabledOnNtp() {
        // Cache the result so it will not change during the same browser session.
        if (sShowQueryTilesOnNtp != null) return sShowQueryTilesOnNtp;
        boolean queryTileEnabled =
                ChromeFeatureList.isEnabled(ChromeFeatureList.QUERY_TILES)
                        && ChromeFeatureList.isEnabled(ChromeFeatureList.QUERY_TILES_IN_NTP);
        sShowQueryTilesOnNtp = queryTileEnabled;
        return sShowQueryTilesOnNtp;
    }

    /**
     * Whether query tiles is enabled and should be shown on start surface.
     * @return Whether the query tile feature is enabled on start surface.
     */
    public static boolean isQueryTilesEnabledOnStartSurface() {
        // Cache the result so it will not change during the same browser session.
        if (sShowQueryTilesOnStartSurface != null) return sShowQueryTilesOnStartSurface;
        boolean queryTileEnabled =
                ChromeFeatureList.isEnabled(ChromeFeatureList.QUERY_TILES)
                        || QueryTileUtilsJni.get().isQueryTilesEnabled();
        sShowQueryTilesOnStartSurface = queryTileEnabled;
        return sShowQueryTilesOnStartSurface;
    }

    @NativeMethods
    interface Natives {
        boolean isQueryTilesEnabled();
    }
}
