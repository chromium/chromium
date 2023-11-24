// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.interstitial;

import android.content.Context;
import android.view.View;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.modaldialog.AppModalPresenter;
import org.chromium.components.offline_items_collection.OfflineContentProvider;
import org.chromium.ui.modaldialog.ModalDialogManager;
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
     * @param contextSupplier Supplier which provides the context of the parent tab.
     * @param downloadUrl Url spec used for matching and binding the correct offline item.
     * @param provider An {@link OfflineContentProvider} to observe changes to downloads.
     * @param snackbarManager Snackbar manager for the current activity.
     * @param reloadCallback Callback run to reload the tab therefore restarting the download.
     */
    public DownloadInterstitialCoordinatorImpl(
            Supplier<Context> contextSupplier,
            String downloadUrl,
            OfflineContentProvider provider,
            SnackbarManager snackbarManager,
            Runnable reloadCallback) {
        mView = DownloadInterstitialView.create(contextSupplier.get());
        PropertyModel model =
                new PropertyModel.Builder(DownloadInterstitialProperties.ALL_KEYS).build();
        model.set(DownloadInterstitialProperties.RELOAD_TAB, reloadCallback);
        ModalDialogManager modalDialogManager =
                new ModalDialogManager(
                        new AppModalPresenter(contextSupplier.get()),
                        ModalDialogManager.ModalDialogType.APP);
        mMediator =
                new DownloadInterstitialMediator(
                        contextSupplier,
                        model,
                        downloadUrl,
                        provider,
                        snackbarManager,
                        modalDialogManager);
        mModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        model, mView, DownloadInterstitialViewBinder::bind);
    }

    @Override
    public View getView() {
        return mView.getView();
    }

    @Override
    public void onTabReparented(Context context) {
        // Update the ModalDialogManager as the context has changed.
        mMediator.setModalDialogManager(
                new ModalDialogManager(
                        new AppModalPresenter(context), ModalDialogManager.ModalDialogType.APP));
    }

    @Override
    public void destroy() {
        mMediator.destroy();
        mModelChangeProcessor.destroy();
    }
}
