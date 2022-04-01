// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.interstitial;

import android.content.Context;

import org.chromium.chrome.browser.download.items.OfflineContentAggregatorFactory;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManagerProvider;
import org.chromium.ui.base.WindowAndroid;

/** Factory class to build a {@link DownloadInterstitialCoordinator} instance. */
public class DownloadInterstitialCoordinatorFactory {
    /**
     * @param context The activity context.
     * @param downloadUrl Url spec used for matching and binding the correct offline item.
     * @param windowAndroid The {@link WindowAndroid} associated with the activity.
     * @return A new {@link DownloadInterstitialCoordinatorImpl} instance.
     */
    public static DownloadInterstitialCoordinator create(
            Context context, String downloadUrl, WindowAndroid windowAndroid) {
        return new DownloadInterstitialCoordinatorImpl(context, downloadUrl,
                OfflineContentAggregatorFactory.get(), SnackbarManagerProvider.from(windowAndroid));
    }

    private DownloadInterstitialCoordinatorFactory() {}
}
