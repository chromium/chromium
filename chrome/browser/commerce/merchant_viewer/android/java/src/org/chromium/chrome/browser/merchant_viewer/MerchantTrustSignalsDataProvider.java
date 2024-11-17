// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.merchant_viewer;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.commerce.ShoppingServiceFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.commerce.core.ShoppingService.MerchantInfo;
import org.chromium.url.GURL;

/** Merchant trust data provider via {@link ShoppingServiceFactory}. */
class MerchantTrustSignalsDataProvider {
    /** Fetches {@link MerchantInfo} based on {@link GURL}. */
    public void getDataForUrl(Profile profile, GURL url, Callback<MerchantInfo> callback) {
        if (profile == null || profile.isOffTheRecord()) {
            callback.onResult(null);
            return;
        }
        ShoppingServiceFactory.getForProfile(profile)
                .getMerchantInfoForUrl(
                        url,
                        (gurl, info) ->
                                callback.onResult(isValidMerchantTrustSignals(info) ? info : null));
    }

    @VisibleForTesting
    boolean isValidMerchantTrustSignals(MerchantInfo info) {
        return (info != null)
                && (info.detailsPageUrl != null)
                && !info.containsSensitiveContent
                && (info.starRating > 0 || info.hasReturnPolicy);
    }
}
