// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.content_creation.notes;

import android.app.Dialog;
import android.os.Bundle;
import android.view.View;

import androidx.appcompat.app.AlertDialog;
import androidx.fragment.app.DialogFragment;

import org.chromium.chrome.browser.content_creation.internal.R;

/**
 * Dialog for the note creation.
 */
public class NoteCreationDialog extends DialogFragment {
    interface NoteDialogObserver {
        void onViewCreated(View view);
    }
    private NoteDialogObserver mNoteDialogObserver;

    public void initDialog(NoteDialogObserver noteDialogObserver) {
        mNoteDialogObserver = noteDialogObserver;
    }

    @Override
    public Dialog onCreateDialog(Bundle savedInstanceState) {
        AlertDialog.Builder builder =
                new AlertDialog.Builder(getActivity(), R.style.Theme_Chromium_Fullscreen);
        View contentView =
                getActivity().getLayoutInflater().inflate(R.layout.creation_dialog, null);
        builder.setView(contentView);

        if (mNoteDialogObserver != null) mNoteDialogObserver.onViewCreated(contentView);

        return builder.create();
    }
}