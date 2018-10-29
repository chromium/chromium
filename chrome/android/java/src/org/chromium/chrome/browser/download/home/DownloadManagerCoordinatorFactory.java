// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home;

import android.app.Activity;
import android.content.ComponentName;

import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.download.ui.DownloadManagerUi;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.snackbar.SnackbarManager;

/** A helper class to build and return an {@link DownloadManagerCoordinator}. */
public class DownloadManagerCoordinatorFactory {
    /**
     * Returns an instance of a {@link DownloadManagerCoordinator} to be used in the UI.
     * @param activity           The parent {@link Activity}.
     * @param config             A {@link DownloadManagerUiConfig} to provide configuration params.
     * @param parentComponent    The parent component.
     * @param snackbarManager    The {@link SnackbarManager} that should be used to show snackbars.
     * @return                   A new {@link DownloadManagerCoordinator} instance.
     */
    public static DownloadManagerCoordinator create(Activity activity,
            DownloadManagerUiConfig config, SnackbarManager snackbarManager,
            ComponentName parentComponent) {
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.DOWNLOAD_HOME_V2)) {
            return new DownloadManagerCoordinatorImpl(
                    Profile.getLastUsedProfile(), activity, config, snackbarManager);
        } else {
            return new DownloadManagerUi(activity, config.isOffTheRecord, parentComponent,
                    config.isSeparateActivity, snackbarManager);
        }
    }
}
