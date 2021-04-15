// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.followmanagement;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.chrome.browser.feed.webfeed.R;

/**
 * View class for the individual line items in the following management activity.
 */
public class FollowManagementItemView extends LinearLayout {
    private TextView mTitle;
    private TextView mDescription;
    private boolean mChecked;

    public FollowManagementItemView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }
    public void setTitle(String title) {
        mTitle.setText(title);
    }
    public void setDescription(String description) {
        mDescription.setText(description);
    }
    // TODO(petewil): Add icon, checkbox..

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mTitle = (TextView) findViewById(R.id.follow_management_item_text);
        mDescription = (TextView) findViewById(R.id.follow_management_item_description);
    }
}
