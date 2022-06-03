// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps.launchpad;

import android.os.Bundle;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.SnackbarActivity;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.components.browser_ui.modaldialog.AppModalPresenter;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManagerHolder;

/**
 * Activity for displaying the app launcher.
 */
public class LaunchpadActivity extends SnackbarActivity implements ModalDialogManagerHolder {
    private LaunchpadCoordinator mLaunchpadCoordinator;
    private ModalDialogManager mModalDialogManager;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        mModalDialogManager = new ModalDialogManager(
                new AppModalPresenter(this), ModalDialogManager.ModalDialogType.APP);

        mLaunchpadCoordinator = new LaunchpadCoordinator(this, this::getModalDialogManager,
                new SettingsLauncherImpl(), LaunchpadUtils.retrieveWebApks(this),
                true /* isSeparateActivity */);
        setContentView(mLaunchpadCoordinator.getView());
    }

    @Override
    public ModalDialogManager getModalDialogManager() {
        return mModalDialogManager;
    }

    @Override
    protected void onDestroy() {
        mLaunchpadCoordinator.destroy();
        mLaunchpadCoordinator = null;
        super.onDestroy();
    }

    @VisibleForTesting
    LaunchpadCoordinator getCoordinatorForTesting() {
        return mLaunchpadCoordinator;
    }
}
