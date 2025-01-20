// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.example.autofill_service.fragments;

import android.os.Bundle;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.fragment.app.Fragment;

import org.chromium.example.autofill_service.R;

/** Primary fragment of the landing page. It describes the setup of the bundled AutofillService. */
public class InstructionsFragment extends Fragment {
    public InstructionsFragment() {
        super(R.layout.fragment_instructions);
    }

    public void onViewCreated(@NonNull View view, Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);

        view.findViewById(R.id.button_first)
                .setOnClickListener(
                        v -> {
                            getParentFragmentManager()
                                    .beginTransaction()
                                    .replace(
                                            R.id.fragment_container_view,
                                            BrowserCommunicationFragment.class,
                                            null)
                                    .setReorderingAllowed(true)
                                    .addToBackStack("name") // Name can be null
                                    .commit();
                        });
    }
}
