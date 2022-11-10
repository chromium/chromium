// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history_clusters;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.View;
import android.widget.ImageView;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableItemView;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableListUtils;
import org.chromium.ui.base.ViewUtils;

class HistoryClustersItemView extends SelectableItemView<ClusterVisit> {
    private DividerView mDividerView;
    /**
     * Constructor for inflating from XML.
     */
    public HistoryClustersItemView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mDividerView = new DividerView(getContext(), null, 0, R.style.HorizontalDivider);
        mDividerView.addToParent(this, generateDefaultLayoutParams());
        mEndButtonView.setVisibility(VISIBLE);
        mEndButtonView.setImageResource(R.drawable.btn_delete_24dp);
        mEndButtonView.setContentDescription(getContext().getString((R.string.remove)));
        ApiCompatibilityUtils.setImageTintList(mEndButtonView,
                AppCompatResources.getColorStateList(
                        getContext(), R.color.default_icon_color_secondary_tint_list));
        mEndButtonView.setScaleType(ImageView.ScaleType.CENTER_INSIDE);
        mEndButtonView.setPaddingRelative(getResources().getDimensionPixelSize(
                                                  R.dimen.visit_item_remove_button_lateral_padding),
                getPaddingTop(),
                getResources().getDimensionPixelSize(
                        R.dimen.visit_item_remove_button_lateral_padding),
                getPaddingBottom());
    }

    @Override
    protected void onClick() {}

    void setTitleText(CharSequence text) {
        mTitleView.setText(text);
        SelectableListUtils.setContentDescriptionContext(getContext(), mEndButtonView,
                text.toString(), SelectableListUtils.ContentDescriptionSource.REMOVE_BUTTON);
    }

    void setHostText(CharSequence text) {
        mDescriptionView.setText(text);
    }

    void setIconDrawable(Drawable drawable) {
        super.setStartIconDrawable(drawable);
    }

    void setEndButtonClickHandler(OnClickListener onClickListener) {
        mEndButtonView.setOnClickListener(onClickListener);
    }

    void setDividerVisibility(boolean visible) {
        mDividerView.setVisibility(visible ? VISIBLE : GONE);
    }

    void setHasThickDivider(boolean hasThickDivider) {
        mDividerView.setIsThickDivider(hasThickDivider);
        LayoutParams layoutParams = (LayoutParams) mContentView.getLayoutParams();
        if (hasThickDivider) {
            layoutParams.bottomMargin =
                    getResources().getDimensionPixelSize(R.dimen.divider_margin);
        } else {
            layoutParams.bottomMargin = 0;
        }

        ViewUtils.requestLayout(this, "HistoryClustersItemView.setHasThickDivider");
    }

    void setEndButtonVisibility(boolean visible) {
        mEndButtonView.setVisibility(visible ? View.VISIBLE : View.GONE);
    }
}
