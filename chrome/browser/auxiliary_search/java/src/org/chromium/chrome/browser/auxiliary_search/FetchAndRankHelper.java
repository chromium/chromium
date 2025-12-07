// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search;

import org.jni_zero.CalledByNative;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.url.GURL;

@NullMarked
class FetchAndRankHelper {
    /**
     * Helper to create a new {@link AuxiliarySearchDataEntry} instance.
     *
     * @param type The type of the data source.
     * @param url The {@link GURL} of the entry.
     * @param title The page title.
     * @param lastActiveTime The last visited timestamp.
     * @param tabId The Tad ID of the entry if it is a local Tab, -1 otherwise.
     * @param appId The ID of the app which opens the URL if the entry is a CCT, null otherwise.
     * @param visitId A unique ID of the entry if it isn't a local Tab, -1 otherwise.
     */
    @CalledByNative
    static AuxiliarySearchDataEntry addDataEntry(
            @AuxiliarySearchEntryType int type,
            GURL url,
            String title,
            long lastActiveTime,
            int tabId,
            @Nullable String appId,
            int visitId) {
        return new AuxiliarySearchDataEntry(
                type, url, title, lastActiveTime, tabId, appId, visitId, /* score= */ 0);
    }
}
