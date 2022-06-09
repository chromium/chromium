// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history_clusters;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;

import org.chromium.components.browser_ui.widget.selectable_list.SelectableItemView;

class HistoryClustersItemView extends SelectableItemView<ClusterVisit> {
    /**
     * Constructor for inflating from XML.
     */
    public HistoryClustersItemView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mEndButtonView.setVisibility(GONE);
    }

    @Override
    protected void onClick() {}

    void setTitleText(CharSequence text) {
        mTitleView.setText(text);
    }

    void setHostText(CharSequence text) {
        mDescriptionView.setText(text);
    }

    void setIconDrawable(Drawable drawable) {
        super.setStartIconDrawable(drawable);
    }
}
