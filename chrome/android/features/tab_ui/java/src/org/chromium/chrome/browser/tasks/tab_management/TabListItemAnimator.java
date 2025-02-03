// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_TYPE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.ModelType.TAB;
import static org.chromium.chrome.browser.tasks.tab_management.TabProperties.USE_SHRINK_CLOSE_ANIMATION;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.util.Pair;
import android.view.View;
import android.view.animation.Interpolator;

import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.RecyclerView.ViewHolder;
import androidx.recyclerview.widget.SimpleItemAnimator;

import org.chromium.ui.interpolators.Interpolators;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

import java.util.ArrayList;
import java.util.HashMap;

/**
 * The {@link ItemAnimator} for the {@link TabListRecyclerView}. Extending a {@code
 * DefaultItemAnimator} results in conflicting animations animation timings. As such this code
 * instead extends {@link SimpleItemAnimator} and copies much of the implementation of {@code
 * DefaultItemAnimator}. See
 * https://cs.android.com/android/platform/superproject/main/+/main:frameworks/base/core/java/com/android/internal/widget/DefaultItemAnimator.java.
 */
public class TabListItemAnimator extends SimpleItemAnimator {
    private static final float ORIGINAL_SCALE = 1.0f;
    private static final float REMOVE_PART_1_FINAL_SCALE = 0.6f;
    private static final float REMOVE_PART_2_FINAL_SCALE = 0f;

    public static final long DEFAULT_REMOVE_DURATION = 200;

    /** Holds a set of pending and running animations of a type. */
    private static class AnimatorHolder {
        private final String mAnimationType;
        private final HashMap<ViewHolder, Animator> mPendingAnimators = new HashMap<>();
        private final HashMap<ViewHolder, Animator> mRunningAnimators = new HashMap<>();

        AnimatorHolder(String animationType) {
            mAnimationType = animationType;
        }

        /** Adds a pending animator for a view holder. */
        void put(ViewHolder holder, Animator animator) {
            assert !mPendingAnimators.containsKey(holder) && !mRunningAnimators.containsKey(holder)
                    : mAnimationType + " animation already queued for " + holder;
            mPendingAnimators.put(holder, animator);
        }

        /**
         * Removes an animator for a running view holder. This should only happen when the animation
         * is finished.
         */
        void remove(ViewHolder holder) {
            assert !mPendingAnimators.containsKey(holder)
                    : mAnimationType + " animation still pending for " + holder;
            mRunningAnimators.remove(holder);
        }

        /** Checks if the pending set of animations is empty. */
        boolean isPendingEmpty() {
            return mPendingAnimators.isEmpty();
        }

        /** Checks if there are any pending or running animations. */
        boolean isRunning() {
            return !mPendingAnimators.isEmpty() || !mRunningAnimators.isEmpty();
        }

        /** Runs all the pending animations immediately. */
        void runAllPendingAnimations() {
            runAllPendingAnimationsDelayed(0);
        }

        /** Runs all pending animations after a delay. */
        void runAllPendingAnimationsDelayed(long delay) {
            for (var entry : mPendingAnimators.entrySet()) {
                Animator animator = entry.getValue();
                animator.setStartDelay(delay);
                mRunningAnimators.put(entry.getKey(), animator);
                animator.start();
            }
            mPendingAnimators.clear();
        }

        /**
         * Ends the animation for the view holder. The {@link Animator} is guaranteed to get start()
         * and end() calls at some point.
         */
        void endAnimation(ViewHolder holder) {
            Animator animator = mPendingAnimators.get(holder);
            mPendingAnimators.remove(holder);
            if (animator != null) {
                mRunningAnimators.put(holder, animator);
                animator.start();
                // This call should remove the animator for mRunningAnimators.
                animator.end();
            }
            animator = mRunningAnimators.get(holder);
            if (animator != null) {
                // This call should remove the animator for mRunningAnimators.
                animator.end();
            }
            assert !mRunningAnimators.containsKey(holder)
                    : mAnimationType + " failed to animation for " + holder;
        }

