// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_entry_edit;

import android.os.Bundle;

import androidx.preference.PreferenceFragmentCompat;

/**
 * This class is responsible for rendering the edit fragment where users can edit a saved password.
 */
public class CredentialEditFragmentView extends PreferenceFragmentCompat {
    @Override
    public void onCreatePreferences(Bundle bundle, String rootKey) {
        getActivity().setTitle(R.string.password_entry_viewer_edit_stored_password_action_title);
    }
}
