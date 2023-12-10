// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.creator;

import android.animation.Animator;
import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.content.Context;
import android.graphics.Rect;
import android.util.AttributeSet;
import android.view.TouchDelegate;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.appcompat.widget.TooltipCompat;

import org.chromium.components.browser_ui.widget.animation.CancelAwareAnimatorListener;
import org.chromium.ui.widget.ButtonCompat;

import java.util.ArrayList;
import java.util.Collection;

/** View class for the Creator Toolbar section */
public class CreatorToolbarView extends LinearLayout {
    private static final int ANIMATION_DURATION_MS = 300;
    private TextView mCreatorTitleToolbar;
    private FrameLayout mButtonsContainer;
    private ButtonCompat mFollowButton;
    private ButtonCompat mFollowingButton;
    private View mToolbarBottomBorder;
    private int mTouchSize;

    public CreatorToolbarView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    public void setTitle(String title) {
        mCreatorTitleToolbar.setText(title);
        TooltipCompat.setTooltipText(mCreatorTitleToolbar, title);
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

    public void setToolbarVisibility(boolean isVisible) {
        AnimatorSet fullAnimation = new AnimatorSet();
        Collection<Animator> animationsList = new ArrayList<>();
        if (isVisible) {
            animationsList.add(animateFadeInView(mCreatorTitleToolbar));
            animationsList.add(animateFadeInView(mButtonsContainer));
            animationsList.add(animateFadeInView(mToolbarBottomBorder));
        } else {
            animationsList.add(animateFadeOutView(mCreatorTitleToolbar));
            animationsList.add(animateFadeOutView(mButtonsContainer));
            animationsList.add(animateFadeOutView(mToolbarBottomBorder));
        }
        fullAnimation.playTogether(animationsList);
        fullAnimation.start();
    }

    public Animator animateFadeInView(View view) {
        view.setVisibility(View.VISIBLE);
        ObjectAnimator fadeInAnimation = ObjectAnimator.ofFloat(view, View.ALPHA, 0.0f, 1.0f);
        fadeInAnimation.setDuration(ANIMATION_DURATION_MS);
        fadeInAnimation.addListener(
                new CancelAwareAnimatorListener() {
                    @Override
                    public void onEnd(Animator animation) {
                        view.setVisibility(View.VISIBLE);
                    }
                });
        return fadeInAnimation;
    }

    public Animator animateFadeOutView(View view) {
        ObjectAnimator fadeOutAnimation = ObjectAnimator.ofFloat(view, View.ALPHA, 1.0f, 0.0f);
        fadeOutAnimation.setDuration(ANIMATION_DURATION_MS);
        fadeOutAnimation.addListener(
                new CancelAwareAnimatorListener() {
                    @Override
                    public void onEnd(Animator animation) {
                        view.setVisibility(View.GONE);
                    }
                });
        return fadeOutAnimation;
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
        mCreatorTitleToolbar = (TextView) findViewById(R.id.creator_title_toolbar);
        mButtonsContainer = (FrameLayout) findViewById(R.id.creator_all_buttons_toolbar);
        mFollowButton = (ButtonCompat) findViewById(R.id.creator_follow_button_toolbar);
        mFollowingButton = (ButtonCompat) findViewById(R.id.creator_following_button_toolbar);
        mToolbarBottomBorder = (View) findViewById(R.id.creator_toolbar_bottom_border);

        mTouchSize =
                getResources().getDimensionPixelSize(R.dimen.creator_toolbar_button_touch_size);

        mButtonsContainer.addOnLayoutChangeListener(
                (View v,
                        int left,
                        int top,
                        int right,
                        int bottom,
                        int oldLeft,
                        int oldTop,
                        int oldRight,
                        int oldBottom) -> adjustButtonTouchDelegates());
    }

    private void adjustButtonTouchDelegates() {
        if (mFollowButton.getVisibility() == View.VISIBLE) {
            adjustButtonTouchDelegate(mFollowButton);
        } else if (mFollowingButton.getVisibility() == View.VISIBLE) {
            adjustButtonTouchDelegate(mFollowingButton);
        }
    }

    private void adjustButtonTouchDelegate(ButtonCompat button) {
        Rect rect = new Rect();
        button.getHitRect(rect);

        int halfWidthDelta = Math.max((mTouchSize - button.getWidth()) / 2, 0);
        int halfHeightDelta = Math.max((mTouchSize - button.getHeight()) / 2, 0);

        rect.left -= halfWidthDelta;
        rect.right += halfWidthDelta;
        rect.top -= halfHeightDelta;
        rect.bottom += halfHeightDelta;

        mButtonsContainer.setTouchDelegate(new TouchDelegate(rect, button));
    }
}
