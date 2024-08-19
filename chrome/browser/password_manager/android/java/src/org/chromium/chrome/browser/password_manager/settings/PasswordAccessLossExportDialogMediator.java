// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager.settings;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.view.View;

import androidx.fragment.app.FragmentActivity;
import androidx.fragment.app.FragmentManager;

import org.chromium.chrome.browser.profiles.Profile;

/**
 * The mediator for the password access loss warning export flow. It implements the {@link
 * ExportFlowInterface.Delegate} and contains the dialog buttons callbacks.
 */
class PasswordAccessLossExportDialogMediator
        implements ExportFlowInterface.Delegate,
                PasswordAccessLossExportDialogFragment.Delegate,
                PasswordListObserver {
    private final FragmentActivity mActivity;
    private final Profile mProfile;
    private final View mDialogView;
    private final PasswordAccessLossExportDialogFragment mExportDialogFragment;
    private ExportFlow mExportFlow;

    public PasswordAccessLossExportDialogMediator(
            FragmentActivity activity,
            Profile profile,
            View dialogView,
            PasswordAccessLossExportDialogFragment exportDialogFragment) {
        mActivity = activity;
        mProfile = profile;
        mDialogView = dialogView;
        mExportDialogFragment = exportDialogFragment;
    }

    public void handlePositiveButtonClicked() {
        PasswordManagerHandlerProvider.getForProfile(mProfile).addObserver(this);
        mExportFlow = new ExportFlow();
        // TODO (crbug.com/354876446): Handle metrics in separate CL.
        mExportFlow.onCreate(new Bundle(), this, "");
        mExportFlow.startExporting();
    }

    // Implementation of ExportFlowInterface.Delegate.
    @Override
    public Activity getActivity() {
        return mActivity;
    }

    @Override
    public FragmentManager getFragmentManager() {
        return mActivity.getSupportFragmentManager();
    }

    @Override
    public int getViewId() {
        return mDialogView.getId();
    }

    @Override
    public void runCreateFileOnDiskIntent(Intent intent) {
        assert mExportDialogFragment != null
                : "Password access loss export dialog was already dismissed!";
        mExportDialogFragment.runCreateFileOnDiskIntent();
    }

    @Override
    public void onExportFlowSucceeded() {
        // TODO(crbug.com/356850960): Remove passwords here.
        destroy();
    }

    @Override
    public Profile getProfile() {
        return mProfile;
    }

    // Implementation of PasswordListObserver.
    @Override
    public void passwordListAvailable(int count) {
        if (mExportFlow == null) return;
        mExportFlow.passwordsAvailable();
    }

    @Override
    public void passwordExceptionListAvailable(int count) {
        // Not used here.
    }

    // Implementation of PasswordAccessLossExportDialogFragment.Delegate.
    @Override
    public void onDocumentCreated(Uri uri) {
        if (mExportFlow != null) {
            mExportFlow.savePasswordsToDownloads(uri);
        }
        mExportDialogFragment.dismiss();
    }

    @Override
    public void onResume() {
        if (mExportFlow == null) return;
        mExportFlow.onResume();
    }

    @Override
    public void onExportFlowFailed() {
        mExportDialogFragment.dismiss();
    }

    @Override
    public void onExportFlowCanceled() {
        destroy();
    }

    private void destroy() {
        if (mExportDialogFragment.getShowsDialog()) {
            mExportDialogFragment.dismiss();
        }
        mExportFlow = null;
        if (PasswordManagerHandlerProvider.getForProfile(mProfile).getPasswordManagerHandler()
                != null) {
            PasswordManagerHandlerProvider.getForProfile(mProfile).removeObserver(this);
        }
    }
}
