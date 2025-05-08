// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.merchant_viewer;

import static org.chromium.build.NullUtil.assumeNonNull;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.MonotonicNonNull;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileKeyedMap;

/** {@link Profile}-aware factory class for MerchantTrustSignalsStorage. */
@NullMarked
class MerchantTrustSignalsStorageFactory {
    @VisibleForTesting
    protected static @MonotonicNonNull ProfileKeyedMap<MerchantTrustSignalsEventStorage>
            sProfileToStorage;

    private final ObservableSupplier<Profile> mProfileSupplier;

    MerchantTrustSignalsStorageFactory(ObservableSupplier<Profile> profileSupplier) {
        if (sProfileToStorage == null) {
            // TODO(crbug.com/40259781): MerchantTrustSignalsEventStorage has a native counterpart
            // that is
            //     never destroyed. So, this will leak native objects anytime a profile is
            //     destroyed, which is infrequent given the single profile app behavior. To fix
            //     this, add a cleanup/destroy method to MerchantTrustSignalsEventStorage and
            //     switch to the ProfileKeyedMap variant that handles proper cleanup.
            sProfileToStorage = new ProfileKeyedMap<>(ProfileKeyedMap.NO_REQUIRED_CLEANUP_ACTION);
        }
        mProfileSupplier = profileSupplier;
    }

    /**
     * @return {@link MerchantTrustSignalsEventStorage} that maps to the latest value of the context
     *     {@link Profile} supplier.
     */
    @Nullable MerchantTrustSignalsEventStorage getForLastUsedProfile() {
        assumeNonNull(sProfileToStorage);
        Profile profile = mProfileSupplier.get();
        if (profile == null || profile.isOffTheRecord()) {
            return null;
        }

        return sProfileToStorage.getForProfile(profile, MerchantTrustSignalsEventStorage::new);
    }

    /**
     * Destroys all known {@link MerchantTrustSignalsEventStorage} instances for all value of the
     * context {@link Profile} supplier.
     */
    void destroy() {
        assumeNonNull(sProfileToStorage).destroy();
    }
}
