// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.base.Token;
import org.chromium.build.annotations.NullMarked;
import org.chromium.components.tab_groups.TabGroupColorId;

/** Data container for the visual data of a tab group collection loaded from storage. */
@NullMarked
@JNINamespace("tabs")
public class TabGroupCollectionData {
    private final Token mTabGroupId;
    private final String mTitle;
    private final @TabGroupColorId int mColor;
    private final boolean mIsCollapsed;

    /**
     * @param tabGroupId The tab group ID.
     * @param title The title of the tab group collection.
     * @param color The color of the tab group collection.
     * @param isCollapsed True if the tab group collection is collapsed.
     */
    public TabGroupCollectionData(
            Token tabGroupId, String title, @TabGroupColorId int color, boolean isCollapsed) {
        mTabGroupId = tabGroupId;
        mTitle = title;
        mColor = color;
        mIsCollapsed = isCollapsed;
    }

    @CalledByNative
    public static TabGroupCollectionData create(
            @JniType("base::Token") Token tabGroupId,
            @JniType("std::u16string") String title,
            @TabGroupColorId int color,
            boolean isCollapsed) {
        return new TabGroupCollectionData(tabGroupId, title, color, isCollapsed);
    }

    /** Returns the tab group ID. */
    public Token getTabGroupId() {
        return mTabGroupId;
    }

    /** Returns the title of the tab group collection. */
    public String getTitle() {
        return mTitle;
    }

    /** Returns the color of the tab group collection. */
    public @TabGroupColorId int getColor() {
        return mColor;
    }

    /** Returns true if the tab group collection is collapsed. */
    public boolean isCollapsed() {
        return mIsCollapsed;
    }
}
