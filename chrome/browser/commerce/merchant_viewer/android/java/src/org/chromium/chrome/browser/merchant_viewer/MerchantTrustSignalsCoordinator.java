// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.merchant_viewer;

import android.content.Context;
import android.view.View;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.merchant_viewer.MerchantTrustMetrics.MessageClearReason;
import org.chromium.chrome.browser.merchant_viewer.proto.MerchantTrustSignalsOuterClass.MerchantTrustSignals;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.messages.DismissReason;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

import java.util.concurrent.TimeUnit;

/**
 * Coordinator for managing merchant trust signals experience.
 */
public class MerchantTrustSignalsCoordinator {
    private final MerchantTrustSignalsMediator mMediator;
    private final MerchantTrustMessageScheduler mMessageScheduler;
    private final MerchantTrustDetailsTabCoordinator mDetailsTabCoordinator;
    private final Context mContext;
    private final WindowAndroid mWindowAndroid;
    private final BottomSheetController mBottomSheetController;
    private final View mLayoutView;
    private final MerchantTrustMetrics mMetrics;
    private final MerchantTrustSignalsDataProvider mDataProvider;
    private final MerchantTrustSignalsEventStorage mStorage;

    /** Creates a new instance. */
    public MerchantTrustSignalsCoordinator(Context context, WindowAndroid windowAndroid,
            BottomSheetController bottomSheetController, View layoutView,
            TabModelSelector tabModelSelector, MessageDispatcher messageDispatcher,
            Supplier<Tab> tabSupplier, Supplier<Profile> profileSupplier,
            MerchantTrustMetrics metrics) {
        this(context, windowAndroid, bottomSheetController, layoutView, tabModelSelector,
                new MerchantTrustMessageScheduler(messageDispatcher, metrics), tabSupplier,
                new MerchantTrustSignalsDataProvider(),
                new MerchantTrustSignalsEventStorage(profileSupplier.get()), metrics);
    }

    @VisibleForTesting
    MerchantTrustSignalsCoordinator(Context context, WindowAndroid windowAndroid,
            BottomSheetController bottomSheetController, View layoutView,
            TabModelSelector tabModelSelector, MerchantTrustMessageScheduler messageScheduler,
            Supplier<Tab> tabSupplier, MerchantTrustSignalsDataProvider dataProvider,
            MerchantTrustSignalsEventStorage storage, MerchantTrustMetrics metrics) {
        mContext = context;
        mWindowAndroid = windowAndroid;
        mBottomSheetController = bottomSheetController;
        mLayoutView = layoutView;
        mDataProvider = dataProvider;
        mStorage = storage;
        mMetrics = metrics;

        mMediator = new MerchantTrustSignalsMediator(tabModelSelector, this::maybeDisplayMessage);
        mMessageScheduler = messageScheduler;
        mDetailsTabCoordinator = new MerchantTrustDetailsTabCoordinator(
                context, windowAndroid, bottomSheetController, tabSupplier, layoutView, mMetrics);
    }

    /** Cleans up internal state. */
    public void destroy() {
        mMediator.destroy();
    }

    @VisibleForTesting
    void maybeDisplayMessage(MerchantTrustMessageContext item) {
        MerchantTrustMessageContext scheduledMessage =
                mMessageScheduler.getScheduledMessageContext();
        if (scheduledMessage != null && scheduledMessage.getHostName() != null
                && scheduledMessage.getHostName().equals(item.getHostName())) {
            mMessageScheduler.expedite(this::onMessageEnqueued);
        } else {
            mMessageScheduler.clear(MessageClearReason.NAVIGATE_TO_DIFFERENT_DOMAIN);

            getDataForUnfamiliarMerchant(item.getUrl(), (trustSignals) -> {
                if (trustSignals == null) {
                    return;
                }

                mMessageScheduler.schedule(
                        MerchantTrustMessageViewModel.create(mContext, trustSignals,
                                this::onMessageDismissed, this::onMessagePrimaryAction),
                        item, MerchantViewerConfig.DEFAULT_TRUST_SIGNALS_MESSAGE_DELAY.getValue(),
                        this::onMessageEnqueued);
            });
        }
    }

    private void onMessageEnqueued(MerchantTrustMessageContext messageContext) {
        if (messageContext == null) {
            return;
        }
        mStorage.save(new MerchantTrustSignalsEvent(
                messageContext.getHostName(), System.currentTimeMillis()));
    }

    private void getDataForUnfamiliarMerchant(GURL url, Callback<MerchantTrustSignals> callback) {
        mStorage.load(url.getHost(), (event) -> {
            if (event == null) {
                mDataProvider.getDataForUrl(url, callback);
            } else if (System.currentTimeMillis() - event.getTimestamp()
                    > TimeUnit.SECONDS.toMillis(
                            MerchantViewerConfig.TRUST_SIGNALS_MESSAGE_WINDOW_DURATION_SECONDS
                                    .getValue())) {
                mStorage.delete(event);
                mDataProvider.getDataForUrl(url, callback);
            } else {
                callback.onResult(null);
            }
        });
    }

    @VisibleForTesting
    void onMessageDismissed(@DismissReason int dismissReason) {
        mMetrics.recordMetricsForMessageDismissed(dismissReason);
    }

    @VisibleForTesting
    void onMessagePrimaryAction(MerchantTrustSignals trustSignals) {
        mMetrics.recordMetricsForMessageTapped();
        launchDetailsPage(new GURL(trustSignals.getMerchantDetailsPageUrl()));
    }

    private void launchDetailsPage(GURL url) {
        mDetailsTabCoordinator.requestOpenSheet(url,
                mContext.getResources().getString(R.string.merchant_viewer_preview_sheet_title));
    }
}