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

import androidx.annotation.NonNull;

import org.chromium.base.Callback;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.commerce.ShoppingServiceFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.commerce.core.DiscountInfo;
import org.chromium.ui.base.Clipboard;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Iterator;
import java.util.List;
import java.util.Locale;

/** Mediator for discounts bottom sheet responsible for model list update. */
public class DiscountsBottomSheetContentMediator {
    private final Context mContext;
    private final Tab mTab;
    private final ModelList mModelList;

    public DiscountsBottomSheetContentMediator(
            @NonNull Context context,
            @NonNull Supplier<Tab> tabSupplier,
            @NonNull ModelList modelList) {
        mContext = context;
        mTab = tabSupplier.get();
        mModelList = modelList;
    }

    public void requestShowContent(Callback<Boolean> contentReadyCallback) {
        ShoppingServiceFactory.getForProfile(mTab.getProfile())
                .getDiscountInfoForUrl(
                        mTab.getUrl(),
                        (url, infoList) -> {
                            updateModelList(infoList);
                            contentReadyCallback.onResult(mModelList.size() > 0);
                        });
    }

    public void closeContent() {
        mModelList.clear();
    }

    private void updateModelList(List<DiscountInfo> infoList) {
        for (DiscountInfo info : infoList) {
            if (info == null || info.discountCode.isEmpty()) {
                continue;
            }
            PropertyModel propertyModel =
                    new PropertyModel.Builder(ALL_KEYS)
                            .with(DISCOUNT_CODE, info.discountCode.get())
                            .with(DESCRIPTION_DETAIL, info.descriptionDetail)
                            .with(EXPIRY_TIME, formatExpiryTime(info.expiryTimeSec))
                            .with(
                                    COPY_BUTTON_TEXT,
                                    mContext.getResources()
                                            .getString(R.string.discount_code_copy_button_text))
                            .build();
            propertyModel.set(
                    COPY_BUTTON_ON_CLICK_LISTENER, createCopyButtonOnClickListener(propertyModel));
            mModelList.add(new ListItem(0, propertyModel));
        }
    }

    private String formatExpiryTime(double expiryTimeSec) {
        Locale locale = mContext.getResources().getConfiguration().getLocales().get(0);
        String expiryTime =
                new SimpleDateFormat("MM/dd/yyyy", locale)
                        .format(new Date(Double.valueOf(expiryTimeSec * 1000).longValue()));
        return mContext.getResources()
                .getString(R.string.discount_expiration_date_android, expiryTime);
    }

    private OnClickListener createCopyButtonOnClickListener(PropertyModel propertyModel) {
        return view -> {
            Clipboard.getInstance().setText(propertyModel.get(DISCOUNT_CODE));
            resetCopiedButtonText();
            propertyModel.set(
                    COPY_BUTTON_TEXT,
                    mContext.getResources().getString(R.string.discount_code_copied_button_text));
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
                            mContext.getResources()
                                    .getString(R.string.discount_code_copy_button_text));
        }
    }
}
