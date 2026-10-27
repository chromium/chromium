// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.data_provider;

import org.chromium.base.Token;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.Objects;

/**
 * Tab group card or header item representing a tab group in the grouped or nested layout.
 *
 * <p>Immutable. Carries the group {@link Token} plus the selection and collapse states owned by the
 * provider.
 */
@NullMarked
public final class TabGroupItem implements TabListItem {
    private final Token mTabGroupId;
    private final boolean mIsSelected;
    private final boolean mIsCollapsed;

    /**
     * Constructs a new {@link TabGroupItem}.
     *
     * @param tabGroupId The unique tab group {@link Token}.
     * @param isSelected Whether the group card is selected.
     * @param isCollapsed Whether the group is collapsed.
     */
    public TabGroupItem(Token tabGroupId, boolean isSelected, boolean isCollapsed) {
        mTabGroupId = tabGroupId;
        mIsSelected = isSelected;
        mIsCollapsed = isCollapsed;
    }

    @Override
    public boolean isSelected() {
        return mIsSelected;
    }

    @Override
    public TabGroupItem withSelected(boolean isSelected) {
        if (mIsSelected == isSelected) return this;
        return new TabGroupItem(mTabGroupId, isSelected, mIsCollapsed);
    }

    /** Returns the unique tab group {@link Token}. */
    public Token getTabGroupId() {
        return mTabGroupId;
    }

    /** Returns whether the tab group is collapsed. */
    public boolean isCollapsed() {
        return mIsCollapsed;
    }

    /**
     * Returns a copy of this {@link TabGroupItem} with the updated collapse state.
     *
     * @param isCollapsed Whether the group is collapsed.
     */
    public TabGroupItem withCollapsed(boolean isCollapsed) {
        if (mIsCollapsed == isCollapsed) return this;
        return new TabGroupItem(mTabGroupId, mIsSelected, isCollapsed);
    }

    @Override
    public boolean equals(@Nullable Object obj) {
        if (this == obj) return true;
        if (!(obj instanceof TabGroupItem other)) return false;
        return mTabGroupId.equals(other.mTabGroupId)
                && mIsSelected == other.mIsSelected
                && mIsCollapsed == other.mIsCollapsed;
    }

    @Override
    public int hashCode() {
        return Objects.hash(mTabGroupId, mIsSelected, mIsCollapsed);
    }
}
