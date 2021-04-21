// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.merchant_viewer;

import android.os.Handler;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ThreadUtils;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageScopeType;
import org.chromium.ui.modelutil.PropertyModel;

/** Abstracts the logic needed to schedule a message using {@link MessageDispatcher} framework. */
public class MerchantTrustMessageScheduler {
    public static final long MESSAGE_ENQUEUE_NO_DELAY = 0;

    private final MessageDispatcher mMessageDispatcher;
    private Handler mEnqueueMessageTimer;
    private MerchantTrustMessageContext mScheduledMessageContext;

    public MerchantTrustMessageScheduler(MessageDispatcher messageDispatcher) {
        mEnqueueMessageTimer = new Handler(ThreadUtils.getUiThreadLooper());
        mMessageDispatcher = messageDispatcher;
    }

    /** Cancels any scheduled messages. */
    void clear() {
        mEnqueueMessageTimer.removeCallbacksAndMessages(null);
        setScheduledMessageContext(null);
    }

    /** Adds a message to the underlying {@link MessageDispatcher} queue. */
    void schedule(
            PropertyModel model, MerchantTrustMessageContext messageContext, long delayInMillis) {
        setScheduledMessageContext(messageContext);
        mEnqueueMessageTimer.postDelayed(() -> {
            if (messageContext.isValid()) {
                mMessageDispatcher.enqueueMessage(
                        model, messageContext.getWebContents(), MessageScopeType.NAVIGATION);
            }
            setScheduledMessageContext(null);
        }, delayInMillis);
    }

    /** Returns the currently scheduled message. */
    MerchantTrustMessageContext getScheduledMessageContext() {
        return mScheduledMessageContext;
    }

    @VisibleForTesting
    void setHandlerForTesting(Handler handler) {
        mEnqueueMessageTimer = handler;
    }

    private void setScheduledMessageContext(MerchantTrustMessageContext messageContext) {
        synchronized (mEnqueueMessageTimer) {
            mScheduledMessageContext = messageContext;
        }
    }
}
