// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.continuous_search;

import java.util.List;
import java.util.Objects;

/**
 * A class that holds a group of related {@link PageItem}s.
 */
public class PageGroup {
    private final String mLabel;
    private final boolean mIsAdGroup;
    private final List<PageItem> mPageItems;

    PageGroup(String label, boolean isAdGroup, List<PageItem> pageItems) {
        mLabel = label;
        mIsAdGroup = isAdGroup;
        mPageItems = pageItems;
    }

    String getLabel() {
        return mLabel;
    }

    boolean isAdGroup() {
        return mIsAdGroup;
    }

    List<PageItem> getPageItems() {
        return mPageItems;
    }

    @Override
    public boolean equals(Object o) {
        if (o == this) return true;

        if (!(o instanceof PageGroup)) return false;

        PageGroup other = (PageGroup) o;

        return mLabel.equals(other.mLabel) && mIsAdGroup == other.mIsAdGroup
                && mPageItems.equals(other.mPageItems);
    }

    @Override
    public int hashCode() {
        return Objects.hash(mLabel, mIsAdGroup, mPageItems);
    }
}
