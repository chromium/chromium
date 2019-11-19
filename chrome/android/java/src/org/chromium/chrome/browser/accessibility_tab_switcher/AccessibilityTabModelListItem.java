// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.accessibility_tab_switcher;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.annotation.SuppressLint;
import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Bitmap;
import android.os.Handler;
import android.support.v7.content.res.AppCompatResources;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.GestureDetector;
import android.view.MotionEvent;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.widget.AbsListView;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.annotations.UsedByReflection;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabFavicon;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.ui.styles.ChromeColors;

/**
 * A widget that shows a single row of the {@link AccessibilityTabModelListView} list.
 * This list shows both the title of the {@link Tab} as well as a close button to close
 * the tab.
 */
public class AccessibilityTabModelListItem extends FrameLayout implements OnClickListener {
    private static final int CLOSE_ANIMATION_DURATION_MS = 100;
    private static final int DEFAULT_ANIMATION_DURATION_MS = 300;
    private static final int VELOCITY_SCALING_FACTOR = 150;
    private static final int CLOSE_TIMEOUT_MS = 4000;

    private int mCloseAnimationDurationMs;
    private int mDefaultAnimationDurationMs;
    private int mCloseTimeoutMs;
    // The last run animation (if non-null, it still might have already completed).
    private Animator mActiveAnimation;

    private final float mSwipeCommitDistance;
    private final float mFlingCommitDistance;
    private final int mDefaultLevel;
    private final int mIncognitoLevel;
    private final ColorStateList mDefaultIconColor;
    private final ColorStateList mIncognitoIconColor;
    private final ColorStateList mDefaultCloseIconColor;
    private final ColorStateList mIncognitoCloseIconColor;

    // Keeps track of how a tab was closed
    //  < 0 : swiped to the left.
    //  > 0 : swiped to the right.
    //  = 0 : closed with the close button.
    private float mSwipedAway;

    // The children on the standard view.
    private LinearLayout mTabContents;
    private TextView mTitleView;
    private TextView mDescriptionView;
    private ImageView mFaviconView;
    private ImageButton mCloseButton;

    // The children on the undo view.
    private LinearLayout mUndoContents;
    private Button mUndoButton;

    private Tab mTab;
    private boolean mCanUndo;
    private AccessibilityTabModelListItemListener mListener;
    private final GestureDetector mSwipeGestureDetector;
    private final int mDefaultHeight;
    private AccessibilityTabModelListView mCanScrollListener;
    private boolean mCloseButtonClicked;

    /**
     * An interface that exposes actions taken on this item.  The registered listener will be
     * sent selection and close events based on user input.
     */
    public interface AccessibilityTabModelListItemListener {
        /**
         * Called when a user clicks on this list item.
         * @param tabId The ID of the tab that this list item represents.
         */
        public void tabSelected(int tabId);

        /**
         * Called when a user clicks on the close button of this list item.
         * @param tabId The ID of the tab that this list item represents.
         */
        public void tabClosed(int tabId);

        /**
         * Called when the data corresponding to this list item has changed.
         * @param tabId The ID of the tab that this list item represents.
         */
        public void tabChanged(int tabId);

        /**
         * @return Whether or not the tab is scheduled to be closed.
         */
        public boolean hasPendingClosure(int tabId);

        /**
         * Schedule a tab to be closed in the future.
         * @param tabId The ID of the tab to close.
         */
        public void schedulePendingClosure(int tabId);

        /**
         * Cancel a tab's closure.
         * @param tabId The ID of the tab that should no longer be closed.
         */
        public void cancelPendingClosure(int tabId);
    }

    private final Runnable mCloseRunnable = new Runnable() {
        @Override
        public void run() {
            runCloseAnimation();
        }
    };

    private final Handler mHandler = new Handler();

    /**
     * Used with the swipe away and blink out animations to bring in the undo view.
     */
    private final AnimatorListenerAdapter mCloseAnimatorListener = new AnimatorListenerAdapter() {
        private boolean mIsCancelled;

        @Override
        public void onAnimationStart(Animator animation) {
            mIsCancelled = false;
        }

        @Override
        public void onAnimationCancel(Animator animation) {
            mIsCancelled = true;
            mCloseButtonClicked = false;
        }

        @Override
        public void onAnimationEnd(Animator animator) {
            if (mIsCancelled) return;

            mListener.schedulePendingClosure(mTab.getId());
            setTranslationX(0.f);
            setScaleX(1.f);
            setScaleY(1.f);
            setAlpha(0.f);
            showUndoView(true);
            runResetAnimation(false);
            mHandler.postDelayed(mCloseRunnable, mCloseTimeoutMs);
        }
    };

