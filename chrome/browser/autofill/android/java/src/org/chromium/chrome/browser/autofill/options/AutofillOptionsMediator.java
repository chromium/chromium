// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.options;

import static org.chromium.chrome.browser.autofill.options.AutofillOptionsProperties.THIRD_PARTY_AUTOFILL_ENABLED;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * The mediator of the autofill options component. Ensures that the model and
 * the pref are in sync (in either direction).
 */
class AutofillOptionsMediator {
    @VisibleForTesting
    static final String HISTOGRAM_USE_THIRD_PARTY_FILLING =
            "Autofill.Settings.ToggleUseThirdPartyFilling";
    private final Profile mProfile;
    private PropertyModel mModel;

    AutofillOptionsMediator(Profile profile) {
        mProfile = profile;
    }

    void initialize(PropertyModel model) {
        mModel = model;
    }

    boolean isInitialized() {
        return mModel != null;
    }

    void updateToggleStateFromPref() {
        assert isInitialized();
        mModel.set(THIRD_PARTY_AUTOFILL_ENABLED,
                prefs().getBoolean(Pref.AUTOFILL_USING_VIRTUAL_VIEW_STRUCTURE));
    }

    void onThirdPartyToggleChanged(boolean optIntoThirdPartyFilling) {
        if (mModel.get(THIRD_PARTY_AUTOFILL_ENABLED) == optIntoThirdPartyFilling) {
            return; // Ignore redundant event.
        }
        mModel.set(THIRD_PARTY_AUTOFILL_ENABLED, optIntoThirdPartyFilling);
        prefs().setBoolean(Pref.AUTOFILL_USING_VIRTUAL_VIEW_STRUCTURE, optIntoThirdPartyFilling);
        RecordHistogram.recordBooleanHistogram(
                HISTOGRAM_USE_THIRD_PARTY_FILLING, optIntoThirdPartyFilling);
    }

    private PrefService prefs() {
        return UserPrefs.get(mProfile);
    }
}
