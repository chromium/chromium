// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.options;

import android.os.Bundle;

import androidx.preference.PreferenceFragmentCompat;

import org.chromium.chrome.browser.autofill.R;

/**
 * Autofill options fragment, which allows the user to configure autofill.
 */
public class AutofillOptionsFragment extends PreferenceFragmentCompat {
    /**
     * This default constructor is required to instantiate the fragment.
     */
    public AutofillOptionsFragment() {}

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        getActivity().setTitle(R.string.autofill_options_title);
    }
}
