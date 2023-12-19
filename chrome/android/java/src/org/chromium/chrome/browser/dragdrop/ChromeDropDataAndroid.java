// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dragdrop;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.dragdrop.DropDataAndroid;

/** */
public class ChromeDropDataAndroid extends DropDataAndroid {
    private static final String TAB_DATA_PREFIX = "TabId=";
    private static final String TAB_DATA_DELIMITER = "\n";
    public final Tab mTab;

    /** Not generated from java */
    ChromeDropDataAndroid(Builder builder) {
        super(null, null, null, null, null);
        this.mTab = builder.mTab;
    }

    public boolean hasTab() {
        return mTab != null;
    }

    @Override
    public boolean hasBrowserContent() {
        return hasTab();
    }

    /** Build clip data text with tab info. */
    public String buildTabClipDataText() {
        if (hasTab()) {
            return TAB_DATA_PREFIX + mTab.getId() + TAB_DATA_DELIMITER + mTab.getUrl().getSpec();
        }
        return null;
    }

    /** Extract tabId from clip data item text. */
    public static int extractTabId(String clipDataText) {
        if (clipDataText == null) return Tab.INVALID_TAB_ID;
        String[] parts = clipDataText.split(TAB_DATA_DELIMITER);
        // Check if first part exists with atleast prefix length.
        if (parts.length == 0 || parts[0].length() <= TAB_DATA_PREFIX.length())
            return Tab.INVALID_TAB_ID;
        String tabIdStr = parts[0].substring(TAB_DATA_PREFIX.length());
        // Check string contains all digits since this is tabId.
        if (!tabIdStr.matches("\\d+")) return Tab.INVALID_TAB_ID;
        return Integer.parseInt(tabIdStr);
    }

    /** Builder for @{@link ChromeDropDataAndroid} instance. */
    public static class Builder {
        private Tab mTab;

        /**
         * @param tab to be set in clip data.
         * @return @{@link ChromeDropDataAndroid.Builder} instance.
         */
        public Builder withTab(Tab tab) {
            this.mTab = tab;
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
