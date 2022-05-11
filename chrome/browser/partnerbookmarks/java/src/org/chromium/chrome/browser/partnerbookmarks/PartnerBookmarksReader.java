// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.partnerbookmarks;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;

/**
 * Reads bookmarks from the partner content provider (if any).
 */
public class PartnerBookmarksReader {

    /**
     * A callback used to indicate success or failure of favicon fetching when retrieving favicons
     * from cache or server.
     */
    interface FetchFaviconCallback {
        @CalledByNative("FetchFaviconCallback")
        void onFaviconFetched(@FaviconFetchResult int result);

        @CalledByNative("FetchFaviconCallback")
        void onFaviconFetch();
    }

    @NativeMethods
    interface Natives {
        long init(PartnerBookmarksReader caller);

        void reset(long nativePartnerBookmarksReader, PartnerBookmarksReader caller);
        void destroy(long nativePartnerBookmarksReader, PartnerBookmarksReader caller);
        long addPartnerBookmark(long nativePartnerBookmarksReader, PartnerBookmarksReader caller,
                String url, String title, boolean isFolder, long parentId, byte[] favicon,
                byte[] touchicon, boolean fetchUncachedFaviconsFromServer, int desiredFaviconSizePx,
                FetchFaviconCallback callback);
        void partnerBookmarksCreationComplete(
                long nativePartnerBookmarksReader, PartnerBookmarksReader caller);
        String getNativeUrlString(String url);
        void disablePartnerBookmarksEditing();
    }
}