        /**
         * Ends all animations. The {@link Animator}s are guanteed to get start() and end() calls at
         * some point.
         */
        void endAnimations() {
            // Start all pending animations.
            runAllPendingAnimations();
            // Make a copy so that the animators can be removed from mRunningAnimators via their
            // end() calls.
            ArrayList<Animator> animators = new ArrayList<>();
            animators.addAll(mRunningAnimators.values());
            for (var animator : animators) {
                animator.end();
            }
            assert !isRunning() : mAnimationType + " failed to end all animations.";
        }
    }

    private AnimatorHolder mAdds = new AnimatorHolder("Add");
    private AnimatorHolder mChanges = new AnimatorHolder("Change");
    private AnimatorHolder mMoves = new AnimatorHolder("Move");
    private AnimatorHolder mRemovals = new AnimatorHolder("Removal");

    TabListItemAnimator() {
        setRemoveDuration(DEFAULT_REMOVE_DURATION);
    }

    @Override
    public void runPendingAnimations() {
        boolean hasRemovals = !mRemovals.isPendingEmpty();
        boolean hasMoves = !mMoves.isPendingEmpty();
        boolean hasChanges = !mChanges.isPendingEmpty();
        boolean hasAdds = !mAdds.isPendingEmpty();
        if (!hasRemovals && !hasMoves && !hasChanges && !hasAdds) {
            return;
        }

        // Run animations in the priority
        // - P1: Remove P1
        // - P2: Move and Change
        // - P3: Add
        // Animations are run immediately unless an animation of a higher priority was scheduled. If
        // that happens the animation will wait for all higher priority animations to finish before
        // beginning.
        mRemovals.runAllPendingAnimations();
        if (hasRemovals) {
            mMoves.runAllPendingAnimationsDelayed(getRemoveDuration());
        } else {
            mMoves.runAllPendingAnimations();
        }
        if (hasRemovals) {
            mChanges.runAllPendingAnimationsDelayed(getRemoveDuration());
        } else {
            mChanges.runAllPendingAnimations();
        }
        if (hasRemovals || hasMoves || hasChanges) {
            long delay = hasRemovals ? getRemoveDuration() : 0;
            long moveDuration = hasMoves ? getMoveDuration() : 0;
            long changeDuration = hasMoves ? getChangeDuration() : 0;
            delay += Math.max(moveDuration, changeDuration);
            mAdds.runAllPendingAnimationsDelayed(delay);
        } else {
            mAdds.runAllPendingAnimations();
        }
    }

    @Override
    public void endAnimation(ViewHolder holder) {
        // The order animations are ended is irrelevant, but mirror the start order.
        mRemovals.endAnimation(holder);
        mMoves.endAnimation(holder);
        mChanges.endAnimation(holder);
        mAdds.endAnimation(holder);
    }

    @Override
    public void endAnimations() {
        // The order animations are ended is irrelevant, but mirror the start order.
        mRemovals.endAnimations();
        mMoves.endAnimations();
        mChanges.endAnimations();
        mAdds.endAnimations();
    }

    @Override
    public boolean isRunning() {
        return mRemovals.isRunning()
                || mMoves.isRunning()
                || mChanges.isRunning()
                || mAdds.isRunning();
    }

    @Override
    public boolean animateAdd(ViewHolder holder) {
        endAnimation(holder);
        Animator animator = buildAddAnimator(holder);
        mAdds.put(holder, animator);
        return true;
    }

