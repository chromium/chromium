// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox.v4;

import android.os.Bundle;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.privacy_sandbox.PrivacySandboxSettingsBaseFragment;
import org.chromium.chrome.browser.privacy_sandbox.R;
import org.chromium.components.browser_ui.settings.SettingsUtils;

/**
 * Fragment for the Privacy Sandbox -> Fledge preferences.
 */
public class FledgeFragmentV4 extends PrivacySandboxSettingsBaseFragment {
    @Override
    public void onCreatePreferences(@Nullable Bundle bundle, @Nullable String s) {
        super.onCreatePreferences(bundle, s);
        getActivity().setTitle(R.string.settings_fledge_page_title);
        SettingsUtils.addPreferencesFromResource(this, R.xml.fledge_preference_v4);
    }
}
