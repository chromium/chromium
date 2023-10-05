// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dragdrop;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.dragdrop.DropDataAndroid;

/** */
public class ChromeDropDataAndroid extends DropDataAndroid {
    public final int mTabId;

    /** Not generated from java */
    ChromeDropDataAndroid(Builder builder) {
        super(null, null, null, null, null);
        this.mTabId = builder.mTabId;
    }

    public boolean hasTab() {
        return mTabId != Tab.INVALID_TAB_ID;
    }

    @Override
    public boolean hasBrowserContent() {
        return hasTab();
    }

    /** Builder for @{@link ChromeDropDataAndroid} instance. */
    public static class Builder {
        private int mTabId = Tab.INVALID_TAB_ID;

        /**
         * @param tabId to be set in clip data.
         * @return @{@link ChromeDropDataAndroid.Builder} instance.
         */
        public Builder withTabId(int tabId) {
            this.mTabId = tabId;
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
