// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dragdrop;

import android.content.ClipDescription;
import android.content.Context;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabGroupTitleUtils;
import org.chromium.ui.base.MimeTypeUtils;

import java.util.List;

/** Chrome-specific drop data containing a List of{@link Tab}s. */
@NullMarked
public class ChromeMultiTabDropDataAndroid extends ChromeDropDataAndroid {
    public final @Nullable List<Tab> tabs;
    public final @Nullable Tab primaryTab;

    ChromeMultiTabDropDataAndroid(Builder builder) {
        super(builder);
        tabs = builder.mTabs;
        primaryTab = builder.mPrimaryTab;
        assert tabs != null;
    }

    @Override
    public boolean hasBrowserContent() {
        return tabs != null && !tabs.isEmpty();
    }

    @Override
    public boolean isIncognito() {
        return tabs != null && !tabs.isEmpty() && tabs.get(0).isIncognitoBranded();
    }

    @Override
    public String buildTabClipDataText(Context context) {
        if (tabs == null || tabs.isEmpty()) return "";
        return TabGroupTitleUtils.getDefaultTitle(context, tabs.size());
    }

    @Override
    public String[] getSupportedMimeTypes() {
        return new String[] {
            MimeTypeUtils.CHROME_MIMETYPE_MULTI_TAB,
            ClipDescription.MIMETYPE_TEXT_PLAIN,
            ClipDescription.MIMETYPE_TEXT_INTENT,
            MimeTypeUtils.CHROME_MIMETYPE_LINK
        };
    }

    /** Builder for @{@link ChromeMultiTabDropDataAndroid} instance. */
    public static class Builder extends ChromeDropDataAndroid.Builder {
        private @Nullable List<Tab> mTabs;
        private @Nullable Tab mPrimaryTab;

        /**
         * @param tabs to be set in clip data.
         * @return {@link ChromeMultiTabDropDataAndroid.Builder} instance.
         */
        public Builder withTabs(List<Tab> tabs) {
            mTabs = tabs;
            return this;
        }

        /**
         * @param primaryTab to be set in clip data.
         * @return {@link ChromeMultiTabDropDataAndroid.Builder} instance.
         */
        public Builder withPrimaryTab(Tab primaryTab) {
            mPrimaryTab = primaryTab;
            return this;
        }

        @Override
        public ChromeDropDataAndroid build() {
            return new ChromeMultiTabDropDataAndroid(/* builder= */ this);
        }
    }
}
