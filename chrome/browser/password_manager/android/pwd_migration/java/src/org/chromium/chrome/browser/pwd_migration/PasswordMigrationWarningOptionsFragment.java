// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.pwd_migration;

import static org.chromium.chrome.browser.password_manager.PasswordMetricsUtil.HistogramExportResult.ACTIVITY_DESTROYED;
import static org.chromium.chrome.browser.pwd_migration.PasswordMigrationWarningCoordinator.EXPORT_METRICS_ID;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.view.View;
import android.widget.Button;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentManager;

import org.chromium.chrome.browser.password_manager.PasswordMetricsUtil;
import org.chromium.chrome.browser.pwd_migration.PasswordMigrationWarningProperties.MigrationOption;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescription;

/**
 * This fragment contains the UI for the second page of the password migration
 * warning. The page shows alternative options for users who didn't acknowledge
 * the upcoming merge of passwords for different Chrome channels. The offered
 * alternatives are to export the passwords or to start syncing passwords.
 */
public class PasswordMigrationWarningOptionsFragment extends Fragment {
    private static final int PASSWORD_EXPORT_INTENT_REQUEST_CODE = 3485764;

    private static final String PASSWORD_EXPORT_TEXT = "PASSWORD_EXPORT_TEXT";
    private String mPaswordExportText;
    private PasswordMigrationWarningOnClickHandler mOnClickHandler;
    private Runnable mCancelCallback;
    private RadioButtonWithDescription mSignInOrSyncButton;
    private RadioButtonWithDescription mPasswordExportButton;
    private String mAccountDisplayName;
    private FragmentManager mFragmentManager;
    private Runnable mOnResumeExportFlowCallback;
    private boolean mShouldOfferSync;

    public PasswordMigrationWarningOptionsFragment(
            String paswordExportText,
            boolean shouldOfferSync,
            PasswordMigrationWarningOnClickHandler onClickHandler,
            Runnable cancelCallback,
            String accountDisplayName,
            FragmentManager fragmentManager,
            Runnable onResumeExportFlowCallback) {
        super(R.layout.pwd_migration_warning_options);
        mPaswordExportText = paswordExportText;
        mShouldOfferSync = shouldOfferSync;
        mOnClickHandler = onClickHandler;
        mCancelCallback = cancelCallback;
        mAccountDisplayName = accountDisplayName;
        mFragmentManager = fragmentManager;
        mOnResumeExportFlowCallback = onResumeExportFlowCallback;
    }

    public PasswordMigrationWarningOptionsFragment() {}

    @Override
    public void onViewCreated(View view, @Nullable Bundle savedInstanceState) {
        mSignInOrSyncButton = view.findViewById(R.id.radio_sign_in_or_sync);
        mPasswordExportButton = view.findViewById(R.id.radio_password_export);
        Button nextButton = view.findViewById(R.id.password_migration_next_button);
        Button cancelButton = view.findViewById(R.id.password_migration_cancel_button);

        if (mShouldOfferSync) {
            if (mAccountDisplayName != null) {
                mSignInOrSyncButton.setDescriptionText(mAccountDisplayName);
            }
            mSignInOrSyncButton.setChecked(true);
        } else {
            mSignInOrSyncButton.setVisibility(View.GONE);
            mPasswordExportButton.setChecked(true);
        }

        mPasswordExportButton.setDescriptionText(mPaswordExportText);
        nextButton.setOnClickListener((unusedView) -> handleOptionSelected());
        cancelButton.setOnClickListener((unusedView) -> mCancelCallback.run());
    }

    private void handleOptionSelected() {
        if (mSignInOrSyncButton.isChecked()) {
            mOnClickHandler.onNext(MigrationOption.SYNC_PASSWORDS, mFragmentManager);
        } else if (mPasswordExportButton.isChecked()) {
            mOnClickHandler.onNext(MigrationOption.EXPORT_AND_DELETE, mFragmentManager);
        }
    }

    @Override
    public void onResume() {
        super.onResume();
        if (mOnResumeExportFlowCallback == null) {
            // It can happen that the Activity which started the export flow was killed by Android
            // while the export flow Activity was on screen, due to the low memory on the device
            // running Chrome. If the export flow was started from the password migration warning
            // sheet, the sheet will be destroyed together with the Activity. The Fragment that
            // started the sheet will be recovered, but it won't contain the callback to resume the
            // export flow.
            PasswordMetricsUtil.logPasswordsExportResult(EXPORT_METRICS_ID, ACTIVITY_DESTROYED);
            return;
        }

        mOnResumeExportFlowCallback.run();
    }

    void runCreateFileOnDiskIntent(Intent intent) {
        startActivityForResult(intent, PASSWORD_EXPORT_INTENT_REQUEST_CODE);
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent intent) {
        super.onActivityResult(requestCode, resultCode, intent);
        if (requestCode != PASSWORD_EXPORT_INTENT_REQUEST_CODE) return;
        if (resultCode != Activity.RESULT_OK) return;
        if (intent == null || intent.getData() == null) return;
        if (mOnClickHandler == null) return;

        mOnClickHandler.onSavePasswordsToDownloads(intent.getData());
    }

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        if (savedInstanceState != null) {
            mPaswordExportText = savedInstanceState.getString(PASSWORD_EXPORT_TEXT);
        }
    }

    @Override
    public void onSaveInstanceState(@NonNull Bundle outState) {
        super.onSaveInstanceState(outState);
        outState.putString(PASSWORD_EXPORT_TEXT, mPaswordExportText);
    }
}
