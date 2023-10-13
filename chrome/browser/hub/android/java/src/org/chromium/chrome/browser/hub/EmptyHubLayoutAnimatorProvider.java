// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import android.animation.AnimatorSet;

import androidx.annotation.NonNull;

import org.chromium.base.supplier.SyncOneshotSupplier;
import org.chromium.base.supplier.SyncOneshotSupplierImpl;

/**
 * Temporary empty implementation of {@link HubLayoutAnimatorProvider} to use until all {@link
 * HubLayoutAnimationType}s have concrete implementations.
 */
public class EmptyHubLayoutAnimatorProvider implements HubLayoutAnimatorProvider {
    private final SyncOneshotSupplierImpl<HubLayoutAnimator> mEmptyAnimatorSupplier;

    /**
     * Constructor for the {@link EmptyHubLayoutAnimatorProvider}.
     *
     * @param animationType The {@link HubLayoutAnimationType} to indicate.
     */
    public EmptyHubLayoutAnimatorProvider(@HubLayoutAnimationType int animationType) {
        HubLayoutAnimator animator = new HubLayoutAnimator(animationType, new AnimatorSet(), null);
        mEmptyAnimatorSupplier = new SyncOneshotSupplierImpl<HubLayoutAnimator>();
        mEmptyAnimatorSupplier.set(animator);
    }

    @Override
    public @HubLayoutAnimationType int getPlannedAnimationType() {
        return mEmptyAnimatorSupplier.get().getAnimationType();
    }

    @Override
    public @NonNull SyncOneshotSupplier<HubLayoutAnimator> getAnimatorSupplier() {
        return mEmptyAnimatorSupplier;
    }

    @Override
    public void supplyAnimatorNow() {
        assert false : "Not reached.";
    }
}
