// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.merchant_viewer;

import android.content.Context;
import android.view.View;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.merchant_viewer.MerchantTrustMetrics.MessageClearReason;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.messages.DismissReason;
import org.chromium.components.messages.MessageBannerProperties;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.concurrent.TimeUnit;

/**
 * Coordinator for managing merchant trust signals experience.
 */
public class MerchantTrustSignalsCoordinator {
    // TODO: Make the value configurable.
    @VisibleForTesting
    public static final long MESSAGE_ENQUEUE_DELAY_MILLIS = TimeUnit.SECONDS.toMillis(2);
    private final MerchantTrustSignalsMediator mMediator;
    private final MerchantTrustMessageScheduler mMessageScheduler;
    private final MerchantTrustDetailsTabCoordinator mDetailsTabCoordinator;
    private final Context mContext;
    private final WindowAndroid mWindowAndroid;
    private final BottomSheetController mBottomSheetController;
    private final View mLayoutView;
    private final MerchantTrustMetrics mMetrics;

    /** Creates a new instance. */
    public MerchantTrustSignalsCoordinator(Context context, WindowAndroid windowAndroid,
            BottomSheetController bottomSheetController, View layoutView,
            TabModelSelector tabModelSelector, MerchantTrustMessageScheduler messageScheduler,
            Supplier<Tab> tabSupplier, MerchantTrustMetrics metrics) {
        mContext = context;
        mWindowAndroid = windowAndroid;
        mBottomSheetController = bottomSheetController;
        mLayoutView = layoutView;
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
            MerchantTrustMessageContext replacementMessage = new MerchantTrustMessageContext(
                    scheduledMessage.getHostName(), scheduledMessage.getWebContents());
            mMessageScheduler.clear(MessageClearReason.NAVIGATE_TO_SAME_DOMAIN);
            mMessageScheduler.schedule(getMessagePropertyModel(replacementMessage.getHostName()),
                    replacementMessage, MerchantTrustMessageScheduler.MESSAGE_ENQUEUE_NO_DELAY);
        } else {
            mMessageScheduler.clear(MessageClearReason.NAVIGATE_TO_DIFFERENT_DOMAIN);
            if (!isUnfamiliarMerchant(item.getHostName())) {
                return;
            }

            mMessageScheduler.schedule(getMessagePropertyModel(item.getHostName()), item,
                    MESSAGE_ENQUEUE_DELAY_MILLIS);
        }
    }

    private boolean isUnfamiliarMerchant(String hostname) {
        // TODO: Check if the user has never seen the message for hostname before.
        return true;
    }

    private PropertyModel getMessagePropertyModel(String hostname) {
        // TODO: populate the PropertyModel fields.
        return new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                .with(MessageBannerProperties.ON_PRIMARY_ACTION, this::onMessageTapped)
                .with(MessageBannerProperties.ON_DISMISSED, this::onMessageDismissed)
                .build();
    }

    @VisibleForTesting
    void onMessageTapped() {
        mMetrics.recordMetricsForMessageTapped();
    }

    @VisibleForTesting
    void onMessageDismissed(@DismissReason int dismissReason) {
        mMetrics.recordMetricsForMessageDismissed(dismissReason);
    }
}