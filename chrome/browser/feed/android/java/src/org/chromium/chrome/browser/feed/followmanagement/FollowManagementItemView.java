// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.followmanagement;

import android.content.Context;
import android.graphics.Bitmap;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.View;
import android.widget.CheckBox;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.chrome.browser.feed.R;

/** View class for the individual line items in the following management activity. */
public class FollowManagementItemView extends LinearLayout {
    private TextView mTitle;
    private TextView mUrl;
    private TextView mStatus;
    private ImageView mFavicon;
    private CheckBox mSubscribedCheckbox;

    public FollowManagementItemView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    public void setTitle(String title) {
        mTitle.setText(title);
        mSubscribedCheckbox.setContentDescription(title);
    }

    public void setUrl(String url) {
        mUrl.setText(url);
    }

    public void setStatus(String status) {
        mStatus.setText(status);
        if (TextUtils.isEmpty(status)) {
            mStatus.setVisibility(View.GONE);
        } else {
            mStatus.setVisibility(View.VISIBLE);
        }
    }

    public void setFavicon(Bitmap favicon) {
        mFavicon.setImageBitmap(favicon);
    }

    /** Updates the checkbox state, enable it and starts listenint to clicks.  */
    public void setSubscribed(boolean subscribed) {
        mSubscribedCheckbox.setChecked(subscribed);
    }

    public void setCheckboxEnabled(boolean checkboxEnabled) {
        mSubscribedCheckbox.setClickable(checkboxEnabled);
        mSubscribedCheckbox.setEnabled(checkboxEnabled);
    }

    public void setCheckboxClickListener(Runnable onClick) {
        mSubscribedCheckbox.setOnClickListener((v) -> onClick.run());
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mTitle = (TextView) findViewById(R.id.follow_management_item_text);
        mUrl = (TextView) findViewById(R.id.follow_management_item_url);
        mStatus = (TextView) findViewById(R.id.follow_management_item_status);
        mFavicon = (ImageView) findViewById(R.id.follow_management_favicon);
        mSubscribedCheckbox = (CheckBox) findViewById(R.id.follow_management_subscribed_checkbox);
    }
}
