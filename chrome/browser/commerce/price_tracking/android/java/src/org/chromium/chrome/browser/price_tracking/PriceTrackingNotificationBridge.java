// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.price_tracking;

import android.content.Context;
import android.net.Uri;
import android.text.TextUtils;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import com.google.common.primitives.UnsignedLongs;
import com.google.protobuf.ByteString;
import com.google.protobuf.InvalidProtocolBufferException;

import org.jni_zero.CalledByNative;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.commerce.PriceUtils;
import org.chromium.chrome.browser.price_tracking.PriceDropNotifier.ActionData;
import org.chromium.chrome.browser.price_tracking.proto.Notifications;
import org.chromium.chrome.browser.price_tracking.proto.Notifications.ChromeMessage;
import org.chromium.chrome.browser.price_tracking.proto.Notifications.ChromeNotification;
import org.chromium.chrome.browser.price_tracking.proto.Notifications.ChromeNotification.NotificationDataType;
import org.chromium.chrome.browser.price_tracking.proto.Notifications.ExpandedView;
import org.chromium.chrome.browser.price_tracking.proto.Notifications.PriceDropNotificationPayload;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.commerce.PriceTracking.ProductPrice;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.optimization_guide.proto.CommonTypesProto.Any;

import java.util.ArrayList;
import java.util.List;

/**
 * Class to show a price tracking notification. The Java object is owned by the native side
 * PriceTrackingNotificationBridge object through JNI bridge.
 */
public class PriceTrackingNotificationBridge {
    private static final String TAG = "PriceTrackNotif";
    private final PriceDropNotifier mNotifier;
    private final PriceDropNotificationManager mPriceDropNotificationManager;

    /**
     * Construct a {@link PriceTrackingNotificationBridge} object from native code.
     *
     * @param nativePriceTrackingNotificationBridge The native JNI object pointer.
     * @param notifier {@link PriceDropNotifier} used to create the actual notification in tray.
     * @param notificationManager {@link PriceDropNotificationManager} used to check price drop
     *     notification channel.
     */
    @VisibleForTesting
    PriceTrackingNotificationBridge(
            long nativePriceTrackingNotificationBridge,
            PriceDropNotifier notifier,
            PriceDropNotificationManager notificationManager) {
        mNotifier = notifier;
        mPriceDropNotificationManager = notificationManager;
    }

    @CalledByNative
    private static PriceTrackingNotificationBridge create(
            long nativePriceTrackingNotificationBridge, Profile profile) {
        return new PriceTrackingNotificationBridge(
                nativePriceTrackingNotificationBridge,
                PriceDropNotifier.create(ContextUtils.getApplicationContext(), profile),
                PriceDropNotificationManagerFactory.create(profile));
    }

    @VisibleForTesting
    @CalledByNative
    void showNotification(byte[] payload) {
        // Price drop notification channel is created after the alert card UI is shown. If that
        // didn't happen, don't show the notification.
        if (!mPriceDropNotificationManager.canPostNotification()) return;

        ChromeNotification chromeNotification = parseAndValidateChromeNotification(payload);
        if (chromeNotification == null) {
            Log.e(TAG, "Invalid ChromeNotification proto.");
            return;
        }

        // Parse the PriceDropNotificationPayload.
        PriceDropNotificationPayload priceDropPayload =
                parseAndValidatePriceDropNotificationPayload(
                        chromeNotification.getNotificationData());
        if (priceDropPayload == null) {
            Log.e(TAG, "Invalid PriceDropNotificationPayload proto.");
            return;
        }

        // Show the notification. Uses client side strings for now, which should match
        // HandleProductUpdateEventsProducerModule.java in google3.
        String priceDrop = getPriceDropAmount(priceDropPayload);
        if (TextUtils.isEmpty(priceDrop)) {
            Log.e(TAG, "Invalid price drop amount.");
            return;
        }

        Context context = ContextUtils.getApplicationContext();
        String title =
                context.getString(
                        R.string.price_drop_popup_content_title,
                        priceDrop,
                        priceDropPayload.getProductName());

        Uri productUrl = Uri.parse(priceDropPayload.getDestinationUrl());
        if (productUrl.getHost() == null) {
            Log.e(TAG, "Failed to parse destination URL host.");
            return;
        }
        String text =
                context.getString(
                        R.string.price_drop_popup_content_text,
                        buildDisplayPrice(priceDropPayload.getCurrentPrice()),
                        productUrl.getHost());

        // Use UnsignedLongs to convert OfferId to avoid overflow.
        String offerId = UnsignedLongs.toString(priceDropPayload.getOfferId());
        String clusterId = null;
        if (priceDropPayload.hasProductClusterId() && priceDropPayload.getProductClusterId() != 0) {
            clusterId = UnsignedLongs.toString(priceDropPayload.getProductClusterId());
        }
        ChromeMessage chromeMessage = chromeNotification.getChromeMessage();
        PriceDropNotifier.NotificationData notificationData =
                new PriceDropNotifier.NotificationData(
                        title,
                        text,
                        chromeMessage.hasIconImageUrl() ? chromeMessage.getIconImageUrl() : null,
                        priceDropPayload.getDestinationUrl(),
                        offerId,
                        clusterId,
                        parseActions(chromeNotification));
        mNotifier.showNotification(notificationData);
    }

