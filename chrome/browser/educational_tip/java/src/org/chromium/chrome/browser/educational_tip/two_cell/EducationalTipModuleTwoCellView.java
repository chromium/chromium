// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip.two_cell;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.educational_tip.R;
import org.chromium.chrome.browser.setup_list.SetupListModuleUtils;

/**
 * View for a generic two-cell educational tip module. Contains UI elements to display two tip items
 * stacked vertically.
 */
@NullMarked
public class EducationalTipModuleTwoCellView extends LinearLayout {
    private TextView mModuleTitleView;
    private TextView mSeeMoreView;
    private TextView mItem1TitleView;
    private TextView mItem1DescriptionView;
    private ImageView mItem1IconView;
    private View mItem1Layout;
    private TextView mItem2TitleView;
    private TextView mItem2DescriptionView;
    private ImageView mItem2IconView;
    private View mItem2Layout;

    public EducationalTipModuleTwoCellView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mModuleTitleView = findViewById(R.id.educational_tip_module_title);
        mSeeMoreView = findViewById(R.id.see_more);
        mItem1TitleView = findViewById(R.id.two_cell_item_1_title);
        mItem1DescriptionView = findViewById(R.id.two_cell_item_1_description);
        mItem1IconView = findViewById(R.id.two_cell_item_1_icon);
        mItem1Layout = findViewById(R.id.two_cell_item_1);
        mItem2TitleView = findViewById(R.id.two_cell_item_2_title);
        mItem2DescriptionView = findViewById(R.id.two_cell_item_2_description);
        mItem2IconView = findViewById(R.id.two_cell_item_2_icon);
        mItem2Layout = findViewById(R.id.two_cell_item_2);
    }

    public void setModuleTitle(String title) {
        mModuleTitleView.setText(title);
    }

    public void setSeeMoreOnClickListener(OnClickListener listener) {
        mSeeMoreView.setOnClickListener(listener);
    }

    public void setItem1Title(String title) {
        mItem1TitleView.setText(title);
    }

    public void setItem1Description(String title) {
        mItem1DescriptionView.setText(title);
    }

    public void setItem1Icon(int iconResId) {
        mItem1IconView.setAlpha(1f);
        mItem1IconView.setImageResource(iconResId);
    }

    public void setItem1IconWithAnimation(int iconResId) {
        SetupListModuleUtils.updateIconWithAnimation(mItem1IconView, iconResId);
    }

    public void setItem1OnClickListener(OnClickListener listener) {
        mItem1Layout.setOnClickListener(listener);
    }

    public void setItem2Title(String title) {
        mItem2TitleView.setText(title);
    }

    public void setItem2Description(String title) {
        mItem2DescriptionView.setText(title);
    }

    public void setItem2Icon(int iconResId) {
        mItem2IconView.setAlpha(1f);
        mItem2IconView.setImageResource(iconResId);
    }

    public void setItem2IconWithAnimation(int iconResId) {
        SetupListModuleUtils.updateIconWithAnimation(mItem2IconView, iconResId);
    }

    public void setItem2OnClickListener(OnClickListener listener) {
        mItem2Layout.setOnClickListener(listener);
    }

    private void applyCompletedStyle(
            TextView titleView, TextView descriptionView, View itemLayout, boolean isCompleted) {
        if (isCompleted) {
            int disabledColor = getContext().getColor(R.color.default_text_color_disabled_list);
            titleView.setTextColor(disabledColor);
            titleView.setPaintFlags(
                    titleView.getPaintFlags() | android.graphics.Paint.STRIKE_THRU_TEXT_FLAG);
            descriptionView.setTextColor(disabledColor);
            descriptionView.setPaintFlags(
                    descriptionView.getPaintFlags() | android.graphics.Paint.STRIKE_THRU_TEXT_FLAG);

            // Disable clicks on the item layout
            itemLayout.setOnClickListener(null);
            itemLayout.setClickable(false);
            itemLayout.setForeground(null);
        } else {
            int titleColor = getContext().getColor(R.color.default_text_color_list);
            int descriptionColor = getContext().getColor(R.color.default_text_color_secondary_list);
            titleView.setTextColor(titleColor);
            titleView.setPaintFlags(
                    titleView.getPaintFlags() & ~android.graphics.Paint.STRIKE_THRU_TEXT_FLAG);
            descriptionView.setTextColor(descriptionColor);
            descriptionView.setPaintFlags(
                    descriptionView.getPaintFlags()
                            & ~android.graphics.Paint.STRIKE_THRU_TEXT_FLAG);

            itemLayout.setClickable(true);
        }
    }

    public void setItem1Completed(boolean isCompleted) {
        applyCompletedStyle(mItem1TitleView, mItem1DescriptionView, mItem1Layout, isCompleted);
    }

    public void setItem2Completed(boolean isCompleted) {
        applyCompletedStyle(mItem2TitleView, mItem2DescriptionView, mItem2Layout, isCompleted);
    }
}
