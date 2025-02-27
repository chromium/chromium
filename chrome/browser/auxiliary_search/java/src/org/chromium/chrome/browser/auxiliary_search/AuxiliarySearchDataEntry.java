// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search;

import android.text.TextUtils;

import org.chromium.build.annotations.Nullable;
import org.chromium.url.GURL;

import java.util.Objects;

/**
 * A single data entry for data shared with system. This class can be used for multiple data
 * sources, e.g., Tabs, custom Tabs, or MV tiles.
 */
public class AuxiliarySearchDataEntry {
    public final @AuxiliarySearchEntryType int type;
    public final GURL url;
    public final String title;
    public final long lastActiveTime;

    public final int tabId;
    public @Nullable final String appId;

    // visitId is used for non local Tabs. For local Tabs, its value is {@link Tab.INVALID_TAB_ID},
    // use tabId instead.
    public final int visitId;

    AuxiliarySearchDataEntry(
            @AuxiliarySearchEntryType int type,
            GURL url,
            String title,
            long lastActiveTime,
            int tabId,
            @Nullable String appId,
            int visitId) {
        this.type = type;
        this.url = url;
        this.title = title;
        this.lastActiveTime = lastActiveTime;
        this.tabId = tabId;
        this.appId = appId;
        this.visitId = visitId;
    }

    @Override
    public boolean equals(@Nullable Object obj) {
        if (obj == null || obj.getClass() != this.getClass()) {
            return false;
        }

        AuxiliarySearchDataEntry entry = (AuxiliarySearchDataEntry) obj;

        return this.type == entry.type
                && this.lastActiveTime == entry.lastActiveTime
                && this.tabId == entry.tabId
                && this.visitId == entry.visitId
                && this.url.equals(entry.url)
                && TextUtils.equals(this.title, entry.title)
                && TextUtils.equals(this.appId, entry.appId);
    }

    @Override
    public int hashCode() {
        return Objects.hash(type, url, title, lastActiveTime, tabId, appId, visitId);
    }
}
