// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.snippets;

import androidx.annotation.NonNull;

import org.chromium.chrome.browser.ntp.cards.ItemViewType;
import org.chromium.chrome.browser.ntp.cards.NewTabPageViewHolder;
import org.chromium.chrome.browser.ntp.cards.OptionalLeaf;

/**
 * Represents the data for a header of a group of snippets.
 */
public class SectionHeader extends OptionalLeaf {
    /** The header text to be shown. */
    private final String mHeaderText;

    private Runnable mToggleCallback;
    private boolean mIsExpanded;

    /**
     * Constructor for non-expandable header.
     * @param headerText The title of the header.
     */
    public SectionHeader(String headerText) {
        this.mHeaderText = headerText;
        setVisibilityInternal(true);
    }

    /**
     * Constructor for expandable header.
     * @param headerText The title of the header.
     * @param isExpanded Whether the header is expanded initially.
     * @param toggleCallback The callback to run when the header is toggled.
     */
    public SectionHeader(String headerText, boolean isExpanded, @NonNull Runnable toggleCallback) {
        this(headerText);
        mToggleCallback = toggleCallback;
        mIsExpanded = isExpanded;
    }

    @Override
    @ItemViewType
    public int getItemViewType() {
        return ItemViewType.HEADER;
    }

    public String getHeaderText() {
        return mHeaderText;
    }

    /**
     * @return Whether or not the header is expandable.
     */
    public boolean isExpandable() {
        return mToggleCallback != null;
    }

    /**
     * @return Whether or not the header is currently at the expanded state.
     */
    public boolean isExpanded() {
        return mIsExpanded;
    }

    /**
     * Toggle the expanded state of the header.
     */
    public void toggleHeader() {
        mIsExpanded = !mIsExpanded;
        notifyItemChanged(0, SectionHeaderViewHolder::updateVisuals);
        mToggleCallback.run();
    }

    @Override
    protected void onBindViewHolder(NewTabPageViewHolder holder) {
        ((SectionHeaderViewHolder) holder).onBindViewHolder(this);
    }

    @Override
    public String describeForTesting() {
        return "HEADER";
    }

    public void setVisible(boolean visible) {
        setVisibilityInternal(visible);
    }
}