    private static ChromeNotification parseAndValidateChromeNotification(byte[] payload) {
        ChromeNotification chromeNotification;
        try {
            chromeNotification = ChromeNotification.parseFrom(payload);
        } catch (InvalidProtocolBufferException e) {
            Log.e(TAG, "Failed to parse ChromeNotification payload.");
            return null;
        }

        // Must have ChromeMessage proto and notification_data field, which is
        // PriceDropNotificationPayload.
        if (!chromeNotification.hasChromeMessage() || !chromeNotification.hasNotificationData()) {
            return null;
        }

        // Must have the correct type.
        if (!chromeNotification.hasNotificationDataType()
                || chromeNotification.getNotificationDataType()
                        != NotificationDataType.PRICE_DROP_NOTIFICATION) {
            return null;
        }
        return chromeNotification;
    }

    private static PriceDropNotificationPayload parseAndValidatePriceDropNotificationPayload(
            ByteString payload) {
        // notification_data field is an any.proto.
        Any any = null;
        try {
            any = Any.parseFrom(payload);
        } catch (InvalidProtocolBufferException e) {
            Log.e(TAG, "Failed to parse to Any.");
            return null;
        }

        if (any == null) return null;

        PriceDropNotificationPayload priceDropPayload = null;
        try {
            priceDropPayload = PriceDropNotificationPayload.parseFrom(any.getValue());
        } catch (InvalidProtocolBufferException e) {
            Log.e(TAG, "Failed to parse PriceDropNotificationPayload.");
            return null;
        }
        if (priceDropPayload == null) return null;

        // Current price must be smaller than previous price, or it's not a price drop.
        if (!priceDropPayload.hasCurrentPrice()
                || !priceDropPayload.hasPreviousPrice()
                || (priceDropPayload.getCurrentPrice().getAmountMicros()
                        >= priceDropPayload.getPreviousPrice().getAmountMicros())) {
            return null;
        }

        // Must have valid destination URL to ensure clicking to function.
        if (!priceDropPayload.hasDestinationUrl()
                || TextUtils.isEmpty(priceDropPayload.getDestinationUrl())
                || !UrlUtilities.isHttpOrHttps(priceDropPayload.getDestinationUrl())) {
            return null;
        }

        // Must have the offer id to ensure the subscription to function.
        if (!priceDropPayload.hasOfferId() || priceDropPayload.getOfferId() == 0) return null;

        // Must have the product name to show in the title.
        if (!priceDropPayload.hasProductName()
                || TextUtils.isEmpty(priceDropPayload.getProductName())) {
            return null;
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
            String actionText = getActionText(action.getActionId());
            if (TextUtils.isEmpty(actionText)) continue;
            actions.add(new ActionData(action.getActionId(), actionText));
        }
        return actions;
    }

    private static @Nullable String getActionText(String actionId) {
        if (TextUtils.isEmpty(actionId)) return null;
        Context context = ContextUtils.getApplicationContext();
        if (PriceDropNotificationManagerImpl.ACTION_ID_VISIT_SITE.equals(actionId)) {
            return context.getString(R.string.price_drop_popup_action_button);
        } else if (PriceDropNotificationManagerImpl.ACTION_ID_TURN_OFF_ALERT.equals(actionId)) {
            return context.getString(R.string.price_drop_popup_untrack_button);
        }
        return null;
    }

    private static String getPriceDropAmount(PriceDropNotificationPayload priceDropPayload) {
        long dropAmount =
                priceDropPayload.getPreviousPrice().getAmountMicros()
                        - priceDropPayload.getCurrentPrice().getAmountMicros();
        assert dropAmount > 0;
        return buildDisplayPrice(
                ProductPrice.newBuilder()
                        .setAmountMicros(dropAmount)
                        .setCurrencyCode(priceDropPayload.getCurrentPrice().getCurrencyCode())
                        .build());
    }

    private static String buildDisplayPrice(ProductPrice productPrice) {
        return PriceUtils.formatPrice(
                productPrice.getCurrencyCode(), productPrice.getAmountMicros());
    }
}
