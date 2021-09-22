// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.content_creation.reactions;

import android.app.Dialog;
import android.os.Bundle;
import android.view.View;

import androidx.appcompat.app.AlertDialog;
import androidx.fragment.app.DialogFragment;

import org.chromium.chrome.browser.content_creation.reactions.internal.R;

/**
 * Dialog for the note creation.
 */
public class LightweightReactionsDialog extends DialogFragment {
    private View mContentView;

    @Override
    public Dialog onCreateDialog(Bundle savedInstanceState) {
        AlertDialog.Builder builder =
                new AlertDialog.Builder(getActivity(), R.style.Theme_Chromium_Fullscreen);
        mContentView = getActivity().getLayoutInflater().inflate(R.layout.reactions_dialog, null);
        builder.setView(mContentView);

        return builder.create();
    }
}