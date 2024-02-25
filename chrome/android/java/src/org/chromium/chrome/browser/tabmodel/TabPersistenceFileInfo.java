// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import androidx.annotation.VisibleForTesting;

import java.util.Collections;
import java.util.LinkedList;
import java.util.List;

/** Contains representations of stored Tab data, sufficient to identify said Tab data in storage. */
public class TabPersistenceFileInfo {
    // List of identifiers for TabState files.
    private List<TabStateFileInfo> mTabStateFileInfos = new LinkedList<>();

    // List of metadata files.
    private List<String> mMetadataFiles = new LinkedList<>();

    public void addTabStateFileInfo(int tabId, boolean isEncrypted) {
        mTabStateFileInfos.add(new TabStateFileInfo(tabId, isEncrypted));
    }

    public void addMetadataFile(String metadataFile) {
        mMetadataFiles.add(metadataFile);
    }

    public List<TabStateFileInfo> getTabStateFileInfos() {
        return Collections.unmodifiableList(mTabStateFileInfos);
    }

    public List<String> getMetadataFiles() {
        return Collections.unmodifiableList(mMetadataFiles);
    }

    /** Contains data for identifying a TabState file. */
    public static class TabStateFileInfo {
        public final int tabId;
        public final boolean isEncrypted;

        @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
        public TabStateFileInfo(int tabId, boolean isEncrypted) {
            this.tabId = tabId;
            this.isEncrypted = isEncrypted;
        }

        @Override
        public boolean equals(Object other) {
            if (this == other) return true;
            if (other == null) return false;
            if (other instanceof TabStateFileInfo) {
                TabStateFileInfo otherTabStateId = (TabStateFileInfo) other;
                return otherTabStateId.tabId == tabId && otherTabStateId.isEncrypted == isEncrypted;
            }
            return false;
        }

        @Override
        public int hashCode() {
            return 5 * tabId + 17 * (isEncrypted ? 1 : 0);
        }
    }
}
