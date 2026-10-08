// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.data_provider;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabId;

import java.util.Objects;

/**
 * Tab card or row item representing a single tab in the tab list.
 *
 * <p>Immutable. Carries the tab ID plus the state owned by the provider, and never holds a {@link
 * Tab} reference: consumers resolve a tab by ID against the current tab model when needed.
 */
@NullMarked
public final class TabItem implements TabListItem {
    private final @TabId int mTabId;
    private final boolean mIsSelected;
    private final boolean mIsPinned;
    private final boolean mIsMultiSelected;

    /**
     * Constructs a new {@link TabItem}.
     *
     * @param tabId The unique tab identifier.
     * @param isSelected Whether the tab is selected.
     * @param isPinned Whether the tab is pinned.
     * @param isMultiSelected Whether the tab is part of a multi-selection set.
     */
    public TabItem(
            @TabId int tabId, boolean isSelected, boolean isPinned, boolean isMultiSelected) {
        mTabId = tabId;
        mIsSelected = isSelected;
        mIsPinned = isPinned;
        mIsMultiSelected = isMultiSelected;
    }

    /** Returns the unique tab identifier. */
    public @TabId int getTabId() {
        return mTabId;
    }

    @Override
    public boolean isSelected() {
        return mIsSelected;
    }

    /** Returns whether the tab is pinned. */
    public boolean isPinned() {
        return mIsPinned;
    }

    /** Returns whether the tab is part of a multi-selection set. */
    public boolean isMultiSelected() {
        return mIsMultiSelected;
    }

    @Override
    public TabItem withSelected(boolean isSelected) {
        if (mIsSelected == isSelected) return this;
        return new TabItem(mTabId, isSelected, mIsPinned, mIsMultiSelected);
    }

    /** Returns a copy of this item with the specified pinned state. */
    public TabItem withPinned(boolean isPinned) {
        if (mIsPinned == isPinned) return this;
        return new TabItem(mTabId, mIsSelected, isPinned, mIsMultiSelected);
    }

    // TODO(crbug.com/562590772): Add withMultiSelected(boolean) copy helper for granular item
    // updates.

    @Override
    public boolean equals(@Nullable Object obj) {
        if (this == obj) return true;
        if (!(obj instanceof TabItem other)) return false;
        return mTabId == other.mTabId
                && mIsSelected == other.mIsSelected
                && mIsPinned == other.mIsPinned
                && mIsMultiSelected == other.mIsMultiSelected;
    }

    @Override
    public int hashCode() {
        return Objects.hash(mTabId, mIsSelected, mIsPinned, mIsMultiSelected);
    }
}
