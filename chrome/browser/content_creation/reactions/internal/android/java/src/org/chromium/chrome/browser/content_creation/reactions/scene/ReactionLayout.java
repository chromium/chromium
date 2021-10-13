// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.content_creation.reactions.scene;

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.LayerDrawable;
import android.util.AttributeSet;
import android.view.GestureDetector;
import android.view.MotionEvent;
import android.view.View;
import android.widget.ImageView;
import android.widget.RelativeLayout;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.browser.content_creation.reactions.internal.R;
import org.chromium.ui.widget.ChromeImageButton;

class ReactionLayout extends RelativeLayout {
    private final int mReactionPadding;
    private final Context mContext;

    private ChromeImageButton mCopyButton;
    private ChromeImageButton mDeleteButton;
    private ChromeImageButton mAdjustButton;
    private Drawable mDrawable;
    private ImageView mReaction;
    private SceneEditorDelegate mSceneEditorDelegate;
    private boolean mIsActive;

    public ReactionLayout(Context context, AttributeSet attrs) {
        super(context, attrs);
        mContext = context;
        mReactionPadding = mContext.getResources().getDimensionPixelSize(R.dimen.reaction_padding);
    }

    /**
     * Initialize the ReactionLayout outside of the constructor since the Layout is inflated.
     * @param drawable {@link Drawable} of the reaction.
     * @param sceneEditorDelegate {@link SceneEditorDelegate} to call scene editing methods.
     */
    void init(Drawable drawable, SceneEditorDelegate sceneEditorDelegate) {
        mDrawable = drawable;
        mSceneEditorDelegate = sceneEditorDelegate;
        mIsActive = true;
        setUpReactionView();
    }

    @Override
    public void onFinishInflate() {
        super.onFinishInflate();
        mReaction = findViewById(R.id.reaction_view);
        setUpCopyButton();
        setUpDeleteButton();
        mAdjustButton = findViewById(R.id.adjust_button);
    }

    void setActive(boolean isActive) {
        if (mIsActive == isActive) return;

        mIsActive = isActive;
        int visibility = mIsActive ? View.VISIBLE : View.GONE;
        mCopyButton.setVisibility(visibility);
        mDeleteButton.setVisibility(visibility);
        mAdjustButton.setVisibility(visibility);
        if (!mIsActive) {
            mReaction.setBackground(null);
        } else {
            mReaction.setBackgroundResource(R.drawable.border_inset);
            mReaction.setPadding(
                    mReactionPadding, mReactionPadding, mReactionPadding, mReactionPadding);
        }
    }

    Drawable getReaction() {
        return mDrawable;
    }

    @SuppressLint("ClickableViewAccessibility")
    private void setUpReactionView() {
        mReaction.setImageDrawable(mDrawable);
        GestureDetector gestureDetector =
                new GestureDetector(mContext, new GestureDetector.SimpleOnGestureListener() {
                    @Override
                    public boolean onSingleTapUp(MotionEvent event) {
                        mSceneEditorDelegate.markActiveStatus(ReactionLayout.this, !mIsActive);
                        return true;
                    }
                });
        mReaction.setOnTouchListener(new OnTouchListener() {
            private float mBaseX;
            private float mBaseY;

            @Override
            public boolean onTouch(View view, MotionEvent motionEvent) {
                if (gestureDetector.onTouchEvent(motionEvent)) {
                    return true;
                }
                if (!mIsActive) {
                    return true;
                }
                RelativeLayout.LayoutParams layoutParams =
                        (RelativeLayout.LayoutParams) ReactionLayout.this.getLayoutParams();
                switch (motionEvent.getAction()) {
                    case MotionEvent.ACTION_DOWN:
                        mBaseX = motionEvent.getRawX() - layoutParams.leftMargin;
                        mBaseY = motionEvent.getRawY() - layoutParams.topMargin;
                        break;
                    case MotionEvent.ACTION_MOVE:
                        layoutParams.leftMargin = (int) (motionEvent.getRawX() - mBaseX);
                        layoutParams.topMargin = (int) (motionEvent.getRawY() - mBaseY);
                        ReactionLayout.this.setLayoutParams(layoutParams);
                        break;
                }
                return true;
            }
        });
    }

    private void setUpCopyButton() {
        mCopyButton = findViewById(R.id.copy_button);
        LayerDrawable copyDrawable = (LayerDrawable) mCopyButton.getDrawable();
        // Programmatically tint vector icons since this is impossible in the drawable's XML. Mutate
        // is called to prevent this from affecting other drawables using the same resource.
        copyDrawable.findDrawableByLayerId(R.id.icon).mutate().setTint(
                ApiCompatibilityUtils.getColor(getResources(), R.color.button_icon_color));

        mCopyButton.setOnClickListener(view -> {
            if (mSceneEditorDelegate.canAddReaction()) {
                mSceneEditorDelegate.duplicateReaction(ReactionLayout.this);
            }
        });
    }

    private void setUpDeleteButton() {
        mDeleteButton = findViewById(R.id.delete_button);
        mDeleteButton.setOnClickListener(view -> mSceneEditorDelegate.removeReaction(this));
    }
}
