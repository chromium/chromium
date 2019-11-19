// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.os.Build;
import android.util.AttributeSet;
import android.view.View;
import android.widget.ImageView;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.widget.ViewLookupCachingFrameLayout;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.ref.WeakReference;

/**
 * A view used in the grid tab switcher recycler view that caches commonly used views and handles
 * button setup and animation.
 */
public class ClosableTabGridView extends ViewLookupCachingFrameLayout {
    private static final long RESTORE_ANIMATION_DURATION_MS = 50;
    private static final float ZOOM_IN_SCALE = 0.8f;
    @IntDef({AnimationStatus.SELECTED_CARD_ZOOM_IN, AnimationStatus.SELECTED_CARD_ZOOM_OUT,
            AnimationStatus.HOVERED_CARD_ZOOM_IN, AnimationStatus.HOVERED_CARD_ZOOM_OUT,
            AnimationStatus.CARD_RESTORE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface AnimationStatus {
        int CARD_RESTORE = 0;
        int SELECTED_CARD_ZOOM_OUT = 1;
        int SELECTED_CARD_ZOOM_IN = 2;
        int HOVERED_CARD_ZOOM_OUT = 3;
        int HOVERED_CARD_ZOOM_IN = 4;
        int NUM_ENTRIES = 5;
    }

    private static WeakReference<Bitmap> sCloseButtonBitmapWeakRef;
    private boolean mIsAnimating;

    /** Default XML constructor. */
    public ClosableTabGridView(Context context, AttributeSet atts) {
        super(context, atts);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        ImageView actionButton = (ImageView) fastFindViewById(R.id.action_button);

        if (sCloseButtonBitmapWeakRef == null || sCloseButtonBitmapWeakRef.get() == null) {
            int closeButtonSize =
                    (int) getResources().getDimension(R.dimen.tab_grid_close_button_size);
            Bitmap bitmap = BitmapFactory.decodeResource(getResources(), R.drawable.btn_close);
            sCloseButtonBitmapWeakRef = new WeakReference<>(
                    Bitmap.createScaledBitmap(bitmap, closeButtonSize, closeButtonSize, true));
            bitmap.recycle();
        }
        actionButton.setImageBitmap(sCloseButtonBitmapWeakRef.get());
    }

    /**
     * Play the zoom-in and zoom-out animations for tab grid card.
     * @param status      The target animation status in {@link AnimationStatus}.
     * @param isSelected  Whether the scaling card is selected or not.
     */
    void scaleTabGridCardView(@AnimationStatus int status, boolean isSelected) {
        assert status < AnimationStatus.NUM_ENTRIES;

        final View backgroundView = fastFindViewById(R.id.background_view);
        final View contentView = fastFindViewById(R.id.content_view);
        final View selectedViewBelowLollipop = fastFindViewById(R.id.selected_view_below_lollipop);
        boolean isZoomIn = status == AnimationStatus.SELECTED_CARD_ZOOM_IN
                || status == AnimationStatus.HOVERED_CARD_ZOOM_IN;
        boolean isHovered = status == AnimationStatus.HOVERED_CARD_ZOOM_IN
                || status == AnimationStatus.HOVERED_CARD_ZOOM_OUT;
        boolean isRestore = status == AnimationStatus.CARD_RESTORE;
        long duration = isRestore ? RESTORE_ANIMATION_DURATION_MS
                                  : TabListRecyclerView.BASE_ANIMATION_DURATION_MS;
        float scale = isZoomIn ? ZOOM_IN_SCALE : 1f;
        View animateView = isHovered ? contentView : this;

        if (status == AnimationStatus.HOVERED_CARD_ZOOM_IN) {
            backgroundView.setVisibility(View.VISIBLE);
        }

        AnimatorSet scaleAnimator = new AnimatorSet();
        scaleAnimator.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator animation) {
                if (!isZoomIn) {
                    backgroundView.setVisibility(View.GONE);
                    if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.LOLLIPOP_MR1) {
                        selectedViewBelowLollipop.setVisibility(
                                isSelected ? View.VISIBLE : View.GONE);
                    }
                }
                mIsAnimating = false;
            }
        });

        ObjectAnimator scaleX = ObjectAnimator.ofFloat(animateView, View.SCALE_X, scale);
        ObjectAnimator scaleY = ObjectAnimator.ofFloat(animateView, View.SCALE_Y, scale);
        scaleX.setDuration(duration);
        scaleY.setDuration(duration);
        scaleAnimator.play(scaleX).with(scaleY);
        mIsAnimating = true;
        scaleAnimator.start();
    }

    @VisibleForTesting
    boolean getIsAnimatingForTesting() {
        return mIsAnimating;
    }
}
