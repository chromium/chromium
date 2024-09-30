// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.graphics.drawable.GradientDrawable;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.ColorInt;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.Token;
import org.chromium.chrome.R;
import org.chromium.components.tab_groups.TabGroupColorId;

/** Provides a view for tab group color dots. */
public class TabGroupColorViewProvider {
    private final @NonNull Context mContext;
    private final @NonNull Token mTabGroupId;
    private final boolean mIsIncognito;

    private @Nullable ViewGroup mViewGroup;
    private @TabGroupColorId int mColorId;

    /**
     * @param context The context to use to use for creating the view.
     * @param tabGroupId The tab group id for the group.
     * @param isIncognito Whether the tab group is incognito.
     * @param colorId The {@link TabGroupColorId} to show for the main color.
     */
    public TabGroupColorViewProvider(
            @NonNull Context context,
            @NonNull Token tabGroupId,
            boolean isIncognito,
            @TabGroupColorId int colorId) {
        assert tabGroupId != null;
        mContext = context;
        mTabGroupId = tabGroupId;
        mIsIncognito = isIncognito;
        mColorId = colorId;
    }

    /** Returns the tab group id that this tab group color view provider is for. */
    public @NonNull Token getTabGroupId() {
        return mTabGroupId;
    }

    /**
     * Sets the tab group color by id, this will update the view immediately if it exists.
     *
     * @param colorId The color id to use.
     */
    public void setTabGroupColorId(@TabGroupColorId int colorId) {
        mColorId = colorId;

        if (mViewGroup != null) {
            updateColor();
        }
    }

    /** Returns the color dot view, creating it if it does not exist. */
    public @NonNull View getLazyView() {
        if (mViewGroup == null) {
            mViewGroup =
                    (ViewGroup)
                            LayoutInflater.from(mContext)
                                    .inflate(R.layout.tab_group_color_container, null);
            assert mViewGroup != null;

            updateColor();
        }
        return mViewGroup;
    }

    private void updateColor() {
        assert mViewGroup != null;

        GradientDrawable drawable = (GradientDrawable) mViewGroup.getBackground();
        assert drawable != null;

        final @ColorInt int color =
                ColorPickerUtils.getTabGroupColorPickerItemColor(mContext, mColorId, mIsIncognito);
        drawable.setColor(color);
        mViewGroup.invalidate();
    }
}
