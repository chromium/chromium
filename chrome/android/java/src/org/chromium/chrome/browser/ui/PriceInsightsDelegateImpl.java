// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui;

import android.content.Context;
import android.view.View;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.AppHooks;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.BookmarkUtils;
import org.chromium.chrome.browser.commerce.PriceTrackingUtils;
import org.chromium.chrome.browser.price_insights.PriceInsightsBottomSheetCoordinator.PriceInsightsDelegate;
import org.chromium.chrome.browser.price_tracking.PriceDropNotificationManagerFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.commerce.core.ShoppingService.PriceInsightsInfo;

public class PriceInsightsDelegateImpl implements PriceInsightsDelegate {

    private final Context mContext;
    private final ObservableSupplier<Boolean> mPriceTrackingStateSupplier;

    public PriceInsightsDelegateImpl(
            Context context, ObservableSupplier<Boolean> priceTrackingStateSupplier) {
        mContext = context;
        mPriceTrackingStateSupplier = priceTrackingStateSupplier;
    }

    @Override
    public ObservableSupplier<Boolean> getPriceTrackingStateSupplier(Tab tab) {
        return mPriceTrackingStateSupplier;
    }

    @Override
    public void setPriceTrackingStateForTab(Tab tab, boolean enabled, Callback<Boolean> callback) {
        BookmarkModel bookmarkModel = BookmarkModel.getForProfile(tab.getProfile());
        if (bookmarkModel == null) {
            callback.onResult(false);
            return;
        }
        BookmarkId bookmarkId = bookmarkModel.getUserBookmarkIdForTab(tab);
        bookmarkId =
                bookmarkId != null
                        ? bookmarkId
                        : BookmarkUtils.addBookmarkWithoutShowingSaveFlow(
                                mContext, tab, bookmarkModel);

        Callback<Boolean> wrapperCallback =
                (success) -> {
                    callback.onResult(success);
                    if (success) {
                        PriceDropNotificationManagerFactory.create(tab.getProfile())
                                .createNotificationChannel();
                    }
                };

        PriceTrackingUtils.setPriceTrackingStateForBookmark(
                tab.getProfile(), bookmarkId.getId(), enabled, wrapperCallback);
    }

    @Override
    public View getPriceHistoryChartForPriceInsightsInfo(PriceInsightsInfo info) {
        return AppHooks.get().getLineChartForPriceInsightsInfo(mContext, info);
    }
}
