// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.device_dialog;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.browser.omnibox.ChromeAutocompleteSchemeClassifier;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.omnibox.AutocompleteSchemeClassifier;
import org.chromium.components.permissions.BluetoothChooserAndroidDelegate;

/**
 *  The implementation of {@link BluetoothChooserAndroidDelegate} for Chrome.
 */
public class ChromeBluetoothChooserAndroidDelegate implements BluetoothChooserAndroidDelegate {
    /**
     * {@inheritDoc}
     */
    @Override
    public AutocompleteSchemeClassifier createAutocompleteSchemeClassifier() {
        // TODO (https://crbug.com/1048632): Use the current profile (i.e., regular profile or
        // incognito profile) instead of always using regular profile. It works correctly now, but
        // it is not safe.
        return new ChromeAutocompleteSchemeClassifier(Profile.getLastUsedRegularProfile());
    }

    @CalledByNative
    private static ChromeBluetoothChooserAndroidDelegate create() {
        return new ChromeBluetoothChooserAndroidDelegate();
    }
}
