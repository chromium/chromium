// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.merchant_viewer;

import android.os.Handler;
import android.util.Pair;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.merchant_viewer.MerchantTrustMetrics.MessageClearReason;
import org.chromium.components.messages.DismissReason;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageScopeType;
import org.chromium.ui.modelutil.PropertyModel;

/** Abstracts the logic needed to schedule a message using {@link MessageDispatcher} framework. */
public class MerchantTrustMessageScheduler {
    public static final long MESSAGE_ENQUEUE_NO_DELAY = 0;

    private final MessageDispatcher mMessageDispatcher;
    private final MerchantTrustMetrics mMetrics;
    private Handler mEnqueueMessageTimer;
    private Pair<MerchantTrustMessageContext, PropertyModel> mScheduledMessage;

    public MerchantTrustMessageScheduler(
            MessageDispatcher messageDispatcher, MerchantTrustMetrics metrics) {
        mEnqueueMessageTimer = new Handler(ThreadUtils.getUiThreadLooper());
        mMessageDispatcher = messageDispatcher;
        mMetrics = metrics;
    }

    /** Cancels any scheduled messages. */
    void clear(@MessageClearReason int clearReason) {
        if (mScheduledMessage != null) {
            mMetrics.recordMetricsForMessageCleared(clearReason);
        }
        mEnqueueMessageTimer.removeCallbacksAndMessages(null);
        if (mScheduledMessage != null && mScheduledMessage.second != null) {
            mMessageDispatcher.dismissMessage(
                    mScheduledMessage.second, DismissReason.SCOPE_DESTROYED);
        }
        setScheduledMessage(null);
    }

    /** Adds a message to the underlying {@link MessageDispatcher} queue. */
    void schedule(PropertyModel model, MerchantTrustMessageContext messageContext,
            long delayInMillis, Callback<MerchantTrustMessageContext> messageEnqueuedCallback) {
        setScheduledMessage(
                new Pair<MerchantTrustMessageContext, PropertyModel>(messageContext, model));
        mMetrics.recordMetricsForMessagePrepared();
        mEnqueueMessageTimer.postDelayed(() -> {
            if (messageContext.isValid()) {
                mMessageDispatcher.enqueueMessage(
                        model, messageContext.getWebContents(), MessageScopeType.NAVIGATION);
                mMetrics.recordMetricsForMessageShown();
                messageEnqueuedCallback.onResult(messageContext);
            } else {
                messageEnqueuedCallback.onResult(null);
            }
            setScheduledMessage(null);
        }, delayInMillis);
    }

    /**
     * Forces the currently scheduled message (if any) to be enqueued through the {@link
     * MessageDispatcher} right away without having to wait for the original time. This is achieved
     * by calling MerchantTrustMessageScheduler#schedule with no delay time. This is a NOP if there
     * isn't a scheduled message.
     */
    void expedite(Callback<MerchantTrustMessageContext> callback) {
        if (mScheduledMessage == null) {
            callback.onResult(null);
            return;
        }

        Pair<MerchantTrustMessageContext, PropertyModel> replacement =
                new Pair<MerchantTrustMessageContext, PropertyModel>(
                        mScheduledMessage.first, mScheduledMessage.second);
        clear(MessageClearReason.NAVIGATE_TO_SAME_DOMAIN);
        schedule(replacement.second, replacement.first, MESSAGE_ENQUEUE_NO_DELAY, callback);
    }

    /** Returns the currently scheduled message. */
    MerchantTrustMessageContext getScheduledMessageContext() {
        return mScheduledMessage == null ? null : mScheduledMessage.first;
    }

    @VisibleForTesting
    void setHandlerForTesting(Handler handler) {
        mEnqueueMessageTimer = handler;
    }

    @VisibleForTesting
    void setScheduledMessage(Pair<MerchantTrustMessageContext, PropertyModel> pair) {
        synchronized (mEnqueueMessageTimer) {
            mScheduledMessage = pair;
        }
    }
}
