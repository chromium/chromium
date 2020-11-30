// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.consent_auditor;

import androidx.annotation.StringRes;

import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.profiles.Profile;

import java.util.List;

/**
 * This class is used to pass consent records from Android Java UI to the C++
 * consent_auditor component.
 */
public final class ConsentAuditorBridge {
    private static ConsentAuditorBridge sInstance;

    /**
     * Records that the user consented to a feature.
     * @param feature The {@link ConsentAuditorFeature} for which to record the consent.
     * @param consentDescription The resource IDs of the text the user read before consenting.
     * @param consentConfirmation The resource ID of the text the user clicked when consenting.
     */
    public void recordConsent(String accountId, @ConsentAuditorFeature int feature,
            List<Integer> consentDescription, @StringRes int consentConfirmation) {
        int[] consentDescriptionArray = new int[consentDescription.size()];
        for (int i = 0; i < consentDescription.size(); ++i) {
            consentDescriptionArray[i] = consentDescription.get(i);
        }
        ConsentAuditorBridgeJni.get().recordConsent(ConsentAuditorBridge.this,
                Profile.getLastUsedRegularProfile(), accountId, feature, consentDescriptionArray,
                consentConfirmation);
    }

    private ConsentAuditorBridge() {}

    /**
     * Returns the singleton bridge object.
     */
    public static ConsentAuditorBridge getInstance() {
        ThreadUtils.assertOnUiThread();
        if (sInstance == null) sInstance = new ConsentAuditorBridge();
        return sInstance;
    }

    @NativeMethods
    interface Natives {
        void recordConsent(ConsentAuditorBridge caller, Profile profile, String accountId,
                int feature, int[] consentDescription, int consentConfirmation);
    }
}
