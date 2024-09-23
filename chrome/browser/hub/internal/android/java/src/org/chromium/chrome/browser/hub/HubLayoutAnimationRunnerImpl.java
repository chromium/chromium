// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.supplier.SyncOneshotSupplier;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.hub.HubLayoutAnimationRunner.AnimationState;

import java.util.Collections;
import java.util.LinkedList;

/** Implementation of {@link HubLayoutAnimationRunner}. */
public class HubLayoutAnimationRunnerImpl implements HubLayoutAnimationRunner {
    private final HubLayoutAnimatorProvider mAnimatorProvider;

    private @AnimationState int mAnimationState;
    private @HubLayoutAnimationType int mAnimationType;
    private boolean mWasForcedToFinish;

    private @Nullable LinkedList<HubLayoutAnimationListener> mListeners;

    /**
     * Creates a {@link HubLayoutAnimatorRunnerImpl}.
     *
     * @param animatorProvider The {@link HubLayoutAnimatorProvider} to run the animation of.
     */
    HubLayoutAnimationRunnerImpl(HubLayoutAnimatorProvider animatorProvider) {
        mAnimatorProvider = animatorProvider;
        mAnimationType = animatorProvider.getPlannedAnimationType();
        mAnimationState = AnimationState.INITIALIZING;
    }

    @Override
    public @AnimationState int getAnimationState() {
        return mAnimationState;
    }

    @Override
    public @HubLayoutAnimationType int getAnimationType() {
        return mAnimationType;
    }

    @Override
    public void runWithWaitForAnimatorTimeout(long timeoutMillis) {
        assert mAnimationState == AnimationState.INITIALIZING
                : "Attempting to start an already started runner.";

        mAnimationState = AnimationState.WAITING_FOR_ANIMATOR;
        SyncOneshotSupplier<HubLayoutAnimator> animatorSupplier =
                mAnimatorProvider.getAnimatorSupplier();
        if (animatorSupplier.hasValue()) {
            // Post the callback so we don't run immediately and any other setup work can complete
            // first.
            animatorSupplier.onAvailable(this::postOnAnimatorReady);
            return;
        }
        // Don't post here, we want the animation to start as soon as the animation is ready since
        // it is waiting on async dependencies.
        animatorSupplier.onAvailable(this::onAnimatorReady);
        if (timeoutMillis >= 0) {
            PostTask.postDelayedTask(
                    TaskTraits.UI_DEFAULT, this::onWaitForAnimatorTimeout, timeoutMillis);
        }
    }

    @Override
    public void forceAnimationToFinish() {
        if (mAnimationState == AnimationState.FINISHED) return;

        // If forceAnimationToFinish is called without calling runWithWaitForAnimatorTimeout then
        // the downstream calls onAnimatorReady will fail with an assertion. While this is not a
        // state that is expected to happen, it is recoverable by advancing to the
        // WAITING_FOR_ANIMATOR state.
        // TODO(crbug.com/40285429): Consider changing this to an assert or exception.
        if (mAnimationState == AnimationState.INITIALIZING) {
            mAnimationState = AnimationState.WAITING_FOR_ANIMATOR;
        }

        mWasForcedToFinish = true;
        SyncOneshotSupplier<HubLayoutAnimator> animatorSupplier =
                mAnimatorProvider.getAnimatorSupplier();
        if (animatorSupplier.hasValue()) {
            HubLayoutAnimator animator = animatorSupplier.get();
            if (mAnimationState == AnimationState.STARTED) {
                animator.getAnimatorSet().end();
            } else {
                onAnimatorReady(animator);
            }
            return;
        }

        supplyAnimatorNow();
    }

    @Override
    public void addListener(@NonNull HubLayoutAnimationListener animationListener) {
        assert mAnimationState == AnimationState.INITIALIZING
                : "Attempting to add an HubLayoutAnimationListener that may not be called.";
        ensureListenersList();
        mListeners.add(animationListener);
    }

    private void onWaitForAnimatorTimeout() {
        if (mAnimationState >= AnimationState.STARTED) return;

        supplyAnimatorNow();
    }

    private void supplyAnimatorNow() {
        assert mAnimationState == AnimationState.WAITING_FOR_ANIMATOR;

        mAnimatorProvider.supplyAnimatorNow();

        SyncOneshotSupplier<HubLayoutAnimator> animatorSupplier =
                mAnimatorProvider.getAnimatorSupplier();
        assert animatorSupplier.hasValue()
                : "HubAnimatorProvider#supplyAnimatorNow() failed to provide an animation for "
                        + getAnimationType();

        // Don't rely on the observable supplier here as we might post when the value is set. Call
        // the onAnimatorReady method directly (repeat calls will be dropped).
        onAnimatorReady(animatorSupplier.get());
    }

    private void postOnAnimatorReady(@NonNull HubLayoutAnimator animator) {
        PostTask.postTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    onAnimatorReady(animator);
                });
    }

    private void onAnimatorReady(@NonNull HubLayoutAnimator animator) {
        if (mAnimationState >= AnimationState.STARTED) return;

        assert mAnimationState == AnimationState.WAITING_FOR_ANIMATOR
                : "Starting an animation that was not waiting for an animator.";

        HubLayoutAnimationListener animatorListener = animator.getListener();
        if (animatorListener != null) {
            ensureListenersList();
            mListeners.addFirst(animatorListener);
        }
        mAnimationType = animator.getAnimationType();

        for (HubLayoutAnimationListener listener : getListenersIterable()) {
            listener.beforeStart();
        }
        AnimatorSet animatorSet = animator.getAnimatorSet();
        animatorSet.addListener(
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationStart(Animator animation) {
                        for (HubLayoutAnimationListener listener : getListenersIterable()) {
                            listener.onStart();
                        }
                    }

                    @Override
                    public void onAnimationEnd(Animator animation) {
                        mAnimationState = AnimationState.FINISHED;

                        for (HubLayoutAnimationListener listener : getListenersIterable()) {
                            listener.onEnd(mWasForcedToFinish);
                        }
                        for (HubLayoutAnimationListener listener : getListenersIterable()) {
                            listener.afterEnd();
                        }
                        mListeners = null;
                    }
                });

        animatorSet.start();
        mAnimationState = AnimationState.STARTED;
        if (mWasForcedToFinish) {
            animatorSet.end();
        }
    }

    private Iterable<HubLayoutAnimationListener> getListenersIterable() {
        return mListeners == null ? Collections.emptyList() : mListeners;
    }

    private void ensureListenersList() {
        if (mListeners == null) {
            mListeners = new LinkedList<HubLayoutAnimationListener>();
        }
    }
}
