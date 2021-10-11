// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.price_tracking;

import androidx.annotation.VisibleForTesting;

import com.google.protobuf.ByteString;
import com.google.protobuf.InvalidProtocolBufferException;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.browser.price_tracking.PriceDropNotifier.ActionData;
import org.chromium.chrome.browser.price_tracking.proto.Notifications;
import org.chromium.chrome.browser.price_tracking.proto.Notifications.ChromeMessage;
import org.chromium.chrome.browser.price_tracking.proto.Notifications.ChromeNotification;
import org.chromium.chrome.browser.price_tracking.proto.Notifications.ChromeNotification.NotificationDataType;
import org.chromium.chrome.browser.price_tracking.proto.Notifications.ExpandedView;
import org.chromium.chrome.browser.price_tracking.proto.Notifications.PriceDropNotificationPayload;

import java.util.ArrayList;
import java.util.List;

/**
 * Class to show a price tracking notification. The Java object is owned by the native side
 * PriceTrackingNotificationBridge object through JNI bridge.
 */
public class PriceTrackingNotificationBridge {
    private static final String TAG = "PriceTrackNotif";
    private final long mNativePriceTrackingNotificationBridge;
    private final PriceDropNotifier mNotifier;

    /**
     * Construct a {@link PriceTrackingNotificationBridge} object from native code.
     * @param nativePriceTrackingNotificationBridge The native JNI object pointer.
     */
    @VisibleForTesting
    PriceTrackingNotificationBridge(
            long nativePriceTrackingNotificationBridge, PriceDropNotifier notifier) {
        mNativePriceTrackingNotificationBridge = nativePriceTrackingNotificationBridge;
        mNotifier = notifier;
    }

    @CalledByNative
    private static PriceTrackingNotificationBridge create(
            long nativePriceTrackingNotificationBridge) {
        return new PriceTrackingNotificationBridge(nativePriceTrackingNotificationBridge,
                PriceDropNotifier.create(ContextUtils.getApplicationContext()));
    }

    @VisibleForTesting
    @CalledByNative
    void showNotification(byte[] payload) {
        ChromeNotification chromeNotification;
        try {
            chromeNotification = ChromeNotification.parseFrom(payload);
        } catch (InvalidProtocolBufferException e) {
            Log.e(TAG, "Failed to parse ChromeNotification payload.");
            return;
        }

        if (!chromeNotification.hasChromeMessage()) return;
        ChromeMessage chromeMessage = chromeNotification.getChromeMessage();
        if (!chromeMessage.hasDestinationUrl()) return;

        // TODO(xingliu): Use client strings for display text, title, and action text.
        // Show the notification.
        PriceDropNotifier.NotificationData notificationData =
                new PriceDropNotifier.NotificationData(chromeMessage.getDisplayTitle(),
                        chromeMessage.getDisplayText(),
                        chromeMessage.hasIconImageUrl() ? chromeMessage.getIconImageUrl() : null,
                        chromeMessage.getDestinationUrl(), parseOfferId(chromeNotification),
                        parseActions(chromeNotification));
        mNotifier.showNotification(notificationData);
    }

    private static String parseOfferId(ChromeNotification chromeNotification) {
        PriceDropNotificationPayload priceDropPayload = null;
        Long offerId = null;
        if (chromeNotification.hasNotificationData()
                && chromeNotification.hasNotificationDataType()) {
            if (chromeNotification.getNotificationDataType()
                    == NotificationDataType.PRICE_DROP_NOTIFICATION) {
                priceDropPayload = parsePriceDropPayload(chromeNotification.getNotificationData());
            }
        }
        if (priceDropPayload != null && priceDropPayload.hasOfferId()) {
            offerId = priceDropPayload.getOfferId();
        }
        // TODO(crbug.com/1257380): Figure out how to serialize offer id correctly.
        return offerId != null ? String.valueOf(offerId) : null;
    }

    private static PriceDropNotificationPayload parsePriceDropPayload(ByteString payload) {
        PriceDropNotificationPayload priceDropPayload = null;
        try {
            priceDropPayload = PriceDropNotificationPayload.parseFrom(payload);
        } catch (InvalidProtocolBufferException e) {
            Log.e(TAG, "Failed to parse PriceDropNotificationPayload.");
        }
        return priceDropPayload;
    }

    private static List<PriceDropNotifier.ActionData> parseActions(
            ChromeNotification chromeNotification) {
        List<PriceDropNotifier.ActionData> actions = new ArrayList<>();
        if (!chromeNotification.hasChromeMessage()) return actions;
        ChromeMessage chromeMessage = chromeNotification.getChromeMessage();
        if (!chromeMessage.hasExpandedView()) return actions;
        ExpandedView expandedView = chromeMessage.getExpandedView();
        for (Notifications.Action action : expandedView.getActionList()) {
            if (!action.hasActionId() || !action.hasText()) continue;
            actions.add(new ActionData(action.getActionId(), action.getText()));
        }
        return actions;
    }
}
