// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import android.animation.AnimatorSet;
import android.graphics.Bitmap;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.supplier.SyncOneshotSupplier;

/**
 * Interface for providing an {@link HubLayoutAnimator} that may have async dependencies.
 *
 * <p>Implementors of this class should handle the setup of the {@link AnimatorSet} to run an
 * animation. The {@link HubLayoutAnimationRunner} will handle setup of AnimatorListeners, including
 * registering {@link HubLayoutAnimationListeners}, and running of the actual animation. This serves
 * to cleanly divide the API of the {@link AnimatorSet} between setup and observation + execution.
 */
public interface HubLayoutAnimatorProvider {

    /**
     * Returns the type of animation that is planned to be supplied. If {@link #supplyAnimatorNow()}
     * is invoked the supplied animation may be different from this type.
     */
    @HubLayoutAnimationType
    int getPlannedAnimationType();

    /**
     * Returns a {@link SyncObservableSupplier} that will yield a {@link HubLayoutAnimator}.
     *
     * <p>The animator will be started once the {@link HubContainerView} is laid out to the match
     * its containers width and height and will be in {@link View#INVISIBLE} state.
     */
    @NonNull
    SyncOneshotSupplier<HubLayoutAnimator> getAnimatorSupplier();

    /**
     * Requires the {@link SyncOneshotSupplier} returned by {@link getAnimatorSupplier} to set a
     * {@link HubLayoutAnimator} immediately and synchronously.
     *
     * <p>This will only be called if the {@link HubLayoutAnimationRunner} if no value has been
     * supplied and it is needed.
     *
     * <p>Implementers may provide either the intended animation without all its dependencies or
     * provide a fallback animation in its place.
     */
    void supplyAnimatorNow();

    /**
     * Returns a callback that will be called exactly once with a thumbnail for the current tab that
     * can be used for the animation. The callback may receive a null bitmap if no bitmap is
     * available. Returning null instead of a callback indicates to {@link HubLayout} that a
     * thumbnail will not be used for this animation and will allow the {@link HubLayout} to skip
     * the work of transferring the bitmap from native up to Java.
     */
    @Nullable
    default Callback<Bitmap> getThumbnailCallback() {
        return null;
    }
}
