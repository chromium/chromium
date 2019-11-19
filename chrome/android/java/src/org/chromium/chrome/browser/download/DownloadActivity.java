// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.app.Activity;
import android.content.ComponentName;
import android.os.Bundle;

import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.SnackbarActivity;
import org.chromium.chrome.browser.download.home.DownloadManagerCoordinator;
import org.chromium.chrome.browser.download.home.DownloadManagerCoordinatorFactory;
import org.chromium.chrome.browser.download.home.DownloadManagerUiConfig;
import org.chromium.chrome.browser.download.home.filter.Filters;
import org.chromium.chrome.browser.download.items.OfflineContentAggregatorNotificationBridgeUiFactory;
import org.chromium.chrome.browser.modaldialog.AppModalPresenter;
import org.chromium.chrome.browser.util.IntentUtils;
import org.chromium.chrome.browser.util.UrlConstants;
import org.chromium.ui.base.ActivityAndroidPermissionDelegate;
import org.chromium.ui.base.AndroidPermissionDelegate;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManagerHolder;

import java.lang.ref.WeakReference;

/**
 * Activity for managing downloads handled through Chrome.
 */
public class DownloadActivity extends SnackbarActivity implements ModalDialogManagerHolder {
    private static final String BUNDLE_KEY_CURRENT_URL = "current_url";

    private DownloadManagerCoordinator mDownloadCoordinator;
    private boolean mIsOffTheRecord;
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

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        // Loads offline pages and prefetch downloads.
        OfflineContentAggregatorNotificationBridgeUiFactory.instance();
        boolean isOffTheRecord = DownloadUtils.shouldShowOffTheRecordDownloads(getIntent());
        boolean showPrefetchContent = DownloadUtils.shouldShowPrefetchContent(getIntent());
        ComponentName parentComponent = IntentUtils.safeGetParcelableExtra(
                getIntent(), IntentHandler.EXTRA_PARENT_COMPONENT);
        mPermissionDelegate =
                new ActivityAndroidPermissionDelegate(new WeakReference<Activity>(this));
        DownloadManagerUiConfig config =
                new DownloadManagerUiConfig.Builder()
                        .setIsOffTheRecord(isOffTheRecord)
                        .setIsSeparateActivity(true)
                        .setShowOfflineHome(DownloadUtils.shouldShowOfflineHome())
                        .build();

        mModalDialogManager = new ModalDialogManager(
                new AppModalPresenter(this), ModalDialogManager.ModalDialogType.APP);
        mDownloadCoordinator = DownloadManagerCoordinatorFactory.create(
                this, config, getSnackbarManager(), parentComponent, mModalDialogManager);
        setContentView(mDownloadCoordinator.getView());
        mIsOffTheRecord = isOffTheRecord;
        mDownloadCoordinator.addObserver(mUiObserver);

        // TODO(crbug/905893) : Use {@link Filters.toUrl) once old download home is removed.
        mCurrentUrl = savedInstanceState == null
                ? UrlConstants.DOWNLOADS_URL
                : savedInstanceState.getString(BUNDLE_KEY_CURRENT_URL);
        if (showPrefetchContent) mCurrentUrl = Filters.toUrl(Filters.FilterType.PREFETCHED);
        mDownloadCoordinator.updateForUrl(mCurrentUrl);
    }

    @Override
    protected void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);
        if (mCurrentUrl != null) outState.putString(BUNDLE_KEY_CURRENT_URL, mCurrentUrl);
    }

    @Override
    public void onResume() {
        super.onResume();
        DownloadUtils.checkForExternallyRemovedDownloads(mIsOffTheRecord);
    }

    @Override
    public void onBackPressed() {
        if (mDownloadCoordinator.onBackPressed()) return;
        super.onBackPressed();
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

    public AndroidPermissionDelegate getAndroidPermissionDelegate() {
        return mPermissionDelegate;
    }

    @Override
    public void onRequestPermissionsResult(
            int requestCode, String[] permissions, int[] grantResults) {
        mPermissionDelegate.handlePermissionResult(requestCode, permissions, grantResults);
    }
}
