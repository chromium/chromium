// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.app.Dialog;
import android.os.Bundle;
import android.support.v4.app.DialogFragment;
import android.support.v4.app.Fragment;
import android.support.v4.util.ObjectsCompat;
import android.support.v7.app.AlertDialog;
import android.support.v7.content.res.AppCompatResources;
import android.support.v7.widget.LinearLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import org.chromium.base.Log;
import org.chromium.chrome.R;
import org.chromium.components.signin.AccountManagerDelegateException;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountsChangeObserver;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.List;

/**
 * This class implements dialog-based account picker that is used by SigninFragmentBase. This
 * fragment uses {@link Fragment#getParentFragment()} to report selection results, so parent
 * fragment should:
 * 1. Use {@link Fragment#getChildFragmentManager()} to add this fragment.
 * 2. Implement {@link Callback} interface to get selection result.
 */
public class AccountPickerDialogFragment extends DialogFragment {
    public interface Callback {
        /**
         * Notifies that the user has selected an account.
         * @param accountName The email of the selected account.
         * @param isDefaultAccount Whether the selected account is the first in the account list.
         */
        void onAccountSelected(String accountName, boolean isDefaultAccount);

        /** Notifies that the user has clicked "Add account" button. */
        void addAccount();
    }

    @IntDef({ViewType.EXISTING_ACCOUNT, ViewType.NEW_ACCOUNT})
    @Retention(RetentionPolicy.SOURCE)
    private @interface ViewType {
        int EXISTING_ACCOUNT = 0;
        int NEW_ACCOUNT = 1;
    }

    private class Adapter extends RecyclerView.Adapter<Adapter.ViewHolder> {
        class ViewHolder extends RecyclerView.ViewHolder {
            private final @Nullable ImageView mAccountImage;
            private final @Nullable TextView mAccountTextPrimary;
            private final @Nullable TextView mAccountTextSecondary;
            private final @Nullable ImageView mSelectionMark;

            /** Used for displaying profile data for existing account. */
            ViewHolder(View view, @Nullable ImageView accountImage,
                    @Nullable TextView accountTextPrimary, @Nullable TextView accountTextSecondary,
                    @Nullable ImageView selectionMark) {
                super(view);
                mAccountImage = accountImage;
                mAccountTextPrimary = accountTextPrimary;
                mAccountTextSecondary = accountTextSecondary;
                mSelectionMark = selectionMark;
            }

            /** Used for "Add account" row. */
            ViewHolder(View view) {
                this(view, null, null, null, null);
            }

            void onBind(DisplayableProfileData profileData, boolean isSelected) {
                mAccountImage.setImageDrawable(profileData.getImage());

                String fullName = profileData.getFullName();
                if (!TextUtils.isEmpty(fullName)) {
                    mAccountTextPrimary.setText(fullName);
                    mAccountTextSecondary.setText(profileData.getAccountName());
                    mAccountTextSecondary.setVisibility(View.VISIBLE);
                } else {
                    // Full name is not available, show the email in the primary TextView.
                    mAccountTextPrimary.setText(profileData.getAccountName());
                    mAccountTextSecondary.setVisibility(View.GONE);
                }

                mSelectionMark.setVisibility(isSelected ? View.VISIBLE : View.GONE);
            }
        }

        private String mSelectedAccountName;
        private List<DisplayableProfileData> mProfileDataList;

        Adapter(String selectedAccountName, List<DisplayableProfileData> profileDataList) {
            mSelectedAccountName = selectedAccountName;
            mProfileDataList = profileDataList;
        }

        @Override
        public ViewHolder onCreateViewHolder(ViewGroup viewGroup, @ViewType int viewType) {
            LayoutInflater inflater = LayoutInflater.from(viewGroup.getContext());
            if (viewType == ViewType.NEW_ACCOUNT) {
                TextView view = (TextView) inflater.inflate(
                        R.layout.account_picker_new_account_row, viewGroup, false);
                // Set the vector drawable programmatically because app:drawableStartCompat is only
                // available after AndroidX appcompat library.
                // TODO(https://crbug.com/948367): Use app:drawableStartCompat.
                view.setCompoundDrawablesRelativeWithIntrinsicBounds(
                        AppCompatResources.getDrawable(
                                viewGroup.getContext(), R.drawable.ic_add_circle_40dp),
                        null, null, null);
                return new ViewHolder(view);
            }
            View view = inflater.inflate(R.layout.account_picker_row, viewGroup, false);
            ImageView accountImage = view.findViewById(R.id.account_image);
            TextView accountTextPrimary = view.findViewById(R.id.account_text_primary);
            TextView accountTextSecondary = view.findViewById(R.id.account_text_secondary);
            ImageView selectionMark = view.findViewById(R.id.account_selection_mark);
            return new ViewHolder(
                    view, accountImage, accountTextPrimary, accountTextSecondary, selectionMark);
        }

