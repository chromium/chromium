// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.followmanagement;

import android.content.Context;
import android.graphics.Bitmap;
import android.util.AttributeSet;
import android.view.View;
import android.widget.CheckBox;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.chrome.browser.feed.webfeed.R;

/**
 * View class for the individual line items in the following management activity.
 */
public class FollowManagementItemView extends LinearLayout {
    private TextView mTitle;
    private TextView mUrl;
    private TextView mStatus;
    private boolean mChecked;
    private ImageView mFavicon;
    private CheckBox mSubscribedCheckbox;

    public FollowManagementItemView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    public void setTitle(String title) {
        mTitle.setText(title);
    }

    public void setUrl(String url) {
        mUrl.setText(url);
    }

    public void setStatus(String status) {
        mStatus.setText(status);
        if (status != null && status.isEmpty()) mStatus.setVisibility(View.GONE);
    }

    public void setFavicon(Bitmap favicon) {
        mFavicon.setImageBitmap(favicon);
    }

    /** Gets the current subscription state based on the checkbox status. */
    public boolean isSubscribed() {
        return mSubscribedCheckbox.isChecked();
    }

    /** Updates the checkbox state, enable it and starts listenint to clicks.  */
    public void setSubscribed(boolean subscribed) {
        mSubscribedCheckbox.setChecked(subscribed);
        mSubscribedCheckbox.setClickable(true);
        mSubscribedCheckbox.setEnabled(true);
    }

    /* Present the checkbox as disabled and checked while transitioning. And
     * stop listening for clicks to prevent duplicate events.
     */
    public void setTransitioning() {
        mSubscribedCheckbox.setChecked(true);
        mSubscribedCheckbox.setClickable(false);
        mSubscribedCheckbox.setEnabled(false);
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
