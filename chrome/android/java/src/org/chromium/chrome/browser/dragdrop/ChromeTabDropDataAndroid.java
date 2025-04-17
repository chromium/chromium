// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dragdrop;

import android.content.ClipDescription;
import android.content.Context;

import androidx.annotation.NonNull;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.base.MimeTypeUtils;

/** Chrome-specific drop data containing a {@link Tab}. */
public class ChromeTabDropDataAndroid extends ChromeDropDataAndroid {
    public final Tab tab;
    public final boolean isTabInGroup;

    ChromeTabDropDataAndroid(Builder builder) {
        super(builder);
        tab = builder.mTab;
        assert tab != null;
        isTabInGroup = builder.mIsTabInGroup;
    }

    @Override
    public boolean hasBrowserContent() {
        return tab != null;
    }

    @Override
    public boolean isIncognito() {
        return tab.isIncognitoBranded();
    }

    @Override
    public String buildTabClipDataText(Context context) {
        return tab.getUrl().getSpec();
    }

    @Override
    public String[] getSupportedMimeTypes() {
        return new String[] {
            MimeTypeUtils.CHROME_MIMETYPE_TAB,
            ClipDescription.MIMETYPE_TEXT_PLAIN,
            ClipDescription.MIMETYPE_TEXT_INTENT,
            MimeTypeUtils.CHROME_MIMETYPE_LINK
        };
    }

    /** Builder for @{@link ChromeTabDropDataAndroid} instance. */
    public static class Builder extends ChromeDropDataAndroid.Builder {
        private Tab mTab;
        private boolean mIsTabInGroup;

        /**
         * @param tab to be set in clip data.
         * @return {@link ChromeTabDropDataAndroid.Builder} instance.
         */
        public Builder withTab(@NonNull Tab tab) {
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
