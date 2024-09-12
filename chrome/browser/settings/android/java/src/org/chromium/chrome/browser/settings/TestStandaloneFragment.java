// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import android.os.Bundle;

import androidx.fragment.app.Fragment;

/** A standalone settings fragment. */
public class TestStandaloneFragment extends Fragment {
    public static final String EXTRA_TITLE = "title";

    @Override
    public void onCreate(Bundle savedInstanceState) {
        String title = "standalone";
        Bundle args = getArguments();
        if (args != null) {
            String extraTitle = args.getString(EXTRA_TITLE);
            if (extraTitle != null) {
                title = extraTitle;
            }
        }
        requireActivity().setTitle(title);

        super.onCreate(savedInstanceState);
    }
}