    /**
     * Used with the close animation to actually close a tab after it has shrunk away.
     */
    private final AnimatorListenerAdapter mActuallyCloseAnimatorListener =
            new AnimatorListenerAdapter() {
                private boolean mIsCancelled;

                @Override
                public void onAnimationStart(Animator animation) {
                    mIsCancelled = false;
                }

                @Override
                public void onAnimationCancel(Animator animation) {
                    mIsCancelled = true;
                    mCloseButtonClicked = false;
                }

                @Override
                public void onAnimationEnd(Animator animator) {
                    if (mIsCancelled) return;

                    showUndoView(false);
                    setAlpha(1.f);
                    mTabContents.setAlpha(1.f);
                    mUndoContents.setAlpha(1.f);
                    cancelRunningAnimation();
                    mListener.tabClosed(mTab.getId());
                }
            };

    /**
     * @param context The Context to build this widget in.
     * @param attrs The AttributeSet to use to build this widget.
     */
    public AccessibilityTabModelListItem(Context context, AttributeSet attrs) {
        super(context, attrs);
        mSwipeGestureDetector = new GestureDetector(context, new SwipeGestureListener());
        mSwipeCommitDistance = context.getResources().getDimension(R.dimen.swipe_commit_distance);
        mFlingCommitDistance = mSwipeCommitDistance / 3;

        mDefaultHeight =
                context.getResources().getDimensionPixelOffset(R.dimen.accessibility_tab_height);
        mDefaultIconColor = ChromeColors.getIconTint(context, false);
        mIncognitoIconColor =
                AppCompatResources.getColorStateList(context, R.color.default_icon_color_dark);
        mDefaultCloseIconColor =
                AppCompatResources.getColorStateList(context, R.color.default_icon_color_secondary);
        mIncognitoCloseIconColor =
                AppCompatResources.getColorStateList(context, R.color.white_alpha_70);
        mDefaultLevel = getResources().getInteger(R.integer.list_item_level_default);
        mIncognitoLevel = getResources().getInteger(R.integer.list_item_level_incognito);

        mCloseAnimationDurationMs = CLOSE_ANIMATION_DURATION_MS;
        mDefaultAnimationDurationMs = DEFAULT_ANIMATION_DURATION_MS;
        mCloseTimeoutMs = CLOSE_TIMEOUT_MS;
    }

    @Override
    public void onFinishInflate() {
        super.onFinishInflate();
        mTabContents = findViewById(R.id.tab_contents_modern);
        mTitleView = mTabContents.findViewById(R.id.title);
        mDescriptionView = mTabContents.findViewById(R.id.description);
        mFaviconView = mTabContents.findViewById(R.id.icon_view);
        mCloseButton = mTabContents.findViewById(R.id.close_btn_modern);
        mFaviconView.setBackgroundResource(R.drawable.list_item_icon_modern_bg);

        mUndoContents = (LinearLayout) findViewById(R.id.undo_contents);
        mUndoButton = (Button) findViewById(R.id.undo_button);

        setClickable(true);
        setFocusable(true);

        mCloseButton.setOnClickListener(this);
        mUndoButton.setOnClickListener(this);
        setOnClickListener(this);
    }

    /**
     * Sets the {@link Tab} this {@link View} will represent in the list.
     * @param tab     The {@link Tab} to represent.
     * @param canUndo Whether or not closing this {@link Tab} can be undone.
     */
    public void setTab(Tab tab, boolean canUndo) {
        if (mTab != null) mTab.removeObserver(mTabObserver);
        mTab = tab;
        tab.addObserver(mTabObserver);
        mCanUndo = canUndo;
        updateTabText();
        updateFavicon();
    }

