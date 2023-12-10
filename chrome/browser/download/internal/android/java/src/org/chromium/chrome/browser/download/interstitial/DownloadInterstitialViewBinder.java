// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.interstitial;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** View binder for DownloadInterstitialView. */
public class DownloadInterstitialViewBinder {
    public static void bind(PropertyModel model, DownloadInterstitialView view, PropertyKey key) {
        if (key.equals(DownloadInterstitialProperties.DOWNLOAD_ITEM)) {
            view.updateFileInfo(model.get(DownloadInterstitialProperties.DOWNLOAD_ITEM), model);
        } else if (key.equals(DownloadInterstitialProperties.TITLE_TEXT)) {
            view.setTitleText(model.get(DownloadInterstitialProperties.TITLE_TEXT));
        } else if (key.equals(DownloadInterstitialProperties.PRIMARY_BUTTON_IS_VISIBLE)) {
            view.setPrimaryButtonVisibility(
                    model.get(DownloadInterstitialProperties.PRIMARY_BUTTON_IS_VISIBLE));
        } else if (key.equals(DownloadInterstitialProperties.PRIMARY_BUTTON_TEXT)) {
            view.setPrimaryButtonText(
                    model.get(DownloadInterstitialProperties.PRIMARY_BUTTON_TEXT));
        } else if (key.equals(DownloadInterstitialProperties.PRIMARY_BUTTON_CALLBACK)) {
            view.setPrimaryButtonCallback(
                    model.get(DownloadInterstitialProperties.PRIMARY_BUTTON_CALLBACK)
                            .bind(model.get(DownloadInterstitialProperties.DOWNLOAD_ITEM)));
        } else if (key.equals(DownloadInterstitialProperties.SECONDARY_BUTTON_IS_VISIBLE)) {
            view.setSecondaryButtonVisibility(
                    model.get(DownloadInterstitialProperties.SECONDARY_BUTTON_IS_VISIBLE));
        } else if (key.equals(DownloadInterstitialProperties.SECONDARY_BUTTON_TEXT)) {
            view.setSecondaryButtonText(
                    model.get(DownloadInterstitialProperties.SECONDARY_BUTTON_TEXT));
        } else if (key.equals(DownloadInterstitialProperties.SECONDARY_BUTTON_CALLBACK)) {
            view.setSecondaryButtonCallback(
                    model.get(DownloadInterstitialProperties.SECONDARY_BUTTON_CALLBACK)
                            .bind(model.get(DownloadInterstitialProperties.DOWNLOAD_ITEM)));
        } else if (key.equals(DownloadInterstitialProperties.PENDING_MESSAGE_IS_VISIBLE)) {
            view.setPendingMessageIsVisible(
                    model.get(DownloadInterstitialProperties.PENDING_MESSAGE_IS_VISIBLE));
        } else if (key.equals(DownloadInterstitialProperties.STATE)
                && model.get(DownloadInterstitialProperties.STATE)
                        == DownloadInterstitialProperties.State.CANCELLED) {
            view.switchToCancelledViewHolder(
                    model.get(DownloadInterstitialProperties.DOWNLOAD_ITEM), model);
        }
    }
}
