// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.download.home;

import android.app.Activity;
import android.os.Bundle;

import org.chromium.chrome.browser.SnackbarActivity;
import org.chromium.chrome.browser.back_press.BackPressHelper;
import org.chromium.chrome.browser.back_press.SecondaryActivityBackPressUma.SecondaryActivity;
import org.chromium.chrome.browser.download.DownloadUtils;
import org.chromium.chrome.browser.download.home.DownloadManagerCoordinator;
import org.chromium.chrome.browser.download.home.DownloadManagerUiConfig;
import org.chromium.chrome.browser.download.home.DownloadManagerUiConfigHelper;
import org.chromium.chrome.browser.download.items.OfflineContentAggregatorNotificationBridgeUiFactory;
import org.chromium.chrome.browser.profiles.OTRProfileID;
import org.chromium.components.browser_ui.modaldialog.AppModalPresenter;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManagerHolder;
import org.chromium.ui.permissions.ActivityAndroidPermissionDelegate;
import org.chromium.ui.permissions.AndroidPermissionDelegate;

import java.lang.ref.WeakReference;

/** Activity for managing downloads handled through Chrome. */
public class DownloadActivity extends SnackbarActivity implements ModalDialogManagerHolder {
    private static final String BUNDLE_KEY_CURRENT_URL = "current_url";

    private DownloadManagerCoordinator mDownloadCoordinator;
    private AndroidPermissionDelegate mPermissionDelegate;
    private ModalDialogManager mModalDialogManager;

    /** Caches the current URL for the filter being applied. */
    private String mCurrentUrl;

    private final DownloadManagerCoordinator.Observer mUiObserver =
            new DownloadManagerCoordinator.Observer() {
                @Override
                public void onUrlChanged(String url) {
                    mCurrentUrl = url;
                }
            };
    private OTRProfileID mOtrProfileID;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // If the profile doesn't exist, then do not perform any action.
        if (!DownloadUtils.doesProfileExistFromIntent(getIntent())) finish();

        mCurrentUrl =
                savedInstanceState == null
                        ? UrlConstants.DOWNLOADS_URL
                        : savedInstanceState.getString(BUNDLE_KEY_CURRENT_URL);

        // Loads offline pages and prefetch downloads.
        OfflineContentAggregatorNotificationBridgeUiFactory.instance();
        boolean showPrefetchContent =
                DownloadActivityLauncher.shouldShowPrefetchContent(getIntent());
        mPermissionDelegate =
                new ActivityAndroidPermissionDelegate(new WeakReference<Activity>(this));
        mOtrProfileID = DownloadUtils.getOTRProfileIDFromIntent(getIntent());

        DownloadManagerUiConfig config =
                DownloadManagerUiConfigHelper.fromFlags()
                        .setOTRProfileID(mOtrProfileID)
                        .setIsSeparateActivity(true)
                        .setShowPaginationHeaders(DownloadUtils.shouldShowPaginationHeaders())
                        .setStartWithPrefetchedContent(showPrefetchContent)
                        .build();

        mModalDialogManager =
                new ModalDialogManager(
                        new AppModalPresenter(this), ModalDialogManager.ModalDialogType.APP);
        mDownloadCoordinator =
                DownloadManagerCoordinatorFactoryHelper.create(
                        this, config, getSnackbarManager(), mModalDialogManager);
        setContentView(mDownloadCoordinator.getView());
        if (!showPrefetchContent) mDownloadCoordinator.updateForUrl(mCurrentUrl);
        mDownloadCoordinator.addObserver(mUiObserver);
        BackPressHelper.create(
                this,
                getOnBackPressedDispatcher(),
                mDownloadCoordinator.getBackPressHandlers(),
                SecondaryActivity.DOWNLOAD);
    }

    @Override
    protected void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);
        if (mCurrentUrl != null) outState.putString(BUNDLE_KEY_CURRENT_URL, mCurrentUrl);
    }

    @Override
    protected void onDestroy() {
        mDownloadCoordinator.removeObserver(mUiObserver);
        mDownloadCoordinator.destroy();
        mModalDialogManager.destroy();
        super.onDestroy();
    }

    @Override
    public ModalDialogManager getModalDialogManager() {
        return mModalDialogManager;
    }

    @Override
    @SuppressWarnings("MissingSuperCall")
    public void onRequestPermissionsResult(
            int requestCode, String[] permissions, int[] grantResults) {
        mPermissionDelegate.handlePermissionResult(requestCode, permissions, grantResults);
    }
}
