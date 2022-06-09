// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history_clusters;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;

import androidx.annotation.Nullable;

import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableItemView;

class HistoryClusterView extends SelectableItemView<HistoryCluster> {
    /**
     * Constructor for inflating from XML.
     */
    public HistoryClusterView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mEndButtonView.setVisibility(GONE);
    }

    @Override
    protected void onClick() {}

    @Override
    protected @Nullable ColorStateList getDefaultStartIconTint() {
        return ColorStateList.valueOf(
                SemanticColorUtils.getDefaultIconColorSecondary(getContext()));
    }

    void setTitle(CharSequence text) {
        mTitleView.setText(text);
    }

    void setLabel(CharSequence text) {
        mDescriptionView.setText(text);
    }

    void setIconDrawable(Drawable drawable) {
        super.setStartIconDrawable(drawable);
    }

    void setEndButtonDrawable(Drawable drawable) {
        mEndButtonView.setVisibility(VISIBLE);
        mEndButtonView.setImageDrawable(drawable);
    }
}
