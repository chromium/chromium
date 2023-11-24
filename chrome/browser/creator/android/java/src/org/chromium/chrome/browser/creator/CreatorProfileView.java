// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.creator;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.appcompat.widget.TooltipCompat;

import org.chromium.ui.widget.ButtonCompat;

/** View class for the Creator Profile section */
public class CreatorProfileView extends LinearLayout {
    private static final int FADE_IN_ANIMATION_DURATION_MS = 150;
    private static final int FADE_OUT_ANIMATION_DURATION_MS = 300;
    private TextView mTitle;
    private TextView mUrl;
    private ButtonCompat mFollowButton;
    private ButtonCompat mFollowingButton;

    public CreatorProfileView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    public void setTitle(String title) {
        mTitle.setText(title);
        TooltipCompat.setTooltipText(mTitle, title);
    }

    public void setUrl(String formattedUrl) {
        mUrl.setText(formattedUrl);
        TooltipCompat.setTooltipText(mUrl, formattedUrl);
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

    public void setProfileVisibility(boolean isToolbarVisible) {
        if (isToolbarVisible) {
            setAlpha(1.0f);
            animate()
                    .alpha(0.0f)
                    .setDuration(FADE_OUT_ANIMATION_DURATION_MS)
                    .setListener(
                            new AnimatorListenerAdapter() {
                                @Override
                                public void onAnimationEnd(Animator animation) {
                                    mFollowButton.setEnabled(false);
                                    mFollowingButton.setEnabled(false);
                                }
                            })
                    .start();
        } else {
            setAlpha(0.0f);
            animate()
                    .alpha(1.0f)
                    .setDuration(FADE_IN_ANIMATION_DURATION_MS)
                    .setListener(
                            new AnimatorListenerAdapter() {
                                @Override
                                public void onAnimationStart(Animator animation) {
                                    mFollowButton.setEnabled(true);
                                    mFollowingButton.setEnabled(true);
                                }
                            })
                    .start();
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
