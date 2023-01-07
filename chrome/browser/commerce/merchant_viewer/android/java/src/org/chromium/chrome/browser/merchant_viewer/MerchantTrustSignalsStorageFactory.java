// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.merchant_viewer;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.CallbackController;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.profiles.Profile;

import java.util.HashMap;
import java.util.Map;

/** {@link Profile}-aware factory class for MerchantTrustSignalsStorage. */
class MerchantTrustSignalsStorageFactory {
    @VisibleForTesting
    protected static final Map<Profile, MerchantTrustSignalsEventStorage> sProfileToStorage =
            new HashMap<>();

    private final ObservableSupplier<Profile> mProfileSupplier;
    private final CallbackController mCallbackController;

    MerchantTrustSignalsStorageFactory(ObservableSupplier<Profile> profileSupplier) {
        mProfileSupplier = profileSupplier;
        mCallbackController = new CallbackController();
        mProfileSupplier.addObserver(mCallbackController.makeCancelable(this::onProfileAvailable));
    }

    /**
     * @return {@link MerchantTrustSignalsEventStorage} that maps to the latest value of the
     *         context {@link Profile} supplier.
     */
    MerchantTrustSignalsEventStorage getForLastUsedProfile() {
        Profile profile = mProfileSupplier.get();
        if (profile == null) {
            return null;
        }

        return sProfileToStorage.get(profile);
    }

    /**
     * Destroys all known {@link MerchantTrustSignalsEventStorage} instances for all value of the
     * context {@link Profile} supplier.
     */
    void destroy() {
        sProfileToStorage.clear();
    }

    private void onProfileAvailable(Profile profile) {
        if (profile == null || profile.isOffTheRecord() || sProfileToStorage.get(profile) != null) {
            return;
        }

        sProfileToStorage.put(profile, new MerchantTrustSignalsEventStorage(profile));
    }
}