// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.merchant_viewer;

import static org.chromium.build.NullUtil.assertNonNull;

import android.os.Handler;
import android.util.Pair;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.merchant_viewer.MerchantTrustMetrics.MessageClearReason;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.messages.DismissReason;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageScopeType;
import org.chromium.ui.modelutil.PropertyModel;

/** Abstracts the logic needed to schedule a message using {@link MessageDispatcher} framework. */
@SuppressWarnings("SynchronizeOnNonFinalField") // Non-final in tests.
@NullMarked
public class MerchantTrustMessageScheduler {
    public static final long MESSAGE_ENQUEUE_NO_DELAY = 0;

    private final MessageDispatcher mMessageDispatcher;
    private final MerchantTrustMetrics mMetrics;
    private final ObservableSupplier<@Nullable Tab> mTabSupplier;
    private Handler mEnqueueMessageTimer;
    private @Nullable Pair<MerchantTrustMessageContext, PropertyModel> mScheduledMessage;

    public MerchantTrustMessageScheduler(
            MessageDispatcher messageDispatcher,
            MerchantTrustMetrics metrics,
            ObservableSupplier<@Nullable Tab> tabSupplier) {
        mEnqueueMessageTimer = new Handler(ThreadUtils.getUiThreadLooper());
        mMessageDispatcher = messageDispatcher;
        mMetrics = metrics;
        mTabSupplier = tabSupplier;
    }

    /** Cancels any scheduled messages. */
    void clear(@MessageClearReason int clearReason) {
        mEnqueueMessageTimer.removeCallbacksAndMessages(null);
        if (mScheduledMessage != null && mScheduledMessage.second != null) {
            mMessageDispatcher.dismissMessage(
                    mScheduledMessage.second, DismissReason.SCOPE_DESTROYED);
        }
        clearScheduledMessage(clearReason);
    }

    // TODO(crbug.com/40215605): Clean up this api in tests.
    @Deprecated
    void schedule(
            PropertyModel model,
            MerchantTrustMessageContext messageContext,
            long delayInMillis,
            Callback<@Nullable MerchantTrustMessageContext> messageEnqueuedCallback) {
        schedule(model, 4.0, messageContext, delayInMillis, messageEnqueuedCallback);
    }

    /** Adds a message to the underlying {@link MessageDispatcher} queue. */
    void schedule(
            PropertyModel model,
            double starRating,
            MerchantTrustMessageContext messageContext,
            long delayInMillis,
            Callback<@Nullable MerchantTrustMessageContext> messageEnqueuedCallback) {
        setScheduledMessage(new Pair<>(messageContext, model));
        mMetrics.recordMetricsForMessagePrepared();
        mEnqueueMessageTimer.postDelayed(
                () -> {
                    boolean hasMessageContext = messageContext.isValid();
                    Tab tab = mTabSupplier.get();
                    boolean sameWebContents =
                            hasMessageContext
                                    && tab != null
                                    && messageContext.getWebContents() == tab.getWebContents();
                    if (sameWebContents) {
                        mMetrics.startRecordingMessageImpact(
                                messageContext.getHostName(), starRating);
                        if (MerchantViewerConfig.isTrustSignalsMessageDisabledForImpactStudy()) {
                            messageEnqueuedCallback.onResult(messageContext);
                            // TODO(crbug.com/40215605): Use a new message clear reason.
                            clearScheduledMessage(MessageClearReason.UNKNOWN);
                        } else {
                            mMessageDispatcher.enqueueMessage(
                                    model,
                                    assertNonNull(messageContext.getWebContents()),
                                    MessageScopeType.NAVIGATION,
                                    false);
                            mMetrics.recordMetricsForMessageShown();
                            messageEnqueuedCallback.onResult(messageContext);
                            setScheduledMessage(null);
                        }
                    } else {
                        messageEnqueuedCallback.onResult(null);
                        clearScheduledMessage(
                                !hasMessageContext
                                        ? MessageClearReason.MESSAGE_CONTEXT_NO_LONGER_VALID
                                        : tab != null
                                                ? MessageClearReason.SWITCH_TO_DIFFERENT_WEBCONTENTS
                                                : MessageClearReason.UNKNOWN);
                    }
                },
                delayInMillis);
    }

    /** Returns the currently scheduled message. */
    @Nullable MerchantTrustMessageContext getScheduledMessageContext() {
        return mScheduledMessage == null ? null : mScheduledMessage.first;
    }

    private void clearScheduledMessage(@MessageClearReason int clearReason) {
        if (mScheduledMessage != null) {
            mMetrics.recordMetricsForMessageCleared(clearReason);
        }
        setScheduledMessage(null);
    }

    void setHandlerForTesting(Handler handler) {
        mEnqueueMessageTimer = handler;
    }

    @VisibleForTesting
    void setScheduledMessage(@Nullable Pair<MerchantTrustMessageContext, PropertyModel> pair) {
        synchronized (mEnqueueMessageTimer) {
            mScheduledMessage = pair;
        }
    }
}
