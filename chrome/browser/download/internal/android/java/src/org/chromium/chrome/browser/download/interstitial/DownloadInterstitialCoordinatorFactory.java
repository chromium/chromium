// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.interstitial;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;

import org.chromium.base.supplier.Supplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.download.items.OfflineContentAggregatorFactory;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManagerProvider;
import org.chromium.ui.base.WindowAndroid;

/** Factory class to build a {@link DownloadInterstitialCoordinator} instance. */
@NullMarked
public class DownloadInterstitialCoordinatorFactory {
    /**
     * @param contextSupplier Supplier which provides the context of the parent tab.
     * @param downloadUrl Url spec used for matching and binding the correct offline item.
     * @param windowAndroid The {@link WindowAndroid} associated with the activity.
     * @param reloadCallback Callback run to reload the tab therefore restarting the download.
     * @return A new {@link DownloadInterstitialCoordinatorImpl} instance.
     */
    public static DownloadInterstitialCoordinator create(
            Supplier<Context> contextSupplier,
            String downloadUrl,
            WindowAndroid windowAndroid,
            Runnable reloadCallback) {
        return new DownloadInterstitialCoordinatorImpl(
                contextSupplier,
                downloadUrl,
                OfflineContentAggregatorFactory.get(),
                assumeNonNull(windowAndroid.getModalDialogManager()),
                SnackbarManagerProvider.from(windowAndroid),
                reloadCallback);
    }

    private DownloadInterstitialCoordinatorFactory() {}
}
