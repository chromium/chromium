// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.commerce.coupons;

import static org.chromium.chrome.browser.commerce.coupons.DiscountsBottomSheetContentProperties.ALL_KEYS;
import static org.chromium.chrome.browser.commerce.coupons.DiscountsBottomSheetContentProperties.COPY_BUTTON_ON_CLICK_LISTENER;
import static org.chromium.chrome.browser.commerce.coupons.DiscountsBottomSheetContentProperties.COPY_BUTTON_TEXT;
import static org.chromium.chrome.browser.commerce.coupons.DiscountsBottomSheetContentProperties.DESCRIPTION_DETAIL;
import static org.chromium.chrome.browser.commerce.coupons.DiscountsBottomSheetContentProperties.DISCOUNT_CODE;
import static org.chromium.chrome.browser.commerce.coupons.DiscountsBottomSheetContentProperties.EXPIRY_TIME;

import android.content.Context;
import android.view.View.OnClickListener;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.commerce.ShoppingServiceFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.commerce.core.DiscountClusterType;
import org.chromium.components.commerce.core.DiscountInfo;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.ui.base.Clipboard;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Iterator;
import java.util.List;
import java.util.Locale;
import java.util.function.Supplier;

/** Mediator for discounts bottom sheet responsible for model list update. */
@NullMarked
public class DiscountsBottomSheetContentMediator {
    private final Context mContext;
    private final Supplier<@Nullable Tab> mTabSupplier;
    private final ModelList mModelList;

    private boolean mCopyButtonClickedHistogramRecorded;

    public DiscountsBottomSheetContentMediator(
            Context context, Supplier<@Nullable Tab> tabSupplier, ModelList modelList) {
        mContext = context;
        mTabSupplier = tabSupplier;
        mModelList = modelList;
    }

    public void requestShowContent(Callback<Boolean> contentReadyCallback) {
        Tab tab = mTabSupplier.get();
        if (tab == null) {
            contentReadyCallback.onResult(false);
            return;
        }
        ShoppingService shoppingService = ShoppingServiceFactory.getForProfile(tab.getProfile());
        if (shoppingService == null || !shoppingService.isDiscountEligibleToShowOnNavigation()) {
            contentReadyCallback.onResult(false);
            return;
        }
        shoppingService.getDiscountInfoForUrl(
                tab.getUrl(),
                (url, infoList) -> {
                    updateModelList(infoList);
                    contentReadyCallback.onResult(mModelList.size() > 0);
                });
        mCopyButtonClickedHistogramRecorded = false;
    }

    public void closeContent() {
        mModelList.clear();
    }

    private void updateModelList(@Nullable List<DiscountInfo> infoList) {
        if (infoList == null) {
            return;
        }
        for (DiscountInfo info : infoList) {
            if (info == null || info.discountCode == null) {
                continue;
            }
            PropertyModel.Builder propertyModelBuilder =
                    new PropertyModel.Builder(ALL_KEYS)
                            .with(DISCOUNT_CODE, info.discountCode)
                            .with(DESCRIPTION_DETAIL, info.descriptionDetail)
                            .with(
                                    COPY_BUTTON_TEXT,
                                    mContext.getString(R.string.discount_code_copy_button_text));
            if (info.expiryTimeSec != null) {
                propertyModelBuilder.with(EXPIRY_TIME, formatExpiryTime(info.expiryTimeSec));
            } else {
                propertyModelBuilder.with(EXPIRY_TIME, null);
            }
            PropertyModel propertyModel = propertyModelBuilder.build();
            propertyModel.set(
                    COPY_BUTTON_ON_CLICK_LISTENER,
                    createCopyButtonOnClickListener(propertyModel, info));
            mModelList.add(new ListItem(0, propertyModel));
        }
    }

    private String formatExpiryTime(double expiryTimeSec) {
        Locale locale = mContext.getResources().getConfiguration().getLocales().get(0);
        String expiryTime =
                new SimpleDateFormat("MM/dd/yyyy", locale)
                        .format(new Date(Double.valueOf(expiryTimeSec * 1000).longValue()));
        return mContext.getString(R.string.discount_expiration_date_android, expiryTime);
    }

    private OnClickListener createCopyButtonOnClickListener(
            PropertyModel propertyModel, DiscountInfo discountInfo) {
        return view -> {
            if (!mCopyButtonClickedHistogramRecorded) {
                RecordHistogram.recordEnumeratedHistogram(
                        "Commerce.Discounts.BottomSheet.ClusterTypeOnCopy",
                        discountInfo.clusterType,
                        DiscountClusterType.MAX_VALUE);
                mCopyButtonClickedHistogramRecorded = true;
            }

            Clipboard.getInstance().setText(propertyModel.get(DISCOUNT_CODE));
            resetCopiedButtonText();
            propertyModel.set(
                    COPY_BUTTON_TEXT,
                    mContext.getString(R.string.discount_code_copied_button_text));
        };
    }

    private void resetCopiedButtonText() {
        if (mModelList.size() <= 1) {
            return;
        }
        Iterator<ListItem> iterator = mModelList.iterator();
        while (iterator.hasNext()) {
            iterator.next()
                    .model
                    .set(
                            COPY_BUTTON_TEXT,
                            mContext.getString(R.string.discount_code_copy_button_text));
        }
    }
}
