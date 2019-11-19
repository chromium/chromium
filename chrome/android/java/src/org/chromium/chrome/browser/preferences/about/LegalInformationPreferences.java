// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.about;

import android.os.Bundle;
import android.support.v7.preference.PreferenceFragmentCompat;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.preferences.PreferenceUtils;

/**
 * Fragment to display legal information about Chrome.
 */
public class LegalInformationPreferences extends PreferenceFragmentCompat {
    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String s) {
        PreferenceUtils.addPreferencesFromResource(this, R.xml.legal_information_preferences);
        getActivity().setTitle(R.string.legal_information_title);
    }
}
