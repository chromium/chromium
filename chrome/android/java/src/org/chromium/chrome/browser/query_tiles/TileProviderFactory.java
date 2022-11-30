// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.query_tiles;

import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.query_tiles.TileProvider;

/**
 * Basic factory that creates and returns an {@link TileProvider} that is attached
 * natively to the given {@link Profile}.
 */
public class TileProviderFactory {
    private static TileProvider sTileProviderForTesting;

    /**
     * Used to get access to the tile provider backend.
     * @return An {@link TileProvider} instance.
     */
    public static TileProvider getForProfile(Profile profile) {
        if (sTileProviderForTesting != null) return sTileProviderForTesting;
        return TileProviderFactoryJni.get().getForProfile(profile);
    }

    /** For testing only. */
    public static void setTileProviderForTesting(TileProvider provider) {
        sTileProviderForTesting = provider;
    }

    @NativeMethods
    interface Natives {
        TileProvider getForProfile(Profile profile);
    }
}