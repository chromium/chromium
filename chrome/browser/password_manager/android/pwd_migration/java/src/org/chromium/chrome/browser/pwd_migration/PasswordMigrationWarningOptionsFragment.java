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

import org.chromium.components.browser_ui.widget.RadioButtonWithDescription;

/**
 * This fragment contains the UI for the second page of the password migration
 * warning. The page shows alternative options for users who didn't acknowledge
 * the upcoming merge of passwords for different Chrome channels. The offered
 * alternatives are to export the passwords or to start syncing passwords.
 */
public class PasswordMigrationWarningOptionsFragment extends Fragment {
    private Context mContext;
    private Runnable mNextCallback;
    private Runnable mCancelCallback;
    private String mChannelString;

    public PasswordMigrationWarningOptionsFragment(
            Context context, Runnable nextCallback, Runnable cancelCallback, String channelString) {
        super(R.layout.pwd_migration_warning_options);
        mContext = context;
        mNextCallback = nextCallback;
        mCancelCallback = cancelCallback;
        mChannelString = channelString;
    }

    @Override
    public void onViewCreated(View view, @Nullable Bundle savedInstanceState) {
        RadioButtonWithDescription signInOrSyncButton =
                view.findViewById(R.id.radio_sign_in_or_sync);
        RadioButtonWithDescription passwordExportButton =
                view.findViewById(R.id.radio_password_export);
        Button nextButton = view.findViewById(R.id.password_migration_next_button);
        Button cancelButton = view.findViewById(R.id.password_migration_cancel_button);

        signInOrSyncButton.setChecked(true);
        passwordExportButton.setDescriptionText(
                mContext.getString(R.string.password_migration_warning_password_export_subtitle)
                        .replace("%1$s", mChannelString));
        nextButton.setOnClickListener((unusedView) -> mNextCallback.run());
        cancelButton.setOnClickListener((unusedView) -> mCancelCallback.run());
    }
}
