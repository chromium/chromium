// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.content_creation.reactions.scene;

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.drawable.LayerDrawable;
import android.util.AttributeSet;
import android.view.GestureDetector;
import android.view.MotionEvent;
import android.view.View;
import android.widget.ImageView;
import android.widget.RelativeLayout;

import org.chromium.chrome.browser.content_creation.reactions.ReactionGifDrawable;
import org.chromium.chrome.browser.content_creation.reactions.internal.R;
import org.chromium.ui.widget.ChromeImageButton;

/**
 * A Layout holding a Lightweight Reaction.
 */
public class ReactionLayout extends RelativeLayout {
    private final int mReactionPadding;
    private final Context mContext;

    private ChromeImageButton mAdjustButton;
    private ChromeImageButton mCopyButton;
    private ChromeImageButton mDeleteButton;
    private ReactionGifDrawable mDrawable;
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
     * @param drawable {@link ReactionGifDrawable} of the reaction.
     * @param sceneEditorDelegate {@link SceneEditorDelegate} to call scene editing methods.
     * @param localizedName The name of the reaction for accessibility.
     */
    void init(ReactionGifDrawable drawable, SceneEditorDelegate sceneEditorDelegate,
            String localizedName) {
        setDrawable(drawable, localizedName);
        mSceneEditorDelegate = sceneEditorDelegate;
        mIsActive = true;
        setUpReactionView();
    }

    void setDrawable(ReactionGifDrawable drawable, String localizedName) {
        mDrawable = drawable;
        mReaction.setImageDrawable(mDrawable);
        mReaction.setContentDescription(localizedName);
    }

    @Override
    public void onFinishInflate() {
        super.onFinishInflate();
        mReaction = findViewById(R.id.reaction_view);
        setUpAdjustButton();
        setUpCopyButton();
        setUpDeleteButton();
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

    public ReactionGifDrawable getReaction() {
        return mDrawable;
    }

    @SuppressLint("ClickableViewAccessibility")
    private void setUpReactionView() {
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
            private int mHeight;
            private int mWidth;

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
                int screenWidth = mContext.getResources().getDisplayMetrics().widthPixels;
                int screenHeight = mContext.getResources().getDisplayMetrics().heightPixels;
                switch (motionEvent.getAction()) {
                    case MotionEvent.ACTION_DOWN:
                        mSceneEditorDelegate.reactionWasMoved(ReactionLayout.this);
                        mBaseX = motionEvent.getRawX() - layoutParams.leftMargin;
                        mBaseY = motionEvent.getRawY() - layoutParams.topMargin;
                        mHeight = layoutParams.height;
                        mWidth = layoutParams.width;
                        break;
                    case MotionEvent.ACTION_MOVE:
                        layoutParams.leftMargin = (int) (motionEvent.getRawX() - mBaseX);
                        layoutParams.topMargin = (int) (motionEvent.getRawY() - mBaseY);
                        layoutParams.rightMargin = screenWidth - (layoutParams.leftMargin - mWidth);
                        layoutParams.bottomMargin =
                                screenHeight - (layoutParams.topMargin - mHeight);
                        ReactionLayout.this.setLayoutParams(layoutParams);
                        break;
                    case MotionEvent.ACTION_UP:
                        view.announceForAccessibility(mContext.getString(
                                R.string.lightweight_reactions_reaction_moved_announcement));
                        break;
                }
                return true;
            }
        });
    }

    @SuppressLint("ClickableViewAccessibility")
    private void setUpAdjustButton() {
        mAdjustButton = findViewById(R.id.adjust_button);
        mAdjustButton.setOnTouchListener(new OnTouchListener() {
            private float mBaseAngle;
            private float mBaseX;
            private float mBaseY;
            private int mBaseWidth;
            private int mBaseHeight;
            private double mCenterX;
            private double mCenterY;
            private double mInitialDistFromCenter;

            @Override
            public boolean onTouch(View view, MotionEvent motionEvent) {
                if (!mIsActive) {
                    return true;
                }
                RelativeLayout.LayoutParams layoutParams =
                        (RelativeLayout.LayoutParams) ReactionLayout.this.getLayoutParams();
                int screenWidth = mContext.getResources().getDisplayMetrics().widthPixels;
                int screenHeight = mContext.getResources().getDisplayMetrics().heightPixels;
                float x = motionEvent.getRawX();
                float y = motionEvent.getRawY();
                switch (motionEvent.getAction()) {
                    case MotionEvent.ACTION_DOWN:
                        mSceneEditorDelegate.reactionWasAdjusted();
                        mBaseAngle = ReactionLayout.this.getRotation();
                        mBaseX = x;
                        mBaseY = y;
                        mBaseWidth = layoutParams.width;
                        mBaseHeight = layoutParams.height;
                        mCenterX = layoutParams.leftMargin + mBaseWidth / 2.0;
                        mCenterY = layoutParams.topMargin + mBaseHeight / 2.0;
                        mInitialDistFromCenter = Math.hypot(mCenterX - mBaseX, mCenterY - mBaseY);
                        break;
                    case MotionEvent.ACTION_MOVE:
                        // Resize calculations
                        double currentDistFromCenter = Math.hypot(mCenterX - x, mCenterY - y);
                        double distRatio = currentDistFromCenter / mInitialDistFromCenter;
                        layoutParams.width = (int) (distRatio * mBaseWidth);
                        layoutParams.height = (int) (distRatio * mBaseHeight);
                        layoutParams.leftMargin = (int) (mCenterX - layoutParams.width / 2.0);
                        layoutParams.topMargin = (int) (mCenterY - layoutParams.height / 2.0);
                        layoutParams.rightMargin =
                                screenWidth - (layoutParams.leftMargin - layoutParams.width);
                        layoutParams.bottomMargin =
                                screenHeight - (layoutParams.topMargin - layoutParams.height);
                        ReactionLayout.this.setLayoutParams(layoutParams);

                        // Rotation calculations
                        double newAngle = (Math.toDegrees(Math.atan2(mCenterY - y, mCenterX - x))
                                                  - Math.toDegrees(Math.atan2(
                                                          mCenterY - mBaseY, mCenterX - mBaseX))
                                                  + mBaseAngle + 360.0)
                                % 360.0;
                        ReactionLayout.this.setRotation((float) newAngle);
                        break;
                    case MotionEvent.ACTION_UP:
                        view.announceForAccessibility(mContext.getString(
                                R.string.lightweight_reactions_reaction_adjusted_announcement));
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
                getContext().getColor(R.color.button_icon_color));

        mCopyButton.setOnClickListener(view -> {
            if (mSceneEditorDelegate.canAddReaction()) {
                mSceneEditorDelegate.duplicateReaction(ReactionLayout.this);
            } else {
                mSceneEditorDelegate.showMaxReactionsReachedToast();
            }
        });
    }

    private void setUpDeleteButton() {
        mDeleteButton = findViewById(R.id.delete_button);
        mDeleteButton.setOnClickListener(view -> {
            view.announceForAccessibility(mContext.getString(
                    R.string.lightweight_reactions_reaction_deleted_announcement));
            mSceneEditorDelegate.removeReaction(this);
        });
    }
}
