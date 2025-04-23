// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dragdrop;

import android.content.ClipDescription;
import android.content.Context;
import android.text.TextUtils;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.chrome.browser.tabmodel.TabGroupMetadata;
import org.chromium.chrome.browser.tabmodel.TabGroupTitleUtils;
import org.chromium.ui.base.MimeTypeUtils;

/** Chrome-specific drop data containing a {@link TabGroupMetadata}. */
public class ChromeTabGroupDropDataAndroid extends ChromeDropDataAndroid {
    @Nullable public final TabGroupMetadata tabGroupMetadata;

    ChromeTabGroupDropDataAndroid(Builder builder) {
        super(builder);
        tabGroupMetadata = builder.mTabGroupMetadata;
        assert tabGroupMetadata != null;
    }

    @Override
    public boolean hasBrowserContent() {
        return tabGroupMetadata != null;
    }

    @Override
    public boolean isIncognito() {
        return tabGroupMetadata.isIncognito;
    }

    @Override
    public String buildTabClipDataText(Context context) {
        if (TextUtils.isEmpty(tabGroupMetadata.tabGroupTitle)) {
            return TabGroupTitleUtils.getDefaultTitle(
                    context, tabGroupMetadata.tabIdsToUrls.size());
        }
        return tabGroupMetadata.tabGroupTitle;
    }

    @Override
    public String[] getSupportedMimeTypes() {
        // TODO(crbug.com/404709214): Support Link Mimetype by using the shared tab group link.
        return new String[] {
            MimeTypeUtils.CHROME_MIMETYPE_TAB_GROUP,
            ClipDescription.MIMETYPE_TEXT_PLAIN,
            ClipDescription.MIMETYPE_TEXT_INTENT
        };
    }

    /** Builder for @{@link ChromeTabDropDataAndroid} instance. */
    public static class Builder extends ChromeDropDataAndroid.Builder {
        private TabGroupMetadata mTabGroupMetadata;

        /**
         * @param tabGroupMetadata The {@link TabGroupMetadata} associated with the dragging group.
         * @return {@link ChromeTabGroupDropDataAndroid.Builder} instance.
         */
        public Builder withTabGroupMetadata(@NonNull TabGroupMetadata tabGroupMetadata) {
            mTabGroupMetadata = tabGroupMetadata;
            return this;
        }

        @Override
        public ChromeDropDataAndroid build() {
            return new ChromeTabGroupDropDataAndroid(/* builder= */ this);
        }
    }
}
