// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards;

import android.content.Context;

import androidx.annotation.StringRes;

import org.chromium.chrome.R;

/**
 * Card that is shown when the user needs to be made aware of some information about their
 * configuration that affects the NTP suggestions.
 */
public abstract class StatusItem extends OptionalLeaf implements StatusCardViewHolder.DataSource {
    public static StatusItem createNoSuggestionsItem(SuggestionsCategoryInfo categoryInfo) {
        return new NoSuggestionsItem(categoryInfo);
    }

    private static class NoSuggestionsItem extends StatusItem {
        private final String mDescription;
        public NoSuggestionsItem(SuggestionsCategoryInfo categoryInfo) {
            mDescription = categoryInfo.getNoSuggestionsMessage();
        }

        @Override
        @StringRes
        public int getHeader() {
            return R.string.ntp_title_no_suggestions;
        }

        @Override
        public String getDescription() {
            return mDescription;
        }

        @Override
        @StringRes
        public int getActionLabel() {
            return 0;
        }

        @Override
        public void performAction(Context context) {
            assert false;
        }

        @Override
        public String describeForTesting() {
            return "NO_SUGGESTIONS";
        }
    }

    @Override
    @ItemViewType
    protected int getItemViewType() {
        return ItemViewType.STATUS;
    }

    @Override
    protected void onBindViewHolder(NewTabPageViewHolder holder) {
        ((StatusCardViewHolder) holder).onBindViewHolder(this, null);
    }

    public void setVisible(boolean visible) {
        setVisibilityInternal(visible);
    }
}
