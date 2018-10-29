// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.download.home.filter.chips;

import android.content.res.ColorStateList;
import android.support.v4.view.ViewCompat;
import android.support.v7.widget.AppCompatImageView;
import android.support.v7.widget.RecyclerView.ViewHolder;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;

/** The {@link ViewHolder} responsible for reflecting a {@link Chip} to a {@link View}. */
public class ChipsViewHolder extends ViewHolder {
    private final int mTextStartPaddingWithIconPx;
    private final int mTextStartPaddingWithNoIconPx;

    private final TextView mText;
    private final AppCompatImageView mImage;

    /** Builds a ChipsViewHolder around a specific {@link View}. */
    private ChipsViewHolder(View itemView) {
        super(itemView);

        mText = itemView.findViewById(org.chromium.chrome.R.id.text);
        mImage = (AppCompatImageView) itemView.findViewById(org.chromium.chrome.R.id.icon);

        ColorStateList textColors = mText.getTextColors();
        if (textColors != null) ApiCompatibilityUtils.setImageTintList(mImage, textColors);

        mTextStartPaddingWithIconPx = mText.getResources().getDimensionPixelSize(
                org.chromium.chrome.R.dimen.chip_icon_padding);
        mTextStartPaddingWithNoIconPx = mText.getResources().getDimensionPixelSize(
                org.chromium.chrome.R.dimen.chip_no_icon_padding);
    }

    /**
     * Used as a method reference for ViewHolderFactory.
     * @see org.chromium.chrome.browser.modelutil.RecyclerViewAdapter
     *         .ViewHolderFactory#createViewHolder
     */
    public static ChipsViewHolder create(ViewGroup parent, int viewType) {
        assert viewType == 0;
        return new ChipsViewHolder(LayoutInflater.from(parent.getContext())
                                           .inflate(org.chromium.chrome.R.layout.chip, null));
    }

    /**
     * Used as a method reference for ViewBinder, to push the properties of {@code chip} to
     * {@link #itemView}.
     * @param chip The {@link Chip} to visually reflect in the stored {@link View}.
     * @see org.chromium.chrome.browser.modelutil.SimpleRecyclerViewMcp.ViewBinder#onBindViewHolder
     */
    public void bind(Chip chip) {
        itemView.setEnabled(chip.enabled);
        mText.setEnabled(chip.enabled);
        mImage.setEnabled(chip.enabled);

        itemView.setSelected(chip.selected);
        itemView.setOnClickListener(v -> chip.chipSelectedListener.run());
        mText.setContentDescription(mText.getContext().getText(chip.contentDescription));
        mText.setText(chip.text);

        final int textStartPadding;
        if (chip.icon == Chip.INVALID_ICON_ID) {
            mImage.setVisibility(ViewGroup.GONE);
            textStartPadding = mTextStartPaddingWithNoIconPx;
        } else {
            textStartPadding = mTextStartPaddingWithIconPx;
            mImage.setVisibility(ViewGroup.VISIBLE);
            mImage.setImageResource(chip.icon);
        }

        ViewCompat.setPaddingRelative(mText, textStartPadding, mText.getPaddingTop(),
                ViewCompat.getPaddingEnd(mText), mText.getPaddingBottom());
    }
}