    private void showUndoView(boolean showView) {
        if (showView && mCanUndo) {
            mUndoContents.setVisibility(View.VISIBLE);
            mTabContents.setVisibility(View.INVISIBLE);
        } else {
            mTabContents.setVisibility(View.VISIBLE);
            mUndoContents.setVisibility(View.INVISIBLE);
            updateTabText();
            updateFavicon();
        }
    }

    /**
     * Registers a listener to be notified of selection and close events taken on this list item.
     * @param listener The listener to be notified of selection and close events.
     */
    public void setListeners(AccessibilityTabModelListItemListener listener,
            AccessibilityTabModelListView canScrollListener) {
        mListener = listener;
        mCanScrollListener = canScrollListener;
    }

    private void updateTabText() {
        String title = null;
        String url = null;
        if (mTab != null) {
            title = mTab.getTitle();
            url = mTab.getUrl();
            if (TextUtils.isEmpty(title)) title = url;
        }
        if (TextUtils.isEmpty(title)) {
            title = getContext().getResources().getString(R.string.tab_loading_default_title);
        }

        if (!title.equals(mTitleView.getText())) mTitleView.setText(title);

        String accessibilityString =
                getContext().getString(R.string.accessibility_tabstrip_tab, title);
        if (!accessibilityString.equals(getContentDescription())) {
            setContentDescription(
                    getContext().getString(R.string.accessibility_tabstrip_tab, title));
            mCloseButton.setContentDescription(
                    getContext().getString(R.string.accessibility_tabstrip_btn_close_tab, title));
        }

        if (mTab.isIncognito()) {
            setBackgroundResource(R.color.dark_primary_color);
            mFaviconView.getBackground().setLevel(mIncognitoLevel);
            ApiCompatibilityUtils.setTextAppearance(mTitleView, R.style.TextAppearance_WhiteTitle1);
            ApiCompatibilityUtils.setTextAppearance(
                    mDescriptionView, R.style.TextAppearance_WhiteBody);
            ApiCompatibilityUtils.setImageTintList(mCloseButton, mIncognitoCloseIconColor);
        } else {
            setBackgroundResource(R.color.modern_primary_color);
            mFaviconView.getBackground().setLevel(mDefaultLevel);
            ApiCompatibilityUtils.setTextAppearance(mTitleView, R.style.TextAppearance_BlackTitle1);
            ApiCompatibilityUtils.setTextAppearance(
                    mDescriptionView, R.style.TextAppearance_BlackBody);
            ApiCompatibilityUtils.setImageTintList(mCloseButton, mDefaultCloseIconColor);
        }

        if (TextUtils.isEmpty(url)) {
            mDescriptionView.setVisibility(View.GONE);
        } else {
            mDescriptionView.setText(url);
            mDescriptionView.setVisibility(View.VISIBLE);
        }
    }

    private void updateFavicon() {
        if (mTab != null) {
            Bitmap bitmap = TabFavicon.getBitmap(mTab);
            if (bitmap != null) {
                // Don't tint favicon bitmaps.
                ApiCompatibilityUtils.setImageTintList(mFaviconView, null);
                mFaviconView.setImageBitmap(bitmap);
            } else {
                mFaviconView.setImageResource(R.drawable.ic_globe_24dp);
                ApiCompatibilityUtils.setImageTintList(
                        mFaviconView, mTab.isIncognito() ? mIncognitoIconColor : mDefaultIconColor);
            }
        }
    }

    @Override
    public void onClick(View v) {
        if (mListener == null) return;

        int tabId = mTab.getId();
        if (v == AccessibilityTabModelListItem.this && !mListener.hasPendingClosure(tabId)) {
            mListener.tabSelected(tabId);
        } else if (v == mCloseButton) {
            mCloseButtonClicked = true;
            if (mCanUndo) {
                runBlinkOutAnimation();
            } else {
                runCloseAnimation();
            }
        } else if (v == mUndoButton) {
            // Kill the close action.
            mHandler.removeCallbacks(mCloseRunnable);

            mListener.cancelPendingClosure(tabId);
            showUndoView(false);
            setAlpha(0.f);
            if (mSwipedAway > 0.f) {
                setTranslationX(getWidth());
                runResetAnimation(false);
            } else if (mSwipedAway < 0.f) {
                setTranslationX(-getWidth());
                runResetAnimation(false);
            } else {
                setScaleX(1.2f);
                setScaleY(0.f);
                runResetAnimation(true);
            }
        }
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        if (mTab != null) {
            updateFavicon();
            updateTabText();
            mTab.addObserver(mTabObserver);
        }
    }

