// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.example.autofill_service.fragments;

import android.content.ActivityNotFoundException;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.provider.Settings;
import android.text.SpannableString;
import android.text.Spanned;
import android.text.method.LinkMovementMethod;
import android.text.style.ClickableSpan;
import android.view.View;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.StringRes;
import androidx.fragment.app.Fragment;

import org.chromium.example.autofill_service.R;

/** Primary fragment of the landing page. It describes the setup of the bundled AutofillService. */
public class InstructionsFragment extends Fragment {
    public InstructionsFragment() {
        super(R.layout.fragment_instructions);
    }

    public void onViewCreated(@NonNull View view, Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);

        view.findViewById(R.id.button_first).setOnClickListener(this::openCommunicationFragment);

        final TextView introTextView = view.findViewById(R.id.textview_first);
        introTextView.setMovementMethod(LinkMovementMethod.getInstance());
        introTextView.setText(
                applyClickActionToString(
                        R.string.fragment_instructions_summary,
                        "Android System settings",
                        this::openSystemSettings),
                TextView.BufferType.SPANNABLE);
    }

    private void openCommunicationFragment(View v) {
        getParentFragmentManager()
                .beginTransaction()
                .replace(R.id.fragment_container_view, BrowserCommunicationFragment.class, null)
                .setReorderingAllowed(true)
                .addToBackStack("name") // Name can be null
                .commit();
    }

    private SpannableString applyClickActionToString(
            @StringRes int stringRes, String subString, Runnable action) {
        final String originalText = getResources().getString(stringRes);
        final SpannableString spannableString = new SpannableString(originalText);
        final int subStringIndex = originalText.indexOf(subString);
        spannableString.setSpan(
                new ClickableSpan() {
                    @Override
                    public void onClick(View v) {
                        action.run();
                    }
                },
                subStringIndex,
                subStringIndex + subString.length(),
                Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
        return spannableString;
    }

    private void openSystemSettings() {
        final String kNonPackageName = "package:not.a.package.so.all.providers.show";
        final Intent intent = new Intent(Settings.ACTION_REQUEST_SET_AUTOFILL_SERVICE);
        // Request an unlikely package to become the provider to ensure the picker shows.
        intent.setData(Uri.parse(kNonPackageName));
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        try {
            getContext().startActivity(intent, null);
        } catch (ActivityNotFoundException e) {
        }
    }
}
