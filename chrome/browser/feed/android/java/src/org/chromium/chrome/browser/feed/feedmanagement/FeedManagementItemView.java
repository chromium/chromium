// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.feedmanagement;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.chrome.browser.feed.R;

/** View class for the individual line items in the feed management interstitial. */
public class FeedManagementItemView extends LinearLayout {
    private TextView mTitle;
    private TextView mDescription;

    public void setTitle(String title) {
        mTitle.setText(title);
    }

    public void setDescription(String description) {
        mDescription.setText(description);
    }

    public FeedManagementItemView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mTitle = (TextView) findViewById(R.id.feed_management_menu_item_text);
        mDescription = (TextView) findViewById(R.id.feed_management_menu_item_description);
    }
}
