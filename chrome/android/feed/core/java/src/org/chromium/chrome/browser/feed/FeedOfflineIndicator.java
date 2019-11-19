// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import com.google.android.libraries.feed.api.host.offlineindicator.OfflineIndicatorApi;

/**
 * An extension of the {@link OfflineIndicatorApi} with additional methods needed for offline logic
 * in Chromium.
 */
public interface FeedOfflineIndicator extends OfflineIndicatorApi {
    /** Cleans up native resources, should be called when no longer needed. */
    void destroy();

    /**
     * @param url The url of the page that was previously queried for through
     * {@link OfflineIndicatorApi#getOfflineStatus()} or
     * {@link OfflineStatusListener#updateOfflineStatus()}.
     * @return the offline id of the page if offlined, otherwise null.
     */
    Long getOfflineIdIfPageIsOfflined(String url);
}
