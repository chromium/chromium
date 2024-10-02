// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.download.home;

import android.app.Activity;
import android.content.Context;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.download.home.DownloadManagerCoordinator;
import org.chromium.chrome.browser.download.home.DownloadManagerCoordinatorFactory;
import org.chromium.chrome.browser.download.home.DownloadManagerUiConfig;
import org.chromium.chrome.browser.download.items.OfflineContentAggregatorFactory;
import org.chromium.chrome.browser.download.settings.DownloadSettings;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.profiles.OTRProfileID;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.components.browser_ui.util.GlobalDiscardableReferencePool;
import org.chromium.ui.modaldialog.ModalDialogManager;

/** A helper class to build and return an {@link DownloadManagerCoordinator}. */
class DownloadManagerCoordinatorFactoryHelper {
    /**
     * Returns an instance of a {@link DownloadManagerCoordinator} to be used in the UI.
     * @param activity           The parent {@link Activity}.
     * @param config             A {@link DownloadManagerUiConfig} to provide configuration params.
     * @param snackbarManager    The {@link SnackbarManager} that should be used to show snackbars.
     * @param modalDialogManager The {@link ModalDialogManager} that should be used to show dialog.
     * @return                   A new {@link DownloadManagerCoordinator} instance.
     */
    public static DownloadManagerCoordinator create(
            Activity activity,
            DownloadManagerUiConfig config,
            SnackbarManager snackbarManager,
            ModalDialogManager modalDialogManager) {
        Profile profile =
                OTRProfileID.isOffTheRecord(config.otrProfileID)
                        ? ProfileManager.getLastUsedRegularProfile()
                                .getOffTheRecordProfile(
                                        config.otrProfileID, /* createIfNeeded= */ true)
                        : ProfileManager.getLastUsedRegularProfile();
        Callback<Context> settingsLaunchHelper =
                DownloadManagerCoordinatorFactoryHelper::settingsLaunchHelper;
        return DownloadManagerCoordinatorFactory.create(
                activity,
                config,
                new PrefetchEnabledSupplier(),
                settingsLaunchHelper,
                snackbarManager,
                modalDialogManager,
                TrackerFactory.getTrackerForProfile(profile),
                new FaviconProviderImpl(profile),
                OfflineContentAggregatorFactory.get(),
                GlobalDiscardableReferencePool.getReferencePool());
    }

    private static void settingsLaunchHelper(Context context) {
        SettingsNavigation settingsNavigation =
                SettingsNavigationFactory.createSettingsNavigation();
        settingsNavigation.startSettings(context, DownloadSettings.class);
    }
}
