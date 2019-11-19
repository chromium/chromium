// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.animation.ValueAnimator;
import android.annotation.SuppressLint;
import android.content.Context;
import android.content.res.Resources;
import android.text.TextUtils;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.widget.FrameLayout;

import androidx.annotation.IntDef;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.infobar.InfoBarContainer.InfoBarAnimationListener;
import org.chromium.ui.widget.OptimizedFrameLayout;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;

/**
 * Layout that displays infobars in a stack. Handles all the animations when adding or removing
 * infobars and when swapping infobar contents.
 *
 * The first infobar to be added is visible at the front of the stack. Later infobars peek up just
 * enough behind the front infobar to signal their existence; their contents aren't visible at all.
 * The stack has a max depth of three infobars. If additional infobars are added beyond this, they
 * won't be visible at all until infobars in front of them are dismissed.
 *
 * Animation details:
 *  - Newly added infobars slide up from the bottom and then their contents fade in.
 *  - Disappearing infobars slide down and away. The remaining infobars, if any, resize to the
 *    new front infobar's size, then the content of the new front infobar fades in.
 *  - When swapping the front infobar's content, the old content fades out, the infobar resizes to
 *    the new content's size, then the new content fades in.
 *  - Only a single animation happens at a time. If several infobars are added and/or removed in
 *    quick succession, the animations will be queued and run sequentially.
 *
 * Note: this class depends only on Android view code; it intentionally does not depend on any other
 * infobar code. This is an explicit design decision and should remain this way.
 *
 * TODO(newt): what happens when detached from window? Do animations run? Do animations jump to end
 *     values? Should they jump to end values? Does requestLayout() get called when detached
 *     from window? Probably not; it probably just gets called later when reattached.
 *
 * TODO(newt): use hardware acceleration? See
 *     http://blog.danlew.net/2015/10/20/using-hardware-layers-to-improve-animation-performance/
 *     and http://developer.android.com/guide/topics/graphics/hardware-accel.html#layers
 *
 * TODO(newt): handle tall infobars on small devices. Use a ScrollView inside the InfoBarWrapper?
 *     Make sure InfoBarContainerLayout doesn't extend into tabstrip on tablet.
 *
 * TODO(newt): Disable key events during animations, perhaps by overriding dispatchKeyEvent().
 *     Or can we just call setEnabled() false on the infobar wrapper? Will this cause the buttons
 *     visual state to change (i.e. to turn gray)?
 *
 * TODO(newt): finalize animation timings and interpolators.
 */
public class InfoBarContainerLayout extends OptimizedFrameLayout {
    /**
     * An interface for items that can be added to an InfoBarContainerLayout.
     */
    public interface Item {
        // The infobar priority.
        @IntDef({InfoBarPriority.CRITICAL, InfoBarPriority.USER_TRIGGERED,
                InfoBarPriority.PAGE_TRIGGERED, InfoBarPriority.BACKGROUND})
        @Retention(RetentionPolicy.SOURCE)
        public @interface InfoBarPriority {
            int CRITICAL = 0;
            int USER_TRIGGERED = 1;
            int PAGE_TRIGGERED = 2;
            int BACKGROUND = 3;
        }

        /**
         * Returns the View that represents this infobar. This should have no background or borders;
         * a background and shadow will be added by a wrapper view.
         */
        View getView();

        /**
         * Returns whether controls for this View should be clickable. If false, all input events on
         * this item will be ignored.
         */
        boolean areControlsEnabled();

        /**
         * Sets whether or not controls for this View should be clickable. This does not affect the
         * visual state of the infobar.
         * @param state If false, all input events on this Item will be ignored.
         */
        void setControlsEnabled(boolean state);

        /**
         * Returns the accessibility text to announce when this infobar is first shown.
         */
        CharSequence getAccessibilityText();

        /**
         * Returns the priority of an infobar. High priority infobar is shown in front of low
         * priority infobar. If infobars have the same priorities, the most recently added one
         * is shown behind previous ones.
         *
         */
        int getPriority();

        /**
         * Returns the type of infobar, as best as can be determined at this time.  See
         * components/infobars/core/infobar_delegate.h.
         */
        @InfoBarIdentifier
        int getInfoBarIdentifier();
    }

