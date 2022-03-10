// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.download;

import org.chromium.chrome.browser.download.interstitial.DownloadInterstitialCoordinator;
import org.chromium.chrome.browser.download.interstitial.DownloadInterstitialCoordinatorFactory;
import org.chromium.chrome.browser.download.items.OfflineContentAggregatorFactory;
import org.chromium.chrome.browser.tab.Tab;

/** A helper class to build and return a {@link DownloadInterstitialCoordinator}. */
public class DownloadInterstitialCoordinatorFactoryHelper {
    /**
     * @param tab The tab which will show the download interstitial.
     * @return A new {@link DownloadInterstitialCoordinator} instance.
     */
    public static DownloadInterstitialCoordinator create(Tab tab) {
        return DownloadInterstitialCoordinatorFactory.create(
                tab.getContext(), OfflineContentAggregatorFactory.get(), tab.getWindowAndroid());
    }

    private DownloadInterstitialCoordinatorFactoryHelper() {}
}