    private Animator buildAddAnimator(ViewHolder holder) {
        // A simple fade in animation.
        View view = holder.itemView;
        view.setAlpha(0f);
        ObjectAnimator alphaAnimator = ObjectAnimator.ofFloat(view, View.ALPHA, 1f);
        alphaAnimator.setDuration(getAddDuration());
        alphaAnimator.setInterpolator(Interpolators.LINEAR_INTERPOLATOR);
        alphaAnimator.addListener(
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationStart(Animator animator) {
                        dispatchAddStarting(holder);
                    }

                    @Override
                    public void onAnimationEnd(Animator animator) {
                        view.setAlpha(1f);
                        dispatchAddFinished(holder);
                        mAdds.remove(holder);
                        dispatchFinishedWhenAllAnimationsDone();
                    }
                });
        return alphaAnimator;
    }

    @Override
    public boolean animateChange(
            ViewHolder oldHolder, ViewHolder newHolder, int fromX, int fromY, int toX, int toY) {
        // This is adapted from DefaultItemAnimator.
        if (oldHolder == newHolder) {
            return animateMove(oldHolder, fromX, fromY, toX, toY);
        }
        View oldView = oldHolder.itemView;
        float previousTranslationX = oldView.getTranslationX();
        float previousTranslationY = oldView.getTranslationY();
        float previousAlpha = oldView.getAlpha();
        endAnimation(oldHolder);
        if (newHolder != null) {
            endAnimation(oldHolder);
        }

        Pair<Animator, Animator> animators =
                buildChangeAnimators(
                        oldHolder,
                        newHolder,
                        previousTranslationX,
                        previousTranslationY,
                        previousAlpha,
                        fromX,
                        fromY,
                        toX,
                        toY);
        if (animators.first != null) {
            mChanges.put(oldHolder, animators.first);
        }
        if (animators.second != null) {
            mChanges.put(newHolder, animators.second);
        }
        return true;
    }

    private Pair<Animator, Animator> buildChangeAnimators(
            ViewHolder oldHolder,
            ViewHolder newHolder,
            float previousTranslationX,
            float previousTranslationY,
            float previousAlpha,
            int fromX,
            int fromY,
            int toX,
            int toY) {
        // This is adapted from DefaultItemAnimator. The animation offsets the old view by a delta
        // and fades it out while the new view moves to its default position from the old view's
        // position while fading in.
        View oldView = oldHolder.itemView;
        oldView.setTranslationX(previousTranslationX);
        oldView.setTranslationY(previousTranslationY);
        oldView.setAlpha(previousAlpha);

        AnimatorSet oldAnimator = new AnimatorSet();
        ObjectAnimator oldTranslationX =
                ObjectAnimator.ofFloat(oldView, View.TRANSLATION_X, toX - fromX);
        oldTranslationX.setInterpolator(getRearrangeInterpolator());
        ObjectAnimator oldTranslationY =
                ObjectAnimator.ofFloat(oldView, View.TRANSLATION_Y, toY - fromY);
        oldTranslationY.setInterpolator(getRearrangeInterpolator());
        ObjectAnimator oldAlpha = ObjectAnimator.ofFloat(oldView, View.ALPHA, 0f);
        oldAnimator.play(oldTranslationX).with(oldTranslationY).with(oldAlpha);
        oldAnimator.setDuration(getChangeDuration());
        oldAnimator.setInterpolator(Interpolators.LINEAR_INTERPOLATOR);
        oldAnimator.addListener(
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationStart(Animator animator) {
                        dispatchChangeStarting(oldHolder, true);
                    }

                    @Override
                    public void onAnimationEnd(Animator animator) {
                        oldView.setAlpha(1);
                        oldView.setTranslationX(0);
                        oldView.setTranslationY(0);
                        dispatchChangeFinished(oldHolder, true);
                        mChanges.remove(oldHolder);
                        dispatchFinishedWhenAllAnimationsDone();
                    }
                });

        AnimatorSet newAnimator = null;
        if (newHolder != null) {
            View newView = newHolder.itemView;
            int deltaX = (int) (toX - fromX - previousTranslationX);
            int deltaY = (int) (toY - fromY - previousTranslationY);
            newView.setTranslationX(-deltaX);
            newView.setTranslationY(-deltaY);
            newView.setAlpha(previousAlpha);

            newAnimator = new AnimatorSet();
            ObjectAnimator newTranslationX = ObjectAnimator.ofFloat(newView, View.TRANSLATION_X, 0);
            newTranslationX.setInterpolator(getRearrangeInterpolator());
            ObjectAnimator newTranslationY = ObjectAnimator.ofFloat(newView, View.TRANSLATION_Y, 0);
            newTranslationY.setInterpolator(getRearrangeInterpolator());
            ObjectAnimator newAlpha = ObjectAnimator.ofFloat(newView, View.ALPHA, 1f);
            newAnimator.play(newTranslationX).with(newTranslationY).with(newAlpha);
            newAnimator.setDuration(getChangeDuration());
            newAnimator.setInterpolator(Interpolators.LINEAR_INTERPOLATOR);
            newAnimator.addListener(
                    new AnimatorListenerAdapter() {
                        @Override
                        public void onAnimationStart(Animator animator) {
                            dispatchChangeStarting(newHolder, false);
                        }

                        @Override
                        public void onAnimationEnd(Animator animator) {
                            newView.setAlpha(1);
                            newView.setTranslationX(0);
                            newView.setTranslationY(0);
                            dispatchChangeFinished(newHolder, false);
                            mChanges.remove(newHolder);
                            dispatchFinishedWhenAllAnimationsDone();
                        }
                    });
        }
        return Pair.create(oldAnimator, newAnimator);
    }

    @Override
    public boolean animateMove(ViewHolder holder, int fromX, int fromY, int toX, int toY) {
        // This is adapted from DefaultItemAnimator.
        View view = holder.itemView;
        fromX += Math.round(view.getTranslationX());
        fromY += Math.round(view.getTranslationY());
        endAnimation(holder);
        int deltaX = toX - fromX;
        int deltaY = toY - fromY;
        if (deltaX == 0 && deltaY == 0) {
            dispatchMoveFinished(holder);
            return false;
        }
        Animator animator = buildMoveAnimator(holder, deltaX, deltaY);
        mMoves.put(holder, animator);
        return true;
    }

    private Animator buildMoveAnimator(ViewHolder holder, int deltaX, int deltaY) {
        // This is adapted from DefaultItemAnimator. The view is moved from its previous location to
        // its new location by translating from an offset at its old position to its new origin.
        View view = holder.itemView;
        view.setTranslationX(-deltaX);
        view.setTranslationY(-deltaY);
        AnimatorSet animator = new AnimatorSet();
        ObjectAnimator translateX = ObjectAnimator.ofFloat(view, View.TRANSLATION_X, 0);
        ObjectAnimator translateY = ObjectAnimator.ofFloat(view, View.TRANSLATION_Y, 0);
        animator.play(translateX).with(translateY);
        animator.setDuration(getMoveDuration());
        animator.setInterpolator(getRearrangeInterpolator());
        animator.addListener(
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationStart(Animator animator) {
                        dispatchMoveStarting(holder);
                    }

                    @Override
                    public void onAnimationEnd(Animator animator) {
                        view.setTranslationX(0);
                        view.setTranslationY(0);
                        dispatchMoveFinished(holder);
                        mMoves.remove(holder);
                        dispatchFinishedWhenAllAnimationsDone();
                    }
                });
        return animator;
    }

    @Override
    public boolean animateRemove(ViewHolder holder) {
        endAnimation(holder);

        // If the view is already effectively removed don't run an animation.
        View view = holder.itemView;
        if (view.getAlpha() == 0 || view.getVisibility() != View.VISIBLE) {
            view.setAlpha(1f);
            dispatchRemoveFinished(holder);
            return false;
        }

        Animator animator = null;
        if (TabUiFeatureUtilities.shouldUseListMode() || !shouldUseShrinkCloseAnimation(holder)) {
            animator = buildGenericRemoveAnimator(holder);
        } else {
            animator = buildTabRemoveAnimator(holder);
        }
        mRemovals.put(holder, animator);
        return true;
    }

    private static boolean shouldUseShrinkCloseAnimation(ViewHolder holder) {
        if (holder instanceof SimpleRecyclerViewAdapter.ViewHolder adapterHolder) {
            var model = adapterHolder.model;
            if (model.get(CARD_TYPE) == TAB) {
                return model.get(USE_SHRINK_CLOSE_ANIMATION);
            }
        }
        return false;
    }

    private Animator buildGenericRemoveAnimator(ViewHolder holder) {
        // This is adapted from DefaultItemAnimator.
        View view = holder.itemView;
        ObjectAnimator alphaAnimator = ObjectAnimator.ofFloat(view, View.ALPHA, 0f);
        alphaAnimator.setDuration(getRemoveDuration());
        alphaAnimator.setInterpolator(getGenericRemoveInterpolator());
        alphaAnimator.addListener(
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationStart(Animator animator) {
                        dispatchRemoveStarting(holder);
                    }

                    @Override
                    public void onAnimationEnd(Animator animator) {
                        view.setAlpha(1f);
                        dispatchRemoveFinished(holder);
                        mRemovals.remove(holder);
                        dispatchFinishedWhenAllAnimationsDone();
                    }
                });
        return alphaAnimator;
    }

    private Animator buildTabRemoveAnimator(ViewHolder holder) {
        // This is a new custom remove animation that happens in two parts.
        // Part 1 shrinks from 100% -> 60%.
        // Part 2 shrinks from 60% -> 0% while fading to 0 alpha.
        long partDuration = getRemoveDuration() / 2;
        View view = holder.itemView;
        AnimatorSet part1Shrink = new AnimatorSet();
        ObjectAnimator part1ScaleX =
                ObjectAnimator.ofFloat(view, View.SCALE_X, REMOVE_PART_1_FINAL_SCALE);
        ObjectAnimator part1ScaleY =
                ObjectAnimator.ofFloat(view, View.SCALE_Y, REMOVE_PART_1_FINAL_SCALE);
        part1Shrink.play(part1ScaleX).with(part1ScaleY);
        part1Shrink.setDuration(partDuration);
        part1Shrink.setInterpolator(Interpolators.LINEAR_INTERPOLATOR);

        AnimatorSet part2ShrinkAndFade = new AnimatorSet();
        ObjectAnimator part2ScaleX =
                ObjectAnimator.ofFloat(view, View.SCALE_X, REMOVE_PART_2_FINAL_SCALE);
        ObjectAnimator part2ScaleY =
                ObjectAnimator.ofFloat(view, View.SCALE_Y, REMOVE_PART_2_FINAL_SCALE);
        part2ShrinkAndFade.play(part2ScaleX).with(part2ScaleY);
        part2ShrinkAndFade.setDuration(partDuration);
        part2ShrinkAndFade.setInterpolator(Interpolators.LINEAR_OUT_SLOW_IN_INTERPOLATOR);

        AnimatorSet animator = new AnimatorSet();
        animator.addListener(
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationStart(Animator animator) {
                        dispatchRemoveStarting(holder);
                    }

                    @Override
                    public void onAnimationEnd(Animator animator) {
                        view.setScaleX(ORIGINAL_SCALE);
                        view.setScaleY(ORIGINAL_SCALE);
                        view.setAlpha(1f);
                        dispatchRemoveFinished(holder);
                        mRemovals.remove(holder);
                        dispatchFinishedWhenAllAnimationsDone();
                    }
                });
        animator.play(part1Shrink).before(part2ShrinkAndFade);
        return animator;
    }

    @VisibleForTesting
    void dispatchFinishedWhenAllAnimationsDone() {
        if (!isRunning()) {
            dispatchAnimationsFinished();
        }
    }

    private Interpolator getRearrangeInterpolator() {
        return Interpolators.STANDARD_INTERPOLATOR;
    }

    private Interpolator getGenericRemoveInterpolator() {
        return Interpolators.STANDARD_ACCELERATE;
    }
}