    @Override
    protected void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        if (mTab != null) mTab.removeObserver(mTabObserver);
        cancelRunningAnimation();
    }

    private final TabObserver mTabObserver = new EmptyTabObserver() {
        @Override
        public void onClosingStateChanged(Tab tab, boolean closing) {
            // If the tab is closed through something other than interacting with the ListItem
            // itself (e.g. the tab strip), we need to notify the listener of the change.
            // See https://crbug.com/567863.
            if (closing && !mCloseButtonClicked) {
                if (mListener != null) {
                    mListener.tabChanged(tab.getId());
                }
            }
        }

        @Override
        public void onFaviconUpdated(Tab tab, Bitmap icon) {
            updateFavicon();
            notifyTabUpdated(tab);
        }

        @Override
        public void onTitleUpdated(Tab tab) {
            updateTabText();
            notifyTabUpdated(tab);
        }

        @Override
        public void onUrlUpdated(Tab tab) {
            updateTabText();
            notifyTabUpdated(tab);
        }
    };

    @Override
    public boolean onTouchEvent(MotionEvent e) {
        // If there is a pending close task, remove it.
        mHandler.removeCallbacks(mCloseRunnable);

        boolean handled = mSwipeGestureDetector.onTouchEvent(e);
        if (handled) return true;
        if (e.getActionMasked() == MotionEvent.ACTION_UP) {
            if (Math.abs(getTranslationX()) > mSwipeCommitDistance) {
                runSwipeAnimation(DEFAULT_ANIMATION_DURATION_MS);
            } else {
                runResetAnimation(false);
            }
            mCanScrollListener.setCanScroll(true);
            return true;
        }
        return super.onTouchEvent(e);
    }

    /**
     * This call is exposed for the benefit of the animators.
     *
     * @param height The height of the current view.
     */
    @SuppressLint("AnimatorKeep")
    @UsedByReflection("")
    public void setHeight(int height) {
        AbsListView.LayoutParams params = (AbsListView.LayoutParams) getLayoutParams();
        if (params == null) {
            params = new AbsListView.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, height);
        } else {
            if (params.height == height) return;
            params.height = height;
        }
        setLayoutParams(params);
    }

    /**
     * Used to reset the state because views are recycled.
     */
    public void resetState() {
        setTranslationX(0.f);
        setAlpha(1.f);
        setScaleX(1.f);
        setScaleY(1.f);
        setHeight(mDefaultHeight);
        cancelRunningAnimation();
        // Remove any callbacks.
        mHandler.removeCallbacks(mCloseRunnable);

        if (mListener != null) {
            boolean hasPendingClosure = mListener.hasPendingClosure(mTab.getId());
            showUndoView(hasPendingClosure);
            if (hasPendingClosure) mHandler.postDelayed(mCloseRunnable, mCloseTimeoutMs);
        } else {
            showUndoView(false);
        }
    }

    /**
     * Simple gesture listener to catch the scroll and fling gestures on the list item.
     */
    private class SwipeGestureListener extends GestureDetector.SimpleOnGestureListener {
        @Override
        public boolean onDown(MotionEvent e) {
            // Returns true so that we can handle events that start with an onDown.
            return true;
        }

        @Override
        public boolean onScroll(MotionEvent e1, MotionEvent e2, float distanceX, float distanceY) {
            // Don't scroll if we're waiting for user interaction.
            if (mListener.hasPendingClosure(mTab.getId())) return false;

            // Stop the ListView from scrolling vertically.
            mCanScrollListener.setCanScroll(false);

            float distance = e2.getX() - e1.getX();
            setTranslationX(distance + getTranslationX());
            setAlpha(1 - Math.abs(getTranslationX() / getWidth()));
            return true;
        }

        @Override
        public boolean onFling(MotionEvent e1, MotionEvent e2, float velocityX, float velocityY) {
            // Arbitrary threshold that feels right.
            if (Math.abs(getTranslationX()) < mFlingCommitDistance) return false;

            double velocityMagnitude = Math.sqrt(velocityX * velocityX + velocityY * velocityY);
            long closeTime =
                    (long) Math.abs((getWidth() / velocityMagnitude)) * VELOCITY_SCALING_FACTOR;
            runSwipeAnimation(Math.min(closeTime, mDefaultAnimationDurationMs));
            mCanScrollListener.setCanScroll(true);
            return true;
        }

        @Override
        public boolean onSingleTapConfirmed(MotionEvent e) {
            performClick();
            return true;
        }
    }

    @VisibleForTesting
    public void disableAnimations() {
        mCloseAnimationDurationMs = 0;
        mDefaultAnimationDurationMs = 0;
        mCloseTimeoutMs = 0;
    }

    @VisibleForTesting
    public boolean hasPendingClosure() {
        if (mListener != null) return mListener.hasPendingClosure(mTab.getId());
        return false;
    }

    private void runSwipeAnimation(long time) {
        cancelRunningAnimation();
        mSwipedAway = getTranslationX();

        ObjectAnimator swipe = ObjectAnimator.ofFloat(
                this, View.TRANSLATION_X, getTranslationX() > 0 ? getWidth() : -getWidth());
        ObjectAnimator fadeOut = ObjectAnimator.ofFloat(this, View.ALPHA, 0.f);

        AnimatorSet set = new AnimatorSet();
        set.playTogether(fadeOut, swipe);
        set.addListener(mCloseAnimatorListener);
        set.setDuration(Math.min(time, mDefaultAnimationDurationMs));
        set.start();

        mActiveAnimation = set;
    }

    private void runResetAnimation(boolean useCloseAnimationDuration) {
        cancelRunningAnimation();

        ObjectAnimator swipe = ObjectAnimator.ofFloat(this, View.TRANSLATION_X, 0.f);
        ObjectAnimator fadeIn = ObjectAnimator.ofFloat(this, View.ALPHA, 1.f);
        ObjectAnimator scaleX = ObjectAnimator.ofFloat(this, View.SCALE_X, 1.f);
        ObjectAnimator scaleY = ObjectAnimator.ofFloat(this, View.SCALE_Y, 1.f);
        ObjectAnimator resetHeight = ObjectAnimator.ofInt(this, "height", mDefaultHeight);

        AnimatorSet set = new AnimatorSet();
        set.playTogether(swipe, fadeIn, scaleX, scaleY, resetHeight);
        set.setDuration(useCloseAnimationDuration ? mCloseAnimationDurationMs
                                                  : mDefaultAnimationDurationMs);
        set.start();

        mActiveAnimation = set;
    }

    private void runBlinkOutAnimation() {
        cancelRunningAnimation();
        mSwipedAway = 0;

        ObjectAnimator stretchX = ObjectAnimator.ofFloat(this, View.SCALE_X, 1.2f);
        ObjectAnimator shrinkY = ObjectAnimator.ofFloat(this, View.SCALE_Y, 0.f);
        ObjectAnimator fadeOut = ObjectAnimator.ofFloat(this, View.ALPHA, 0.f);

        AnimatorSet set = new AnimatorSet();
        set.playTogether(fadeOut, shrinkY, stretchX);
        set.addListener(mCloseAnimatorListener);
        set.setDuration(mCloseAnimationDurationMs);
        set.start();

        mActiveAnimation = set;
    }

    private void runCloseAnimation() {
        cancelRunningAnimation();

        ObjectAnimator shrinkHeight = ObjectAnimator.ofInt(this, "height", 0);
        ObjectAnimator shrinkY = ObjectAnimator.ofFloat(this, View.SCALE_Y, 0.f);

        AnimatorSet set = new AnimatorSet();
        set.playTogether(shrinkHeight, shrinkY);
        set.addListener(mActuallyCloseAnimatorListener);
        set.setDuration(mDefaultAnimationDurationMs);
        set.start();

        mActiveAnimation = set;
    }

    private void cancelRunningAnimation() {
        if (mActiveAnimation != null && mActiveAnimation.isRunning()) mActiveAnimation.cancel();

        mActiveAnimation = null;
    }

    private void notifyTabUpdated(Tab tab) {
        if (mListener != null) mListener.tabChanged(tab.getId());
    }
}
