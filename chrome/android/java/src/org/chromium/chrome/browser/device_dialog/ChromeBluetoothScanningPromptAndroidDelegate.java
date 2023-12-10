// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.device_dialog;

import org.jni_zero.CalledByNative;

import org.chromium.chrome.browser.omnibox.ChromeAutocompleteSchemeClassifier;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.omnibox.AutocompleteSchemeClassifier;
import org.chromium.components.permissions.BluetoothScanningPromptAndroidDelegate;

/** The implementation of {@link BluetoothScanningPromptAndroidDelegate} for Chrome. */
public class ChromeBluetoothScanningPromptAndroidDelegate
        implements BluetoothScanningPromptAndroidDelegate {

    private Profile mProfile;

    @CalledByNative
    ChromeBluetoothScanningPromptAndroidDelegate(Profile profile) {
        mProfile = profile;
    }

    /** {@inheritDoc} */
    @Override
    public AutocompleteSchemeClassifier createAutocompleteSchemeClassifier() {
        return new ChromeAutocompleteSchemeClassifier(mProfile);
    }
}
