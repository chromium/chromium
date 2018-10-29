// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget.selection;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.drawable.Drawable;
import android.support.annotation.Nullable;
import android.support.annotation.VisibleForTesting;
import android.support.graphics.drawable.AnimatedVectorDrawableCompat;
import android.support.v7.content.res.AppCompatResources;
import android.support.v7.widget.AppCompatImageView;
import android.util.AttributeSet;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.widget.TintedDrawable;

/**
 * Default implementation of SelectableItemViewBase.
 *
 * @param <E> The type of the item associated with this SelectableItemViewBase.
 */
public abstract class SelectableItemView<E> extends SelectableItemViewBase<E> {
    protected final int mDefaultLevel;
    protected final int mSelectedLevel;
    protected final AnimatedVectorDrawableCompat mCheckDrawable;

    protected AppCompatImageView mIconView;
    protected TextView mTitleView;
    protected TextView mDescriptionView;
    protected ColorStateList mIconColorList;
    private Drawable mIconDrawable;

    /**
     * Constructor for inflating from XML.
     */
    public SelectableItemView(Context context, AttributeSet attrs) {
        super(context, attrs);
        mIconColorList =
                AppCompatResources.getColorStateList(getContext(), R.color.white_mode_tint);
        mDefaultLevel = getResources().getInteger(R.integer.list_item_level_default);
        mSelectedLevel = getResources().getInteger(R.integer.list_item_level_selected);
        mCheckDrawable = AnimatedVectorDrawableCompat.create(
                getContext(), R.drawable.ic_check_googblue_24dp_animated);
    }

    // FrameLayout implementations.
    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mIconView = (AppCompatImageView) findViewById(R.id.icon_view);
        mTitleView = (TextView) findViewById(R.id.title);
        mDescriptionView = (TextView) findViewById(R.id.description);

        if (mIconView != null) {
            mIconView.setBackgroundResource(R.drawable.list_item_icon_modern_bg);
            ApiCompatibilityUtils.setImageTintList(mIconView, getDefaultIconTint());
        }
    }

    /**
     * Set drawable for the icon view. Note that you may need to use this method instead of
     * mIconView#setImageDrawable to ensure icon view is correctly set in selection mode.
     */
    protected void setIconDrawable(Drawable iconDrawable) {
        mIconDrawable = iconDrawable;
        updateView();
    }

    /**
     * Update icon image and background based on whether this item is selected.
     */
    @Override
    protected void updateView() {
        // TODO(huayinz): Refactor this method so that mIconView is not exposed to subclass.
        if (mIconView == null) return;

        if (isChecked()) {
            mIconView.getBackground().setLevel(mSelectedLevel);
            mIconView.setImageDrawable(mCheckDrawable);
            ApiCompatibilityUtils.setImageTintList(mIconView, mIconColorList);
            mCheckDrawable.start();
        } else {
            mIconView.getBackground().setLevel(mDefaultLevel);
            mIconView.setImageDrawable(mIconDrawable);
            ApiCompatibilityUtils.setImageTintList(mIconView, getDefaultIconTint());
        }
    }

    /**
     * @return The {@link ColorStateList} used to tint the icon drawable set via
     *         {@link #setIconDrawable(Drawable)} when the item is not selected.
     */
    protected @Nullable ColorStateList getDefaultIconTint() {
        return null;
    }

    @VisibleForTesting
    public void endAnimationsForTests() {
        mCheckDrawable.stop();
    }

    /**
     * Sets the icon for the image view: the default icon if unselected, the check mark if selected.
     *
     * @param imageView     The image view in which the icon will be presented.
     * @param defaultIcon   The default icon that will be displayed if not selected.
     * @param isSelected    Whether the item is selected or not.
     */
    public static void applyModernIconStyle(
            AppCompatImageView imageView, Drawable defaultIcon, boolean isSelected) {
        imageView.setBackgroundResource(R.drawable.list_item_icon_modern_bg);
        imageView.setImageDrawable(isSelected
                        ? TintedDrawable.constructTintedDrawable(imageView.getContext(),
                                  R.drawable.ic_check_googblue_24dp, R.color.white_mode_tint)
                        : defaultIcon);
        imageView.getBackground().setLevel(isSelected
                        ? imageView.getResources().getInteger(R.integer.list_item_level_selected)
                        : imageView.getResources().getInteger(R.integer.list_item_level_default));
    }
}