        @Override
        public void onBindViewHolder(ViewHolder holder, int position) {
            switch (holder.getItemViewType()) {
                case ViewType.EXISTING_ACCOUNT:
                    DisplayableProfileData profileData = mProfileDataList.get(position);
                    boolean isSelected = ObjectsCompat.equals(
                            profileData.getAccountName(), mSelectedAccountName);
                    holder.onBind(profileData, isSelected);

                    final String accountName = profileData.getAccountName();
                    final boolean isDefault = position == 0;
                    holder.itemView.setOnClickListener(
                            view -> onAccountSelected(accountName, isDefault));

                    return;
                case ViewType.NEW_ACCOUNT:
                    // "Add account" row is immutable.
                    holder.itemView.setOnClickListener(view -> addAccount());
                    return;
                default:
                    assert false : "Unexpected view type!";
            }
        }

        @Override
        public int getItemCount() {
            // The last row is "Add account" and doesn't have a corresponding account.
            return mProfileDataList.size() + 1;
        }

        @Override
        public int getItemViewType(int position) {
            int result = position == mProfileDataList.size() ? ViewType.NEW_ACCOUNT
                                                             : ViewType.EXISTING_ACCOUNT;
            return result;
        }

        void setSelectedAccountName(String selectedAccountName) {
            mSelectedAccountName = selectedAccountName;
            notifyDataSetChanged();
        }

        void setProfileDataList(List<DisplayableProfileData> profileDataList) {
            mProfileDataList = profileDataList;
            notifyDataSetChanged();
        }
    }

    private static final String TAG = "AccountPickerDialog";
    private static final String ARGUMENT_SELECTED_ACCOUNT_NAME =
            "AccountPickerDialogFragment.SelectedAccountName";

    private final AccountsChangeObserver mAccountsChangeObserver = this::updateAccounts;
    private final ProfileDataCache.Observer mProfileDataObserver = accountId -> updateProfileData();
    private ProfileDataCache mProfileDataCache;
    private List<String> mAccounts;
    private Adapter mAdapter;

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

    public AccountPickerDialogFragment() {
        // Fragment must have a publicly accessible default constructor
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        assert getCallback() != null : "No callback for AccountPickerDialogFragment";

        mProfileDataCache = new ProfileDataCache(
                getActivity(), getResources().getDimensionPixelSize(R.dimen.user_picture_size));

        String selectedAccountName = getArguments().getString(ARGUMENT_SELECTED_ACCOUNT_NAME);
        // Account list will be updated in onStart()
        mAdapter = new Adapter(selectedAccountName, new ArrayList<>());
    }

    @Override
    public void onStart() {
        super.onStart();
        AccountManagerFacade.get().addObserver(mAccountsChangeObserver);
        mProfileDataCache.addObserver(mProfileDataObserver);
        updateAccounts();
    }

    @Override
    public void onStop() {
        super.onStop();
        mProfileDataCache.removeObserver(mProfileDataObserver);
        AccountManagerFacade.get().removeObserver(mAccountsChangeObserver);
    }

    @Override
    public Dialog onCreateDialog(Bundle savedInstanceState) {
        AlertDialog.Builder builder =
                new AlertDialog.Builder(getActivity(), R.style.Theme_Chromium_AlertDialog);
        LayoutInflater inflater = LayoutInflater.from(builder.getContext());
        RecyclerView recyclerView =
                (RecyclerView) inflater.inflate(R.layout.account_picker_dialog_body, null);
        recyclerView.setAdapter(mAdapter);
        recyclerView.setLayoutManager(new LinearLayoutManager(getActivity()));
        return builder.setTitle(R.string.signin_account_picker_dialog_title)
                .setView(recyclerView)
                .create();
    }

    /**
     * Updates the selected account.
     * @param selectedAccountName The name of the account that should be marked as selected.
     */
    public void updateSelectedAccount(String selectedAccountName) {
        mAdapter.setSelectedAccountName(selectedAccountName);
    }

    private void onAccountSelected(String accountName, boolean isDefaultAccount) {
        if (!isResumed() || isStateSaved()) return;
        getCallback().onAccountSelected(accountName, isDefaultAccount);
        dismissAllowingStateLoss();
    }

    private void addAccount() {
        if (!isResumed() || isStateSaved()) return;
        getCallback().addAccount();
    }

    private void updateAccounts() {
        try {
            mAccounts = AccountManagerFacade.get().getGoogleAccountNames();
        } catch (AccountManagerDelegateException ex) {
            Log.e(TAG, "Can't get account list", ex);
            dismissAllowingStateLoss();
            return;
        }

        mProfileDataCache.update(mAccounts);
        updateProfileData();
    }

    private void updateProfileData() {
        List<DisplayableProfileData> profileDataList = new ArrayList<>();
        for (String accountName : mAccounts) {
            profileDataList.add(mProfileDataCache.getProfileDataOrDefault(accountName));
        }
        mAdapter.setProfileDataList(profileDataList);
    }

    private Callback getCallback() {
        return (Callback) getParentFragment();
    }
}
