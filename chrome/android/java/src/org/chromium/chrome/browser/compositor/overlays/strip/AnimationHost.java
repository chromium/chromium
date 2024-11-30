// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import android.animation.Animator;
import android.animation.Animator.AnimatorListener;

import org.chromium.chrome.browser.layouts.animation.CompositorAnimationHandler;
import org.chromium.chrome.browser.tabmodel.TabModel;

import java.util.List;

/** Host for animations in the tab strip. */
interface AnimationHost {
    /**
     * @return The {@link CompositorAnimationHandler} associated with this animation host.
     */
    CompositorAnimationHandler getAnimationHandler();

    /** Finishes any outstanding animations. */
    void finishAnimations();

    /**
     * Finishes any outstanding animations and propagates any related changes to the {@link
     * TabModel}.
     */
    void finishAnimationsAndPushTabUpdates();

    /**
     * Starts a given list of animations.
     *
     * @param animationList The {@link Animator} list to start.
     * @param listener The {@link AnimatorListener} for the given animations.
     */
    void startAnimations(List<Animator> animationList, AnimatorListener listener);
}