    /**
     * Creates an empty InfoBarContainerLayout.
     */
    InfoBarContainerLayout(Context context, Runnable makeContainerVisibleRunnable,
            InfoBarAnimationListener animationListener) {
        super(context, null);
        Resources res = context.getResources();
        mBackInfobarHeight = res.getDimensionPixelSize(R.dimen.infobar_peeking_height);
        mFloatingBehavior = new FloatingBehavior(this);
        mAnimationListener = animationListener;
        mMakeContainerVisibleRunnable = makeContainerVisibleRunnable;
    }

    /**
     * Adds an infobar to the container. The infobar appearing animation will happen after the
     * current animation, if any, finishes.
     */
    void addInfoBar(Item item) {
        mItems.add(findInsertIndex(item), item);
        processPendingAnimations();
    }

    /**
     * Finds the appropriate index in the infobar stack for inserting this item.
     * @param item The infobar to be inserted.
     */
    private int findInsertIndex(Item item) {
        for (int i = 0; i < mItems.size(); ++i) {
            if (item.getPriority() < mItems.get(i).getPriority()) {
                return i;
            }
        }

        return mItems.size();
    }

    /**
     * Removes an infobar from the container. The infobar will be animated off the screen if it's
     * currently visible.
     */
    void removeInfoBar(Item item) {
        mItems.remove(item);
        processPendingAnimations();
    }

    /**
     * Notifies that an infobar's View ({@link Item#getView}) has changed. If the
     * infobar is visible in the front of the stack, the infobar will fade out the old contents,
     * resize, then fade in the new contents.
     */
    void notifyInfoBarViewChanged() {
        processPendingAnimations();
    }

    /**
     * Returns true if any animations are pending or in progress.
     */
    boolean isAnimating() {
        return mAnimation != null;
    }

    /////////////////////////////////////////
    // Implementation details
    /////////////////////////////////////////

    /** The maximum number of infobars visible at any time. */
    private static final int MAX_STACK_DEPTH = 3;

    // Animation durations.
    private static final int DURATION_SLIDE_UP_MS = 250;
    private static final int DURATION_SLIDE_DOWN_MS = 250;
    private static final int DURATION_FADE_MS = 100;
    private static final int DURATION_FADE_OUT_MS = 200;

    /**
     * Base class for animations inside the InfoBarContainerLayout.
     *
     * Provides a standardized way to prepare for, run, and clean up after animations. Each subclass
     * should implement prepareAnimation(), createAnimator(), and onAnimationEnd() as needed.
     */
    private abstract class InfoBarAnimation {
        private Animator mAnimator;

        final boolean isStarted() {
            return mAnimator != null;
        }

        final void start() {
            Animator.AnimatorListener listener = new AnimatorListenerAdapter() {
                @Override
                public void onAnimationEnd(Animator animation) {
                    InfoBarAnimation.this.onAnimationEnd();
                    mAnimation = null;
                    mAnimationListener.notifyAnimationFinished(getAnimationType());
                    processPendingAnimations();
                }
            };

            mAnimator = createAnimator();
            mAnimator.addListener(listener);
            mAnimator.start();
        }

        /**
         * Returns an animator that animates an InfoBarWrapper's y-translation from its current
         * value to endValue and updates the side shadow positions on each frame.
         */
        ValueAnimator createTranslationYAnimator(final InfoBarWrapper wrapper, float endValue) {
            ValueAnimator animator = ValueAnimator.ofFloat(wrapper.getTranslationY(), endValue);
            animator.addUpdateListener(new ValueAnimator.AnimatorUpdateListener() {
                @Override
                public void onAnimationUpdate(ValueAnimator animation) {
                    wrapper.setTranslationY((float) animation.getAnimatedValue());
                    mFloatingBehavior.updateShadowPosition();
                }
            });
            return animator;
        }

        /**
         * Called before the animation begins. This is the time to add views to the hierarchy and
         * adjust layout parameters.
         */
        void prepareAnimation() {}

