// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.interstitial;

import android.content.Context;
import android.view.View;

import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.offline_items_collection.OfflineContentProvider;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Implementation of a {@link DownloadInterstitialCoordinator} that sets up a download interstitial.
 * The interstitial displays the progress of the most recent download and provides utilities
 * for the download once completed.
 */
public class DownloadInterstitialCoordinatorImpl implements DownloadInterstitialCoordinator {
    private final DownloadInterstitialView mView;
    private final DownloadInterstitialMediator mMediator;
    private final PropertyModelChangeProcessor mModelChangeProcessor;

    /**
     * Creates a new instance of the {@link DownloadInterstitialCoordinator} implementation.
     * @param context The activity context.
     * @param downloadUrl Url spec used for matching and binding the correct offline item.
     * @param provider An {@link OfflineContentProvider} to observe changes to downloads.
     * @param snackbarManager Snackbar manager for the current activity.
     */
    public DownloadInterstitialCoordinatorImpl(Context context, String downloadUrl,
            OfflineContentProvider provider, SnackbarManager snackbarManager) {
        mView = DownloadInterstitialView.create(context);
        PropertyModel model =
                new PropertyModel.Builder(DownloadInterstitialProperties.ALL_KEYS).build();
        mMediator = new DownloadInterstitialMediator(context, model, downloadUrl, provider,
                snackbarManager, SharedPreferencesManager.getInstance());
        mModelChangeProcessor = PropertyModelChangeProcessor.create(
                model, mView, DownloadInterstitialViewBinder::bind);
    }

    @Override
    public View getView() {
        return mView.getView();
    }

    @Override
    public void destroy() {
        mMediator.destroy();
        mModelChangeProcessor.destroy();
    }
}
