// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dragdrop;

import org.chromium.chrome.browser.tab.Tab;

/** Chrome-specific drop data containing a {@link Tab}. */
public class ChromeTabDropDataAndroid extends ChromeDropDataAndroid {
    public final Tab tab;
    public final boolean isTabInGroup;

    ChromeTabDropDataAndroid(Builder builder) {
        super(builder);
        tab = builder.mTab;
        isTabInGroup = builder.mIsTabInGroup;
    }

    @Override
    public boolean hasBrowserContent() {
        return tab != null;
    }

    @Override
    public boolean isIncognito() {
        return hasBrowserContent() && tab.isIncognitoBranded();
    }

    @Override
    public String buildTabClipDataText() {
        return hasBrowserContent() ? tab.getUrl().getSpec() : null;
    }

    /** Builder for @{@link ChromeTabDropDataAndroid} instance. */
    public static class Builder extends ChromeDropDataAndroid.Builder {
        private Tab mTab;
        private boolean mIsTabInGroup;

        /**
         * @param tab to be set in clip data.
         * @return {@link ChromeTabDropDataAndroid.Builder} instance.
         */
        public Builder withTab(Tab tab) {
            mTab = tab;
            return this;
        }

        /**
         * @param isTabInGroup Whether the dragged tab is in a tab group.
         * @return {@link ChromeTabDropDataAndroid.Builder} instance.
         */
        public Builder withTabInGroup(boolean isTabInGroup) {
            mIsTabInGroup = isTabInGroup;
            return this;
        }

        @Override
        public ChromeDropDataAndroid build() {
            return new ChromeTabDropDataAndroid(/* builder= */ this);
        }
    }
}
