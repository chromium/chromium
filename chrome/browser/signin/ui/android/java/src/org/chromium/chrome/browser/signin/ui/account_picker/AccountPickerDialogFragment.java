// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.ui.account_picker;

import android.app.Dialog;
import android.os.Bundle;
import android.view.LayoutInflater;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AlertDialog;
import androidx.fragment.app.DialogFragment;
import androidx.fragment.app.Fragment;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.chrome.browser.signin.ui.R;

/**
 * This class implements dialog-based account picker that is used by SigninFragmentBase. This
 * fragment uses {@link Fragment#getTargetFragment()} to report selection results, so parent
 * fragment should:
 * 1. Use {@link Fragment#setTargetFragment(Fragment, int)} to add this fragment.
 * 2. Implement {@link AccountPickerCoordinator.Listener} interface to get selection result.
 */
public class AccountPickerDialogFragment extends DialogFragment {
    private static final String ARGUMENT_SELECTED_ACCOUNT_NAME =
            "AccountPickerDialogFragment.SelectedAccountName";
    private AccountPickerCoordinator mCoordinator;

    /**
     * Creates an instance and sets its arguments.
     * @param selectedAccountName The name of the account that should be marked as selected.
     */
    public static AccountPickerDialogFragment create(@Nullable String selectedAccountName) {
        AccountPickerDialogFragment result = new AccountPickerDialogFragment();
        Bundle args = new Bundle();
        args.putString(ARGUMENT_SELECTED_ACCOUNT_NAME, selectedAccountName);
        result.setArguments(args);
        return result;
    }

    @Override
    @NonNull
    public Dialog onCreateDialog(Bundle savedInstanceState) {
        AlertDialog.Builder builder =
                new AlertDialog.Builder(requireActivity(), R.style.Theme_Chromium_AlertDialog);
        LayoutInflater inflater = LayoutInflater.from(builder.getContext());
        RecyclerView recyclerView =
                (RecyclerView) inflater.inflate(R.layout.account_picker_dialog_body, null);
        recyclerView.setLayoutManager(new LinearLayoutManager(getActivity()));
        mCoordinator = new AccountPickerCoordinator(recyclerView,
                (AccountPickerCoordinator.Listener) getTargetFragment(),
                getArguments().getString(ARGUMENT_SELECTED_ACCOUNT_NAME),
                /* showIncognitoRow= */ false);
        return builder.setTitle(R.string.signin_account_picker_dialog_title)
                .setView(recyclerView)
                .create();
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        mCoordinator.destroy();
    }

    /**
     * Updates the selected account.
     * @param selectedAccountName The name of the account that should be marked as selected.
     */
    public void updateSelectedAccount(String selectedAccountName) {
        mCoordinator.setSelectedAccountName(selectedAccountName);
    }
}
