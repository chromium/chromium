// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.url.GURL;

/**
 * Class responsible for fetching images for the views in the NewTabPage and Chrome Home.
 * The images fetched by this class include:
 *   - large icons for URLs (used by the tiles in the NTP/Chrome Home)
 *
 * To fetch an image, the caller should create a request which is done in the following way:
 *   - for large icons: {@link #makeLargeIconRequest(String, int,
 * LargeIconBridge.LargeIconCallback)}
 *
 * If there are no errors is the image fetching process, the corresponding bitmap will be returned
 * in the callback. Otherwise, the callback will not be called.
 */
public class ImageFetcher {
    private boolean mIsDestroyed;

    private final Profile mProfile;
    private LargeIconBridge mLargeIconBridge;

    public ImageFetcher(Profile profile) {
        mProfile = profile;
    }

    /**
     * Gets the large icon (e.g. favicon or touch icon) for a given URL.
     *
     * If there is an error while fetching the icon, the callback will not be called.
     *
     * @param url The URL of the site whose icon is being requested.
     * @param size The desired size of the icon in pixels.
     * @param callback The callback to be notified when the icon is available.
     */
    public void makeLargeIconRequest(
            GURL url, int size, LargeIconBridge.LargeIconCallback callback) {
        assert !mIsDestroyed;

        getLargeIconBridge().getLargeIconForUrl(url, size, callback);
    }

    public void onDestroy() {
        if (mLargeIconBridge != null) {
            mLargeIconBridge.destroy();
            mLargeIconBridge = null;
        }

        mIsDestroyed = true;
    }

    /**
     * Utility method to lazily create the {@link LargeIconBridge}, and avoid unnecessary native
     * calls in tests.
     */
    @VisibleForTesting
    protected LargeIconBridge getLargeIconBridge() {
        if (mLargeIconBridge == null) {
            mLargeIconBridge =
                    SuggestionsDependencyFactory.getInstance().createLargeIconBridge(mProfile);
        }
        return mLargeIconBridge;
    }
}
