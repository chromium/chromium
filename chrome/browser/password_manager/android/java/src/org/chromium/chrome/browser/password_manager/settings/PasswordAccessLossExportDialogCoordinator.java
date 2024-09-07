// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager.settings;

import static org.chromium.chrome.browser.password_manager.settings.PasswordAccessLossExportDialogProperties.CLOSE_BUTTON_CALLBACK;
import static org.chromium.chrome.browser.password_manager.settings.PasswordAccessLossExportDialogProperties.EXPORT_AND_DELETE_BUTTON_CALLBACK;
import static org.chromium.chrome.browser.password_manager.settings.PasswordAccessLossExportDialogProperties.TITLE;

import android.view.LayoutInflater;
import android.view.View;

import androidx.fragment.app.FragmentActivity;

import org.chromium.chrome.browser.password_manager.PasswordStoreBridge;
import org.chromium.chrome.browser.password_manager.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Shows the dialog offering the user to export their passwords. If they accept, it runs the export
 * flow, namely: 1) Serializes user passwords and saves them to the file on disk. 2) Removes all
 * password from the profile store (it the previous step was successful).
 */
public class PasswordAccessLossExportDialogCoordinator {
    public interface Observer {
        void onPasswordsDeletionFinished();
    }

    private final FragmentActivity mActivity;
    private final PasswordAccessLossExportDialogFragment mFragment;
    private final PasswordAccessLossExportDialogMediator mMediator;

    public PasswordAccessLossExportDialogCoordinator(
            FragmentActivity activity,
            Profile profile,
            PasswordAccessLossExportDialogCoordinator.Observer exportDialogObserver) {
        mActivity = activity;
        View dialogView =
                LayoutInflater.from(mActivity)
                        .inflate(R.layout.password_access_loss_export_dialog_view, null);
        mFragment = new PasswordAccessLossExportDialogFragment();
        mMediator =
                new PasswordAccessLossExportDialogMediator(
                        activity,
                        profile,
                        dialogView.getId(),
                        mFragment,
                        new PasswordStoreBridge(profile),
                        exportDialogObserver);
        initialize(dialogView);
    }

    private void initialize(View dialogView) {
        mFragment.setView(dialogView);
        mFragment.setDelegate(mMediator);
        bindDialogView(dialogView);
    }

    private void bindDialogView(View dialogView) {
        PropertyModel model =
                new PropertyModel.Builder(PasswordAccessLossExportDialogProperties.ALL_KEYS)
                        .with(TITLE, mMediator.getDialogTitle())
                        .with(
                                EXPORT_AND_DELETE_BUTTON_CALLBACK,
                                mMediator::handlePositiveButtonClicked)
                        .with(CLOSE_BUTTON_CALLBACK, mMediator::onExportFlowCanceled)
                        .build();

        PropertyModelChangeProcessor.create(
                model, dialogView, PasswordAccessLossExportDialogBinder::bind);
    }

    public void showExportDialog() {
        mFragment.show(mActivity.getSupportFragmentManager(), null);
    }

    public PasswordAccessLossExportDialogMediator getMediatorForTesting() {
        return mMediator;
    }
}
