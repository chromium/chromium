// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.merchant_viewer;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.messages.MessageBannerProperties;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.concurrent.TimeUnit;

/**
 * Coordinator for managing merchant trust signals experience.
 */
public class MerchantTrustSignalsCoordinator {
    // TODO: Make the value configurable.
    @VisibleForTesting
    public static final long MESSAGE_ENQUEUE_DELAY_MILLIS = TimeUnit.SECONDS.toMillis(10);
    private final MerchantTrustSignalsMediator mMediator;
    private final MerchantTrustMessageScheduler mMessageScheduler;

    /** Creates a new instance. */
    public MerchantTrustSignalsCoordinator(
            TabModelSelector tabModelSelector, MerchantTrustMessageScheduler messageScheduler) {
        mMediator = new MerchantTrustSignalsMediator(tabModelSelector, this::maybeDisplayMessage);
        mMessageScheduler = messageScheduler;
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
            mMessageScheduler.clear();
            mMessageScheduler.schedule(getMessagePropertyModel(replacementMessage.getHostName()),
                    replacementMessage, MerchantTrustMessageScheduler.MESSAGE_ENQUEUE_NO_DELAY);
        } else {
            mMessageScheduler.clear();
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
        return new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS).build();
    }
}
