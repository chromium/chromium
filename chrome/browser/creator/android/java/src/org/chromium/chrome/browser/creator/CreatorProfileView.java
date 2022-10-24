// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.creator;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.ui.widget.ButtonCompat;

/**
 * View class for the Creator Profile section
 */
public class CreatorProfileView extends LinearLayout {
    private TextView mTitle;
    private TextView mUrl;
    private ButtonCompat mFollowButton;
    private ButtonCompat mFollowingButton;

    public CreatorProfileView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    public void setTitle(String title) {
        mTitle.setText(title);
    }

    public void setUrl(String url) {
        mUrl.setText(url);
    }

    public void setIsFollowedStatus(boolean isFollowed) {
        if (isFollowed) {
            // When the user follows a site
            mFollowButton.setVisibility(View.GONE);
            mFollowingButton.setVisibility(View.VISIBLE);
        } else {
            // When the user un-follows a site
            mFollowButton.setVisibility(View.VISIBLE);
            mFollowingButton.setVisibility(View.GONE);
        }
    }

    public void setFollowButtonOnClickListener(Runnable onClick) {
        mFollowButton.setOnClickListener((v) -> onClick.run());
    }

    public void setFollowingButtonOnClickListener(Runnable onClick) {
        mFollowingButton.setOnClickListener((v) -> onClick.run());
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mTitle = (TextView) findViewById(R.id.creator_name);
        mUrl = (TextView) findViewById(R.id.creator_url);
        mFollowButton = (ButtonCompat) findViewById(R.id.creator_follow_button);
        mFollowingButton = (ButtonCompat) findViewById(R.id.creator_following_button);
    }
}
