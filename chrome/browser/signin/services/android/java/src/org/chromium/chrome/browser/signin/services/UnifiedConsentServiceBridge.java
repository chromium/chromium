// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.services;

import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.profiles.Profile;

/**
 * Bridge to UnifiedConsentService.
 */
public class UnifiedConsentServiceBridge {
    private UnifiedConsentServiceBridge() {}

    /** Returns whether collection of URL-keyed anonymized data is enabled. */
    public static boolean isUrlKeyedAnonymizedDataCollectionEnabled(Profile profile) {
        return UnifiedConsentServiceBridgeJni.get().isUrlKeyedAnonymizedDataCollectionEnabled(
                profile);
    }

    /** Sets whether collection of URL-keyed anonymized data is enabled. */
    public static void setUrlKeyedAnonymizedDataCollectionEnabled(
            Profile profile, boolean enabled) {
        UnifiedConsentServiceBridgeJni.get().setUrlKeyedAnonymizedDataCollectionEnabled(
                profile, enabled);
    }

    /** Returns whether collection of URL-keyed anonymized data is configured by policy. */
    public static boolean isUrlKeyedAnonymizedDataCollectionManaged(Profile profile) {
        return UnifiedConsentServiceBridgeJni.get().isUrlKeyedAnonymizedDataCollectionManaged(
                profile);
    }

    /**
     * Records the sync data types that were turned off during the advanced sync opt-in flow.
     * See C++ unified_consent::metrics::RecordSyncSetupDataTypesHistrogam for details.
     */
    public static void recordSyncSetupDataTypesHistogram(Profile profile) {
        UnifiedConsentServiceBridgeJni.get().recordSyncSetupDataTypesHistogram(profile);
    }

    @NativeMethods
    interface Natives {
        boolean isUrlKeyedAnonymizedDataCollectionEnabled(Profile profile);
        void setUrlKeyedAnonymizedDataCollectionEnabled(Profile profile, boolean enabled);
        boolean isUrlKeyedAnonymizedDataCollectionManaged(Profile profile);
        void recordSyncSetupDataTypesHistogram(Profile profile);
    }
}