        /**
         * Called to create an Animator which will control the animation. Called after
         * prepareAnimation() and after a subsequent layout has happened.
         */
        abstract Animator createAnimator();

        /**
         * Called after the animation completes. This is the time to do post-animation cleanup, such
         * as removing views from the hierarchy.
         */
        void onAnimationEnd() {}

        /**
         * Returns the InfoBarAnimationListener.ANIMATION_TYPE_* constant that corresponds to this
         * type of animation (showing, swapping, etc).
         */
        abstract int getAnimationType();
    }

    /**
     * The animation to show the first infobar. The infobar slides up from the bottom; then its
     * content fades in.
     */
    private class FirstInfoBarAppearingAnimation extends InfoBarAnimation {
        private Item mFrontItem;
        private InfoBarWrapper mFrontWrapper;
        private View mFrontContents;

        FirstInfoBarAppearingAnimation(Item frontItem) {
            mFrontItem = frontItem;
        }

        @Override
        void prepareAnimation() {
            mFrontContents = mFrontItem.getView();
            mFrontWrapper = new InfoBarWrapper(getContext(), mFrontItem);
            mFrontWrapper.addView(mFrontContents);
            addWrapper(mFrontWrapper);
        }

        @Override
        Animator createAnimator() {
            mFrontWrapper.setTranslationY(mFrontWrapper.getHeight());
            mFrontContents.setAlpha(0f);

            AnimatorSet animator = new AnimatorSet();
            animator.playSequentially(
                    createTranslationYAnimator(mFrontWrapper, 0f)
                            .setDuration(DURATION_SLIDE_UP_MS),
                    ObjectAnimator.ofFloat(mFrontContents, View.ALPHA, 1f)
                            .setDuration(DURATION_FADE_MS));
            return animator;
        }

        @Override
        void onAnimationEnd() {
            announceForAccessibility(mFrontItem.getAccessibilityText());
        }

        @Override
        int getAnimationType() {
            return InfoBarAnimationListener.ANIMATION_TYPE_SHOW;
        }
    }

    /**
     * The animation to show the a new front-most infobar in front of existing visible infobars. The
     * infobar slides up from the bottom; then its content fades in. The previously visible infobars
     * will be resized simulatenously to the new desired size.
     */
    private class FrontInfoBarAppearingAnimation extends InfoBarAnimation {
        private Item mFrontItem;
        private InfoBarWrapper mFrontWrapper;
        private InfoBarWrapper mOldFrontWrapper;
        private View mFrontContents;

        FrontInfoBarAppearingAnimation(Item frontItem) {
            mFrontItem = frontItem;
        }

        @Override
        void prepareAnimation() {
            mOldFrontWrapper = mInfoBarWrappers.get(0);

            mFrontContents = mFrontItem.getView();
            mFrontWrapper = new InfoBarWrapper(getContext(), mFrontItem);
            mFrontWrapper.addView(mFrontContents);
            addWrapperToFront(mFrontWrapper);
        }

        @Override
        Animator createAnimator() {
            // After adding the new wrapper, the new front item's view, and the old front item's
            // view are both in their wrappers, and the height of the stack as determined by
            // FrameLayout will take both into account. This means the height of the container will
            // be larger than it needs to be, if the previous old front item is larger than the sum
            // of the new front item and mBackInfobarHeight.
            //
            // First work out how much the container will grow or shrink by.
            int heightDelta =
                    mFrontWrapper.getHeight() + mBackInfobarHeight - mOldFrontWrapper.getHeight();

            // Now work out where to animate the new front item to / from.
            int newFrontStart = mFrontWrapper.getHeight();
            int newFrontEnd = 0;
            if (heightDelta < 0) {
                // If the container is shrinking, this won't be reflected in the layout just yet.
                // The layout will have extra space in it for the previous front infobar, which the
                // animation of the new front infobar has to take into account.
                newFrontStart -= heightDelta;
                newFrontEnd -= heightDelta;
            }
            mFrontWrapper.setTranslationY(newFrontStart);
            mFrontContents.setAlpha(0f);

            // Since we are adding the infobar to the top of the stack, make the container fully
            // visible since it could be at hidden or partially hidden state.
            mMakeContainerVisibleRunnable.run();

            AnimatorSet animator = new AnimatorSet();
            animator.play(createTranslationYAnimator(mFrontWrapper,
                    newFrontEnd).setDuration(DURATION_SLIDE_UP_MS));

            // If the container is shrinking, the back infobars need to animate down (from 0 to the
            // positive delta). Otherwise they have to animate up (from the negative delta to 0).
            int backStart = Math.max(0, heightDelta);
            int backEnd = Math.max(-heightDelta, 0);
            for (int i = 1; i < mInfoBarWrappers.size(); i++) {
                mInfoBarWrappers.get(i).setTranslationY(backStart);
                animator.play(createTranslationYAnimator(mInfoBarWrappers.get(i),
                        backEnd).setDuration(DURATION_SLIDE_UP_MS));
            }

            animator.play(ObjectAnimator.ofFloat(mFrontContents, View.ALPHA, 1f)
                                    .setDuration(DURATION_FADE_MS))
                    .after(DURATION_SLIDE_UP_MS);

            return animator;
        }

