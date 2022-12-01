// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.creator;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.LinearLayout;

import androidx.appcompat.widget.Toolbar;

import org.chromium.ui.widget.ButtonCompat;

/**
 * View class for the Creator Toolbar section
 */
public class CreatorToolbarView extends LinearLayout {
    private Toolbar mActionBar;
    private FrameLayout mButtonsContainer;
    private ButtonCompat mFollowButton;
    private ButtonCompat mFollowingButton;

    public CreatorToolbarView(Context context, AttributeSet attrs) {
        super(context, attrs);
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

    public void setToolbarVisibility(boolean isVisible, String title) {
        if (isVisible) {
            mActionBar.setTitle(title);
            mButtonsContainer.setVisibility(View.VISIBLE);
        } else {
            mActionBar.setTitle("");
            mButtonsContainer.setVisibility(View.GONE);
        }
    }

    public void setFollowButtonToolbarOnClickListener(Runnable onClick) {
        mFollowButton.setOnClickListener((v) -> onClick.run());
    }

    public void setFollowingButtonToolbarOnClickListener(Runnable onClick) {
        mFollowingButton.setOnClickListener((v) -> onClick.run());
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mActionBar = (Toolbar) findViewById(R.id.action_bar);
        mButtonsContainer = (FrameLayout) findViewById(R.id.creator_all_buttons_toolbar);
        mFollowButton = (ButtonCompat) findViewById(R.id.creator_follow_button_toolbar);
        mFollowingButton = (ButtonCompat) findViewById(R.id.creator_following_button_toolbar);
    }
}
