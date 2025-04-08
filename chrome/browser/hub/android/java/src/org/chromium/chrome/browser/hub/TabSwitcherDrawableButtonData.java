// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import android.content.Context;
import android.graphics.drawable.Drawable;

import androidx.annotation.Nullable;
import androidx.annotation.PluralsRes;
import androidx.annotation.StringRes;

import java.util.Objects;

/** Resolves the tab switcher drawable's button data. */
public class TabSwitcherDrawableButtonData extends DrawableButtonData {
    private final int mTabCount;

    /**
     * @param textRes The text resource to resolve.
     * @param contentDescriptionRes The content description resource to resolve.
     * @param drawable The tab switcher drawable {@link Drawable} to display.
     * @param tabCount The number of tabs in the tab switcher.
     */
    public TabSwitcherDrawableButtonData(
            @StringRes int textRes,
            @PluralsRes int contentDescriptionRes,
            Drawable drawable,
            int tabCount) {
        super(textRes, contentDescriptionRes, drawable);
        mTabCount = tabCount;
    }

    @Override
    public String resolveContentDescription(Context context) {
        return context.getResources()
                .getQuantityString(mContentDescriptionRes, mTabCount, mTabCount);
    }

    @Override
    public int hashCode() {
        return Objects.hash(super.hashCode(), mTabCount);
    }

    @Override
    public boolean equals(@Nullable Object o) {
        if (this == o) {
            return true;
        }
        if (o instanceof TabSwitcherDrawableButtonData that) {
            return super.equals(o) && mTabCount == that.mTabCount;
        }
        return false;
    }
}