        @Override
        void onAnimationEnd() {
            // Remove the old front wrappers view so it won't affect the height of the container any
            // more.
            mOldFrontWrapper.removeAllViews();

            // Now set any Y offsets to 0 as there is no need to account for the old front wrapper
            // making the container higher than it should be.
            for (int i = 0; i < mInfoBarWrappers.size(); i++) {
                mInfoBarWrappers.get(i).setTranslationY(0);
            }
            updateLayoutParams();
            announceForAccessibility(mFrontItem.getAccessibilityText());
        }

        @Override
        int getAnimationType() {
            return InfoBarAnimationListener.ANIMATION_TYPE_SHOW;
        }
    }

    /**
     * The animation to show a back infobar. The infobar slides up behind the existing infobars, so
     * its top edge peeks out just a bit.
     */
    private class BackInfoBarAppearingAnimation extends InfoBarAnimation {
        private InfoBarWrapper mAppearingWrapper;

        BackInfoBarAppearingAnimation(Item appearingItem) {
            mAppearingWrapper = new InfoBarWrapper(getContext(), appearingItem);
        }

        @Override
        void prepareAnimation() {
            addWrapper(mAppearingWrapper);
        }

        @Override
        Animator createAnimator() {
            mAppearingWrapper.setTranslationY(mAppearingWrapper.getHeight());
            return createTranslationYAnimator(mAppearingWrapper, 0f)
                    .setDuration(DURATION_SLIDE_UP_MS);
        }

        @Override
        public void onAnimationEnd() {
            mAppearingWrapper.removeView(mAppearingWrapper.getItem().getView());
        }

        @Override
        int getAnimationType() {
            return InfoBarAnimationListener.ANIMATION_TYPE_SHOW;
        }
    }

    /**
     * The animation to hide the front infobar and reveal the second-to-front infobar. The front
     * infobar slides down and off the screen. The back infobar(s) will adjust to the size of the
     * new front infobar, and then the new front infobar's contents will fade in.
     */
    private class FrontInfoBarDisappearingAndRevealingAnimation extends InfoBarAnimation {
        private InfoBarWrapper mOldFrontWrapper;
        private InfoBarWrapper mNewFrontWrapper;
        private View mNewFrontContents;

        @Override
        void prepareAnimation() {
            mOldFrontWrapper = mInfoBarWrappers.get(0);
            mNewFrontWrapper = mInfoBarWrappers.get(1);
            mNewFrontContents = mNewFrontWrapper.getItem().getView();
            mNewFrontWrapper.addView(mNewFrontContents);
        }

