// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.view.View;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.download.home.DownloadManagerCoordinator;
import org.chromium.chrome.browser.download.home.DownloadManagerCoordinatorFactory;
import org.chromium.chrome.browser.download.home.DownloadManagerUiConfig;
import org.chromium.chrome.browser.native_page.BasicNativePage;
import org.chromium.chrome.browser.native_page.NativePageHost;
import org.chromium.chrome.browser.snackbar.SnackbarManager.SnackbarManageable;

/**
 * Native page for managing downloads handled through Chrome.
 */
public class DownloadPage extends BasicNativePage implements DownloadManagerCoordinator.Observer {
    private ActivityStateListener mActivityStateListener;

    private DownloadManagerCoordinator mDownloadCoordinator;
    private String mTitle;

    /**
     * Create a new instance of the downloads page.
     * @param activity The activity to get context and manage fragments.
     * @param host A NativePageHost to load urls.
     */
    public DownloadPage(ChromeActivity activity, NativePageHost host) {
        super(activity, host);
    }

    @Override
    protected void initialize(ChromeActivity activity, final NativePageHost host) {
        ThreadUtils.assertOnUiThread();

        DownloadManagerUiConfig config = new DownloadManagerUiConfig.Builder()
                                                 .setIsOffTheRecord(host.isIncognito())
                                                 .setIsSeparateActivity(false)
                                                 .build();
        mDownloadCoordinator = DownloadManagerCoordinatorFactory.create(activity, config,
                ((SnackbarManageable) activity).getSnackbarManager(), activity.getComponentName());

        mDownloadCoordinator.addObserver(this);
        mTitle = activity.getString(R.string.menu_downloads);

        // #destroy() unregisters the ActivityStateListener to avoid checking for externally removed
        // downloads after the downloads page is closed. This requires each DownloadPage to have its
        // own ActivityStateListener. If multiple tabs are showing the downloads page, multiple
        // requests to check for externally removed downloads will be issued when the activity is
        // resumed.
        mActivityStateListener = (activity1, newState) -> {
            if (newState == ActivityState.RESUMED) {
                DownloadUtils.checkForExternallyRemovedDownloads(host.isIncognito());
            }
        };
        ApplicationStatus.registerStateListenerForActivity(mActivityStateListener, activity);
    }

    @Override
    public View getView() {
        return mDownloadCoordinator.getView();
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
        onStateChange(url);
    }
}
