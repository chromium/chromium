// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.View;
import android.widget.CompoundButton;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.components.sync.SyncFeatureMap;

/** Controls the bookmarks save-flow view. */
public class ImprovedBookmarkSaveFlowView extends FrameLayout {
    private View mBookmarkContainer;
    private ImageView mBookmarkImageView;
    private TextView mBookmarkTitleView;
    private TextView mBookmarkSubtitleView;
    private View mPriceTrackingContainer;
    private CompoundButton mPriceTrackingSwitch;

    /** Constructor for inflating from XML. */
    public ImprovedBookmarkSaveFlowView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mBookmarkContainer = findViewById(R.id.bookmark_container);
        mBookmarkImageView = findViewById(R.id.bookmark_image);
        mBookmarkTitleView = findViewById(R.id.bookmark_title);
        mBookmarkSubtitleView = findViewById(R.id.bookmark_subtitle);
        mPriceTrackingContainer = findViewById(R.id.price_tracking_container);
        mPriceTrackingSwitch = findViewById(R.id.price_tracking_switch);

        mBookmarkContainer.setBackgroundResource(
                R.drawable.improved_bookmark_save_flow_single_pane_background);

        if (SyncFeatureMap.isEnabled(SyncFeatureMap.SYNC_ENABLE_BOOKMARKS_IN_TRANSPORT_MODE)) {
            mBookmarkTitleView.setTextAppearance(R.style.TextAppearance_TextMedium_Secondary);
            mBookmarkSubtitleView.setTextAppearance(R.style.TextAppearance_TextMedium_Secondary);
        }
    }

    void setBookmarkRowClickListener(View.OnClickListener listener) {
        mBookmarkContainer.setOnClickListener(listener);
    }

    void setBookmarkDrawable(Drawable drawable) {
        mBookmarkImageView.setImageDrawable(drawable);
    }

    void setTitle(CharSequence charSequence) {
        mBookmarkTitleView.setText(charSequence);
    }

    void setSubtitle(CharSequence charSequence) {
        mBookmarkSubtitleView.setText(charSequence);
    }

    void setPriceTrackingUiVisible(boolean visible) {
        mPriceTrackingContainer.setVisibility(visible ? View.VISIBLE : View.GONE);
        if (visible) {
            mBookmarkContainer.setBackgroundResource(
                    R.drawable.improved_bookmark_save_flow_multi_pane_top_background);
            mPriceTrackingContainer.setBackgroundResource(
                    R.drawable.improved_bookmark_save_flow_multi_pane_bottom_background);
        }
    }

    void setPriceTrackingUiEnabled(boolean enabled) {
        mPriceTrackingContainer.setEnabled(enabled);
    }

    void setPriceTrackingSwitchChecked(boolean checked) {
        mPriceTrackingSwitch.setChecked(checked);
    }

    void setPriceTrackingSwitchToggleListener(CompoundButton.OnCheckedChangeListener listener) {
        mPriceTrackingSwitch.setOnCheckedChangeListener(listener);
    }
}
