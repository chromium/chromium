// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dragdrop;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.dragdrop.DropDataAndroid;

/** */
public class ChromeDropDataAndroid extends DropDataAndroid {
    public final Tab tab;
    public final boolean allowTabTearing;

    /** Not generated from java */
    ChromeDropDataAndroid(Builder builder) {
        super(null, null, null, null, null);
        tab = builder.mTab;
        allowTabTearing = builder.mAllowTabTearing;
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
        private boolean mAllowTabTearing;

        /**
         * @param tab to be set in clip data.
         * @return {@link ChromeDropDataAndroid.Builder} instance.
         */
        public Builder withTab(Tab tab) {
            mTab = tab;
            return this;
        }

        /**
         * @param allowTabTearing Whether tab tearing should be allowed.
         * @return {@link ChromeDropDataAndroid.Builder} instance.
         */
        public Builder withAllowTabTearing(boolean allowTabTearing) {
            mAllowTabTearing = allowTabTearing;
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
