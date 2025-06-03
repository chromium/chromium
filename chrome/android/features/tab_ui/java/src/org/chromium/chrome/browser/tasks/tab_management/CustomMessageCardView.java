// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.BASE_ANIMATION_DURATION_MS;

import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup.LayoutParams;
import android.widget.LinearLayout;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tasks.tab_management.TabListModel.AnimationStatus;
import org.chromium.ui.animation.AnimationHandler;

/**
 * Represents an empty custom message card view in the Grid Tab Switcher. This view supports
 * attaching a custom message card design to an empty message card view and displaying it. This view
 * is not responsible for handling the attached child view's model and subsequent functionality.
 */
@NullMarked
public class CustomMessageCardView extends LinearLayout {
    public CustomMessageCardView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    private final AnimationHandler mAnimationHandler = new AnimationHandler();

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
    }

    /**
     * Sets the custom view to be displayed as a child view of this message card view.
     *
     * @param view The view to be displayed.
     */
    public void setChildView(View view) {
        addView(
                view,
                new LinearLayout.LayoutParams(
                        LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));
    }

    /**
     * Sets the action button visibility.
     *
     * @param status The type of scaling to perform.
     */
    void scaleCard(@AnimationStatus int status) {
        boolean isZoomIn = status == AnimationStatus.HOVERED_CARD_ZOOM_IN;
        boolean isZoomOut = status == AnimationStatus.HOVERED_CARD_ZOOM_OUT;
        if (!isZoomOut && !isZoomIn) return;

        float scale = isZoomIn ? 0.8f : 1f;

        AnimatorSet scaleAnimator = new AnimatorSet();
        ObjectAnimator scaleX = ObjectAnimator.ofFloat(this, View.SCALE_X, scale);
        ObjectAnimator scaleY = ObjectAnimator.ofFloat(this, View.SCALE_Y, scale);
        scaleX.setDuration(BASE_ANIMATION_DURATION_MS);
        scaleY.setDuration(BASE_ANIMATION_DURATION_MS);
        scaleAnimator.playTogether(scaleX, scaleY);

        mAnimationHandler.startAnimation(scaleAnimator);
    }
}
