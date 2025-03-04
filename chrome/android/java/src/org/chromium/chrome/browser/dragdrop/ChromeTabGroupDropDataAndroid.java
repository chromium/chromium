// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dragdrop;

import org.chromium.chrome.browser.tabmodel.TabGroupMetadata;

/** Chrome-specific drop data containing a {@link TabGroupMetadata}. */
public class ChromeTabGroupDropDataAndroid extends ChromeDropDataAndroid {
    public final TabGroupMetadata tabGroupMetadata;

    ChromeTabGroupDropDataAndroid(Builder builder) {
        super(builder);
        tabGroupMetadata = builder.mTabGroupMetadata;
    }

    @Override
    public boolean hasBrowserContent() {
        return tabGroupMetadata != null;
    }

    @Override
    public boolean isIncognito() {
        return hasBrowserContent() && tabGroupMetadata.isIncognito;
    }

    @Override
    public String buildTabClipDataText() {
        // TODO(crbug.com/380327012): Implement clip data text for groups.
        return null;
    }

    /** Builder for @{@link ChromeTabDropDataAndroid} instance. */
    public static class Builder extends ChromeDropDataAndroid.Builder {
        private TabGroupMetadata mTabGroupMetadata;

        /**
         * @param tabGroupMetadata The {@link TabGroupMetadata} associated with the dragging group.
         * @return {@link ChromeTabGroupDropDataAndroid.Builder} instance.
         */
        public Builder withTabGroupMetadata(TabGroupMetadata tabGroupMetadata) {
            mTabGroupMetadata = tabGroupMetadata;
            return this;
        }

        @Override
        public ChromeDropDataAndroid build() {
            return new ChromeTabGroupDropDataAndroid(/* builder= */ this);
        }
    }
}
