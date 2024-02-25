// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.download.home;

import android.app.Activity;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.download.DownloadUtils;
import org.chromium.chrome.browser.download.home.DownloadManagerCoordinator;
import org.chromium.chrome.browser.download.home.DownloadManagerUiConfig;
import org.chromium.chrome.browser.download.home.DownloadManagerUiConfigHelper;
import org.chromium.chrome.browser.profiles.OTRProfileID;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.native_page.BasicNativePage;
import org.chromium.chrome.browser.ui.native_page.NativePageHost;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.ui.modaldialog.ModalDialogManager;

/** Native page for managing downloads handled through Chrome. */
public class DownloadPage extends BasicNativePage implements DownloadManagerCoordinator.Observer {
    private ActivityStateListener mActivityStateListener;

    private DownloadManagerCoordinator mDownloadCoordinator;
    private String mTitle;

    /**
     * Create a new instance of the downloads page.
     * @param activity The activity to get context and manage fragments.
     * @param snackbarManager The {@link SnackbarManager} to show snack bars.
     * @param modalDialogManager The {@link ModalDialogManager} associated with the activity.
     * @param otrProfileId The {@link OTRProfileID} for the profile. Null for regular profile.
     * @param host A NativePageHost to load urls.
     */
    public DownloadPage(
            Activity activity,
            SnackbarManager snackbarManager,
            ModalDialogManager modalDialogManager,
            OTRProfileID otrProfileId,
            NativePageHost host) {
        super(host);

        ThreadUtils.assertOnUiThread();

        DownloadManagerUiConfig config =
                DownloadManagerUiConfigHelper.fromFlags()
                        .setOTRProfileID(otrProfileId)
                        .setIsSeparateActivity(false)
                        .setShowPaginationHeaders(DownloadUtils.shouldShowPaginationHeaders())
                        .build();

        mDownloadCoordinator =
                DownloadManagerCoordinatorFactoryHelper.create(
                        activity, config, snackbarManager, modalDialogManager);

        mDownloadCoordinator.addObserver(this);
        mTitle = activity.getString(R.string.menu_downloads);

        ApplicationStatus.registerStateListenerForActivity(mActivityStateListener, activity);

        initWithView(mDownloadCoordinator.getView());
    }

    @Override
    public String getTitle() {
        return mTitle;
    }

    @Override
    public String getHost() {
        return UrlConstants.DOWNLOADS_HOST;
    }

    @Override
    public void updateForUrl(String url) {
        super.updateForUrl(url);
        mDownloadCoordinator.updateForUrl(url);
    }

    @Override
    public void destroy() {
        mDownloadCoordinator.removeObserver(this);
        mDownloadCoordinator.destroy();
        mDownloadCoordinator = null;
        ApplicationStatus.unregisterActivityStateListener(mActivityStateListener);
        super.destroy();
    }

    // DownloadManagerCoordinator.Observer implementation.
    @Override
    public void onUrlChanged(String url) {
        // We want to squash consecutive download home URLs having different filters into the one
        // having the latest filter. This will avoid requiring user to press back button too many
        // times to exit download home. In the event, chrome gets killed or if user navigates away
        // from download home, we still will be able to come back to the latest filter.
        onStateChange(url, true);
    }
}
