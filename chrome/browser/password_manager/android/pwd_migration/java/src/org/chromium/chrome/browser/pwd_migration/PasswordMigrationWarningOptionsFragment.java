// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.pwd_migration;

import android.content.Context;
import android.os.Bundle;
import android.view.View;
import android.widget.Button;

import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.pwd_migration.PasswordMigrationWarningProperties.MigrationOption;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescription;

/**
 * This fragment contains the UI for the second page of the password migration
 * warning. The page shows alternative options for users who didn't acknowledge
 * the upcoming merge of passwords for different Chrome channels. The offered
 * alternatives are to export the passwords or to start syncing passwords.
 */
public class PasswordMigrationWarningOptionsFragment extends Fragment {
    private Context mContext;
    private Callback<Integer> mNextCallback;
    private Runnable mCancelCallback;
    private String mChannelString;
    private RadioButtonWithDescription mSignInOrSyncButton;
    private RadioButtonWithDescription mPasswordExportButton;
    private String mAccountDisplayName;

    public PasswordMigrationWarningOptionsFragment(Context context, Callback<Integer> nextCallback,
            Runnable cancelCallback, String channelString, String accountDisplayName) {
        super(R.layout.pwd_migration_warning_options);
        mContext = context;
        mNextCallback = nextCallback;
        mCancelCallback = cancelCallback;
        mChannelString = channelString;
        mAccountDisplayName = accountDisplayName;
    }

    @Override
    public void onViewCreated(View view, @Nullable Bundle savedInstanceState) {
        mSignInOrSyncButton = view.findViewById(R.id.radio_sign_in_or_sync);
        mPasswordExportButton = view.findViewById(R.id.radio_password_export);
        Button nextButton = view.findViewById(R.id.password_migration_next_button);
        Button cancelButton = view.findViewById(R.id.password_migration_cancel_button);

        mSignInOrSyncButton.setChecked(true);
        if (mAccountDisplayName != null) {
            mSignInOrSyncButton.setDescriptionText(mAccountDisplayName);
        }

        mPasswordExportButton.setDescriptionText(
                mContext.getString(R.string.password_migration_warning_password_export_subtitle)
                        .replace("%1$s", mChannelString));
        nextButton.setOnClickListener((unusedView) -> handleOptionSelected());
        cancelButton.setOnClickListener((unusedView) -> mCancelCallback.run());
    }

    private void handleOptionSelected() {
        if (mSignInOrSyncButton.isChecked()) {
            mNextCallback.onResult(MigrationOption.SYNC_PASSWORDS);
        } else if (mPasswordExportButton.isChecked()) {
            mNextCallback.onResult(MigrationOption.EXPORT_AND_DELETE);
        }
    }
}