        @Override
        Animator createAnimator() {
            // The amount by which mNewFrontWrapper will grow (negative value indicates shrinking).
            int deltaHeight = (mNewFrontWrapper.getHeight() - mBackInfobarHeight)
                    - mOldFrontWrapper.getHeight();
            int startTranslationY = Math.max(deltaHeight, 0);
            int endTranslationY = Math.max(-deltaHeight, 0);

            // Slide the front infobar down and away.
            AnimatorSet animator = new AnimatorSet();
            mOldFrontWrapper.setTranslationY(startTranslationY);
            animator.play(createTranslationYAnimator(mOldFrontWrapper,
                    startTranslationY + mOldFrontWrapper.getHeight())
                    .setDuration(DURATION_SLIDE_UP_MS));

            // Slide the other infobars to their new positions.
            // Note: animator.play() causes these animations to run simultaneously.
            for (int i = 1; i < mInfoBarWrappers.size(); i++) {
                mInfoBarWrappers.get(i).setTranslationY(startTranslationY);
                animator.play(createTranslationYAnimator(mInfoBarWrappers.get(i),
                        endTranslationY).setDuration(DURATION_SLIDE_UP_MS));
            }

            mNewFrontContents.setAlpha(0f);
            animator.play(ObjectAnimator.ofFloat(mNewFrontContents, View.ALPHA, 1f)
                    .setDuration(DURATION_FADE_MS)).after(DURATION_SLIDE_UP_MS);

            return animator;
        }

        @Override
        void onAnimationEnd() {
            mOldFrontWrapper.removeAllViews();
            removeWrapper(mOldFrontWrapper);
            for (int i = 0; i < mInfoBarWrappers.size(); i++) {
                mInfoBarWrappers.get(i).setTranslationY(0);
            }
            announceForAccessibility(mNewFrontWrapper.getItem().getAccessibilityText());
        }

        @Override
        int getAnimationType() {
            return InfoBarAnimationListener.ANIMATION_TYPE_HIDE;
        }
    }

    /**
     * The animation to hide the backmost infobar, or the front infobar if there's only one infobar.
     * The infobar simply slides down out of the container.
     */
    private class InfoBarDisappearingAnimation extends InfoBarAnimation {
        private InfoBarWrapper mDisappearingWrapper;

        @Override
        void prepareAnimation() {
            mDisappearingWrapper = mInfoBarWrappers.get(mInfoBarWrappers.size() - 1);
        }

        @Override
        Animator createAnimator() {
            return createTranslationYAnimator(mDisappearingWrapper,
                    mDisappearingWrapper.getHeight())
                    .setDuration(DURATION_SLIDE_DOWN_MS);
        }

        @Override
        void onAnimationEnd() {
            mDisappearingWrapper.removeAllViews();
            removeWrapper(mDisappearingWrapper);
        }

        @Override
        int getAnimationType() {
            return InfoBarAnimationListener.ANIMATION_TYPE_HIDE;
        }
    }

    /**
     * The animation to swap the contents of the front infobar. The current contents fade out,
     * then the infobar resizes to fit the new contents, then the new contents fade in.
     */
    private class FrontInfoBarSwapContentsAnimation extends InfoBarAnimation {
        private InfoBarWrapper mFrontWrapper;
        private View mOldContents;
        private View mNewContents;

        @Override
        void prepareAnimation() {
            mFrontWrapper = mInfoBarWrappers.get(0);
            mOldContents = mFrontWrapper.getChildAt(0);
            mNewContents = mFrontWrapper.getItem().getView();
            mFrontWrapper.addView(mNewContents);
        }

        @Override
        Animator createAnimator() {
            int deltaHeight = mNewContents.getHeight() - mOldContents.getHeight();
            InfoBarContainerLayout.this.setTranslationY(Math.max(0, deltaHeight));
            mNewContents.setAlpha(0f);

            AnimatorSet animator = new AnimatorSet();
            animator.playSequentially(
                    ObjectAnimator.ofFloat(mOldContents, View.ALPHA, 0f)
                            .setDuration(DURATION_FADE_OUT_MS),
                    ObjectAnimator.ofFloat(InfoBarContainerLayout.this, View.TRANSLATION_Y,
                            Math.max(0, -deltaHeight)).setDuration(DURATION_SLIDE_UP_MS),
                    ObjectAnimator.ofFloat(mNewContents, View.ALPHA, 1f)
                            .setDuration(DURATION_FADE_OUT_MS));
            return animator;
        }

        @Override
        void onAnimationEnd() {
            mFrontWrapper.removeViewAt(0);
            InfoBarContainerLayout.this.setTranslationY(0f);
            mFrontWrapper.getItem().setControlsEnabled(true);
            announceForAccessibility(mFrontWrapper.getItem().getAccessibilityText());
        }

