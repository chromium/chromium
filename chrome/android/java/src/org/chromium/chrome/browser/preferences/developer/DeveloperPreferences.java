// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.developer;

import android.os.Bundle;
import android.preference.PreferenceFragment;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.preferences.PreferenceUtils;

/**
 * Settings fragment containing preferences aimed at Chrome and web developers.
 */
public class DeveloperPreferences extends PreferenceFragment {
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        getActivity().setTitle(R.string.prefs_developer);
        PreferenceUtils.addPreferencesFromResource(this, R.xml.developer_preferences);
    }
}
