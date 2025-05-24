// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.example.autofill_service.fragments;

import android.os.Bundle;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.fragment.app.Fragment;

import org.chromium.example.autofill_service.R;

/** Secondary fragment for the main page. It explains how an app can communicate with Chromium. */
public class BrowserCommunicationFragment extends Fragment {
    public BrowserCommunicationFragment() {
        super(R.layout.fragment_browser_communication);
    }

    public void onViewCreated(@NonNull View view, Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);

        view.findViewById(R.id.button_second)
                .setOnClickListener(
                        v -> {
                            getParentFragmentManager()
                                    .beginTransaction()
                                    .replace(
                                            R.id.fragment_container_view,
                                            InstructionsFragment.class,
                                            null)
                                    .setReorderingAllowed(true)
                                    .disallowAddToBackStack()
                                    .commit();
                        });
    }
}
