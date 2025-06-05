// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import androidx.annotation.NonNull;

import org.chromium.base.supplier.SyncOneshotSupplier;
import org.chromium.base.supplier.SyncOneshotSupplierImpl;
import org.chromium.build.annotations.NullMarked;

/**
 * An implementation of {@link HubLayoutAnimatorProvider} to wrap an already completed {@link
 * HubLayoutAnimator}. Use this if adding a {@link HubLayoutAnimator} that has no async
 * dependencies.
 */
@NullMarked
public class PresetHubLayoutAnimatorProvider implements HubLayoutAnimatorProvider {
    private final SyncOneshotSupplierImpl<HubLayoutAnimator> mPresetAnimatorSupplier;

    /**
     * Constructor for the {@link PresetHubLayoutAnimatorProvider}.
     *
     * @param animator The {@link HubLayoutAnimator} to use.
     */
    public PresetHubLayoutAnimatorProvider(@NonNull HubLayoutAnimator animator) {
        mPresetAnimatorSupplier = new SyncOneshotSupplierImpl<>();
        mPresetAnimatorSupplier.set(animator);
    }

    @Override
    public @HubLayoutAnimationType int getPlannedAnimationType() {
        HubLayoutAnimator animator = mPresetAnimatorSupplier.get();
        if (animator == null) return HubLayoutAnimationType.NONE;
        return animator.getAnimationType();
    }

    @Override
    public @NonNull SyncOneshotSupplier<HubLayoutAnimator> getAnimatorSupplier() {
        return mPresetAnimatorSupplier;
    }

    @Override
    public void supplyAnimatorNow() {
        assert false : "Not reached.";
    }
}