        @Override
        int getAnimationType() {
            return InfoBarAnimationListener.ANIMATION_TYPE_SWAP;
        }
    }

    /**
     * Controls whether infobars fill the full available width, or whether they "float" in the
     * middle of the available space. The latter case happens if the available space is wider than
     * the max width allowed for infobars.
     *
     * Also handles the shadows on the sides of the infobars in floating mode. The side shadows are
     * separate views -- rather than being part of each InfoBarWrapper -- to avoid a double-shadow
     * effect, which would happen during animations when two InfoBarWrappers overlap each other.
     */
    private static class FloatingBehavior {
        /** The InfoBarContainerLayout. */
        private FrameLayout mLayout;

        /**
         * The max width of the infobars. If the available space is wider than this, the infobars
         * will switch to floating mode.
         */
        private final int mMaxWidth;

        /** The width of the left and right shadows. */
        private final int mShadowWidth;

        /** Whether the layout is currently floating. */
        private boolean mIsFloating;

        /** The shadows that appear on the sides of the infobars in floating mode. */
        private View mLeftShadowView;
        private View mRightShadowView;

        FloatingBehavior(FrameLayout layout) {
            mLayout = layout;
            Resources res = mLayout.getContext().getResources();
            mMaxWidth = res.getDimensionPixelSize(R.dimen.infobar_max_width);
            mShadowWidth = res.getDimensionPixelSize(R.dimen.infobar_shadow_width);
        }

        /**
         * This should be called in onMeasure() before super.onMeasure(). The return value is a new
         * widthMeasureSpec that should be passed to super.onMeasure().
         */
        int beforeOnMeasure(int widthMeasureSpec) {
            int width = MeasureSpec.getSize(widthMeasureSpec);
            boolean isFloating = width > mMaxWidth;
            if (isFloating != mIsFloating) {
                mIsFloating = isFloating;
                onIsFloatingChanged();
            }

            if (isFloating) {
                int mode = MeasureSpec.getMode(widthMeasureSpec);
                width = Math.min(width, mMaxWidth + 2 * mShadowWidth);
                widthMeasureSpec = MeasureSpec.makeMeasureSpec(width, mode);
            }
            return widthMeasureSpec;
        }

        /**
         * This should be called in onMeasure() after super.onMeasure().
         */
        void afterOnMeasure(int measuredHeight) {
            if (!mIsFloating) return;
            // Measure side shadows to match the parent view's height.
            int widthSpec = MeasureSpec.makeMeasureSpec(mShadowWidth, MeasureSpec.EXACTLY);
            int heightSpec = MeasureSpec.makeMeasureSpec(measuredHeight, MeasureSpec.EXACTLY);
            mLeftShadowView.measure(widthSpec, heightSpec);
            mRightShadowView.measure(widthSpec, heightSpec);
        }

        /**
         * This should be called whenever the Y-position of an infobar changes.
         */
        void updateShadowPosition() {
            if (!mIsFloating) return;
            float minY = mLayout.getHeight();
            int childCount = mLayout.getChildCount();
            for (int i = 0; i < childCount; i++) {
                View child = mLayout.getChildAt(i);
                if (child != mLeftShadowView && child != mRightShadowView) {
                    minY = Math.min(minY, child.getY());
                }
            }
            mLeftShadowView.setY(minY);
            mRightShadowView.setY(minY);
        }

        private void onIsFloatingChanged() {
            if (mIsFloating) {
                initShadowViews();
                mLayout.setPadding(mShadowWidth, 0, mShadowWidth, 0);
                mLayout.setClipToPadding(false);
                mLayout.addView(mLeftShadowView);
                mLayout.addView(mRightShadowView);
            } else {
                mLayout.setPadding(0, 0, 0, 0);
                mLayout.removeView(mLeftShadowView);
                mLayout.removeView(mRightShadowView);
            }
        }

