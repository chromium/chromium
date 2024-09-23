// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dragdrop;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.dragdrop.DropDataAndroid;

/** */
public class ChromeDropDataAndroid extends DropDataAndroid {
    public final Tab tab;
    public final boolean isTabInGroup;
    public final boolean allowTabDragToCreateInstance;

    /** Not generated from java */
    ChromeDropDataAndroid(Builder builder) {
        super(null, null, null, null, null);
        tab = builder.mTab;
        isTabInGroup = builder.mIsTabInGroup;
        allowTabDragToCreateInstance = builder.mAllowTabDragToCreateInstance;
    }

    public boolean hasTab() {
        return tab != null;
    }

    @Override
    public boolean hasBrowserContent() {
        return hasTab();
    }

    /** Build clip data text with tab info. */
    public String buildTabClipDataText() {
        if (hasTab()) {
            return tab.getUrl().getSpec();
        }
        return null;
    }

    /** Builder for @{@link ChromeDropDataAndroid} instance. */
    public static class Builder {
        private Tab mTab;
        private boolean mIsTabInGroup;
        private boolean mAllowTabDragToCreateInstance;

        /**
         * @param tab to be set in clip data.
         * @return {@link ChromeDropDataAndroid.Builder} instance.
         */
        public Builder withTab(Tab tab) {
            mTab = tab;
            return this;
        }

        /**
         * @param isTabInGroup Whether the dragged tab is in a tab group.
         * @return {@link ChromeDropDataAndroid.Builder} instance.
         */
        public Builder withTabInGroup(boolean isTabInGroup) {
            mIsTabInGroup = isTabInGroup;
            return this;
        }

        /**
         * @param allowDragToCreateInstance Whether tab drag to create new instance should be
         *     allowed.
         * @return {@link ChromeDropDataAndroid.Builder} instance.
         */
        public Builder withAllowDragToCreateInstance(boolean allowDragToCreateInstance) {
            mAllowTabDragToCreateInstance = allowDragToCreateInstance;
            return this;
        }

        /**
         * @return new @{@link ChromeDropDataAndroid} instance.
         */
        public ChromeDropDataAndroid build() {
            return new ChromeDropDataAndroid(this);
        }
    }
}