        @SuppressLint("RtlHardcoded")
        private void initShadowViews() {
            if (mLeftShadowView != null) return;

            mLeftShadowView = new View(mLayout.getContext());
            mLeftShadowView.setBackgroundResource(R.drawable.infobar_shadow_left);
            LayoutParams leftLp = new FrameLayout.LayoutParams(0, 0, Gravity.LEFT);
            leftLp.leftMargin = -mShadowWidth;
            mLeftShadowView.setLayoutParams(leftLp);

            mRightShadowView = new View(mLayout.getContext());
            mRightShadowView.setBackgroundResource(R.drawable.infobar_shadow_left);
            LayoutParams rightLp = new FrameLayout.LayoutParams(0, 0, Gravity.RIGHT);
            rightLp.rightMargin = -mShadowWidth;
            mRightShadowView.setScaleX(-1f);
            mRightShadowView.setLayoutParams(rightLp);
        }
    }

    /**
     * The height of back infobars, i.e. the distance between the top of the front infobar and the
     * top of the next infobar back.
     */
    private final int mBackInfobarHeight;

    /**
     * All the Items, in front to back order.
     * This list is updated immediately when addInfoBar(), removeInfoBar(), and swapInfoBar() are
     * called; so during animations, it does *not* match the currently visible views.
     */
    private final ArrayList<Item> mItems = new ArrayList<>();

    /**
     * The currently visible InfoBarWrappers, in front to back order.
     */
    private final ArrayList<InfoBarWrapper> mInfoBarWrappers = new ArrayList<>();

    /** A observer that is notified when animations finish. */
    private final InfoBarAnimationListener mAnimationListener;

    /** The current animation, or null if no animation is happening currently. */
    private InfoBarAnimation mAnimation;

    private FloatingBehavior mFloatingBehavior;

    /** The runnable to make infobar container fully visible. */
    private Runnable mMakeContainerVisibleRunnable;

    /**
     * Determines whether any animations need to run in order to make the visible views match the
     * current list of Items in mItems. If so, kicks off the next animation that's needed.
     */
    private void processPendingAnimations() {
        // If an animation is running, wait until it finishes before beginning the next animation.
        if (mAnimation != null) return;

        // The steps below are ordered to minimize movement during animations. In particular,
        // removals happen before additions or swaps, and changes are made to back infobars before
        // front infobars.

        // First, remove any infobars that are no longer in mItems, if any. Check the back infobars
        // before the front.
        for (int i = mInfoBarWrappers.size() - 1; i >= 0; i--) {
            Item visibleItem = mInfoBarWrappers.get(i).getItem();
            if (!mItems.contains(visibleItem)) {
                if (i == 0 && mInfoBarWrappers.size() >= 2) {
                    // Remove the front infobar and reveal the second-to-front infobar.
                    runAnimation(new FrontInfoBarDisappearingAndRevealingAnimation());
                    return;

                } else {
                    // Move the infobar to the very back if it's not already there.
                    InfoBarWrapper wrapper = mInfoBarWrappers.get(i);
                    if (i != mInfoBarWrappers.size() - 1) {
                        removeWrapper(wrapper);
                        addWrapper(wrapper);
                    }

                    // Remove the backmost infobar (which may be the front infobar).
                    runAnimation(new InfoBarDisappearingAnimation());
                    return;
                }
            }
        }

        // Second, run swap animation on front infobar if needed.
        if (!mInfoBarWrappers.isEmpty()) {
            Item frontItem = mInfoBarWrappers.get(0).getItem();
            View frontContents = mInfoBarWrappers.get(0).getChildAt(0);
            if (frontContents != frontItem.getView()) {
                runAnimation(new FrontInfoBarSwapContentsAnimation());
                return;
            }
        }

        // Third, check if we should add any infobars in front of visible infobars. This can happen
        // if an infobar has been inserted into mItems, in front of the currently visible item. To
        // detect this the items at the beginning of mItems are compared against the first item in
        // mInfoBarWrappers.
        if (!mInfoBarWrappers.isEmpty()) {
            // Find the infobar with the highest index that isn't currently being shown.
            Item currentVisibleItem = mInfoBarWrappers.get(0).getItem();
            Item itemToInsert = null;
            for (int checkIndex = 0; checkIndex < mItems.size(); checkIndex++) {
                if (mItems.get(checkIndex) == currentVisibleItem) {
                    // There are no remaining infobars that can possibly override the
                    // currently displayed one.
                    break;
                } else {
                    // Found an infobar that isn't being displayed yet.  Track it so that
                    // it can be animated in.
                    itemToInsert = mItems.get(checkIndex);
                }
            }
            if (itemToInsert != null) {
                runAnimation(new FrontInfoBarAppearingAnimation(itemToInsert));
                return;
            }
        }

        // Fourth, check if we should add any infobars at the back.
        int desiredChildCount = Math.min(mItems.size(), MAX_STACK_DEPTH);
        if (mInfoBarWrappers.size() < desiredChildCount) {
            Item itemToShow = mItems.get(mInfoBarWrappers.size());
            runAnimation(mInfoBarWrappers.isEmpty()
                    ? new FirstInfoBarAppearingAnimation(itemToShow)
                    : new BackInfoBarAppearingAnimation(itemToShow));
            return;
        }

        // Fifth, now that we've stabilized, let listeners know that we have no more animations.
        Item frontItem = mInfoBarWrappers.size() > 0 ? mInfoBarWrappers.get(0).getItem() : null;
        mAnimationListener.notifyAllAnimationsFinished(frontItem);
    }

    private void runAnimation(InfoBarAnimation animation) {
        mAnimation = animation;
        mAnimation.prepareAnimation();
        if (isLayoutRequested()) {
            // onLayout() will call mAnimation.start().
        } else {
            mAnimation.start();
        }
    }

    private void addWrapper(InfoBarWrapper wrapper) {
        addView(wrapper, 0, new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));
        mInfoBarWrappers.add(wrapper);
        updateLayoutParams();
    }

    private void addWrapperToFront(InfoBarWrapper wrapper) {
        addView(wrapper, new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));
        mInfoBarWrappers.add(0, wrapper);
        updateLayoutParams();
    }

    private void removeWrapper(InfoBarWrapper wrapper) {
        removeView(wrapper);
        mInfoBarWrappers.remove(wrapper);
        updateLayoutParams();
    }

    private void updateLayoutParams() {
        // Stagger the top margins so the back infobars peek out a bit.
        int childCount = mInfoBarWrappers.size();
        for (int i = 0; i < childCount; i++) {
            View child = mInfoBarWrappers.get(i);
            LayoutParams lp = (LayoutParams) child.getLayoutParams();
            lp.topMargin = (childCount - 1 - i) * mBackInfobarHeight;
            child.setLayoutParams(lp);
        }
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        widthMeasureSpec = mFloatingBehavior.beforeOnMeasure(widthMeasureSpec);
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
        mFloatingBehavior.afterOnMeasure(getMeasuredHeight());
    }

    @Override
    public void announceForAccessibility(CharSequence text) {
        if (TextUtils.isEmpty(text)) return;
        super.announceForAccessibility(text);
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        super.onLayout(changed, left, top, right, bottom);
        mFloatingBehavior.updateShadowPosition();

        // Animations start after a layout has completed, at which point all views are guaranteed
        // to have valid sizes and positions.
        if (mAnimation != null && !mAnimation.isStarted()) {
            mAnimation.start();
        }
    }

    @Override
    public boolean onInterceptTouchEvent(MotionEvent ev) {
        // Trap any attempts to fiddle with the infobars while we're animating.
        return super.onInterceptTouchEvent(ev) || mAnimation != null
                || (!mInfoBarWrappers.isEmpty()
                        && !mInfoBarWrappers.get(0).getItem().areControlsEnabled());
    }

    @Override
    @SuppressLint("ClickableViewAccessibility")
    public boolean onTouchEvent(MotionEvent event) {
        super.onTouchEvent(event);
        // Consume all touch events so they do not reach the ContentView.
        return true;
    }

    @Override
    public boolean onHoverEvent(MotionEvent event) {
        super.onHoverEvent(event);
        // Consume all hover events so they do not reach the ContentView. In touch exploration mode,
        // this prevents the user from interacting with the part of the ContentView behind the
        // infobars. http://crbug.com/430701
        return true;
    }
}
