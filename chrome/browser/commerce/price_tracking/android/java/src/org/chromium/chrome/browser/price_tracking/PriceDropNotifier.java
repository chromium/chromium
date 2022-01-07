// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.price_tracking;

import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker.ActionType;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker.SystemNotificationType;
import org.chromium.chrome.browser.notifications.NotificationWrapperBuilderFactory;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions.ChannelId;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxyImpl;
import org.chromium.components.browser_ui.notifications.NotificationMetadata;
import org.chromium.components.browser_ui.notifications.NotificationWrapper;
import org.chromium.components.browser_ui.notifications.NotificationWrapperBuilder;
import org.chromium.components.browser_ui.notifications.PendingIntentProvider;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.components.image_fetcher.ImageFetcherConfig;
import org.chromium.components.image_fetcher.ImageFetcherFactory;

import java.util.List;

/**
 * Used to display price drop notification with {@link NotificationWrapperBuilder} and {@link
 * ImageFetcher} without cache.
 */
public class PriceDropNotifier {
    private static final String NOTIFICATION_TAG = "price_drop";
    private static final int NOTIFICATION_ID = 0;

    static class NotificationData {
        public NotificationData(CharSequence title, CharSequence text, String iconUrl,
                String destinationUrl, String offerId, List<ActionData> actions) {
            this.title = title;
            this.text = text;
            this.iconUrl = iconUrl;
            this.destinationUrl = destinationUrl;
            this.offerId = offerId;
            this.actions = actions;
        }

        /**
         * Title of the notification. The displayable product name or title.
         */
        public final CharSequence title;
        /**
         * Content text of the notification. Could be product description.
         */
        public final CharSequence text;

        /**
         * The URL of a representative image for the product, hosted on Google server. Used as both
         * large icon and the image in the expanded view.
         */
        public final String iconUrl;
        /**
         * The URL that leads to the shopping item.
         */
        public final String destinationUrl;
        /**
         * Associated offer ID.
         */
        public final String offerId;
        /**
         * A list of button actions.
         */
        public final List<ActionData> actions;
    }

    static class ActionData {
        ActionData(String actionId, String text) {
            this.actionId = actionId;
            this.text = text;
        }
        public final String actionId;
        public final CharSequence text;
    }

    private final Context mContext;
    private ImageFetcher mImageFetcher;
    private final NotificationBuilderFactory mNotificationBuilderFactory;
    private final NotificationManagerProxy mNotificationManagerProxy;
    private final PriceDropNotificationManager mPriceDropNotificationManager;

    /**
     * Creates a {@link PriceDropNotifier} instance.
     * @param context The Android context.
     */
    public static PriceDropNotifier create(Context context) {
        NotificationBuilderFactory notificationBuilderFactory = ()
                -> NotificationWrapperBuilderFactory.createNotificationWrapperBuilder(
                        ChannelId.PRICE_DROP,
                        new NotificationMetadata(SystemNotificationType.PRICE_DROP_ALERTS,
                                NOTIFICATION_TAG, NOTIFICATION_ID));
        return new PriceDropNotifier(
                context, notificationBuilderFactory, new NotificationManagerProxyImpl(context));
    }

    /**
     * Factory interface to create {@link NotificationWrapperBuilder}.
     */
    interface NotificationBuilderFactory {
        NotificationWrapperBuilder createNotificationBuilder();
    }

    @VisibleForTesting
    PriceDropNotifier(Context context, NotificationBuilderFactory notificationBuilderFactory,
            NotificationManagerProxy notificationManager) {
        mContext = context;
        mNotificationBuilderFactory = notificationBuilderFactory;
        mNotificationManagerProxy = notificationManager;
        mPriceDropNotificationManager =
                new PriceDropNotificationManager(mContext, mNotificationManagerProxy);
    }

    /**
     * Shows a price drop notification.
     * @param notificationData Information about the notification contents.
     */
    public void showNotification(final NotificationData notificationData) {
        maybeFetchIcon(notificationData, bitmap -> { showWithIcon(notificationData, bitmap); });
    }

    @VisibleForTesting
    protected ImageFetcher getImageFetcher() {
        if (mImageFetcher == null) {
            mImageFetcher = ImageFetcherFactory.createImageFetcher(ImageFetcherConfig.NETWORK_ONLY,
                    Profile.getLastUsedRegularProfile().getProfileKey());
        }
        return mImageFetcher;
    }

    private void maybeFetchIcon(
            final NotificationData notificationData, Callback<Bitmap> callback) {
        if (notificationData.iconUrl == null) {
            callback.onResult(null);
            return;
        }

        ImageFetcher.Params params = ImageFetcher.Params.create(
                notificationData.iconUrl, ImageFetcher.PRICE_DROP_NOTIFICATION);
        getImageFetcher().fetchImage(params, bitmap -> { callback.onResult(bitmap); });
    }

    private void showWithIcon(NotificationData notificationData, @Nullable Bitmap icon) {
        NotificationWrapperBuilder notificationBuilder =
                mNotificationBuilderFactory.createNotificationBuilder();
        if (icon != null) {
            // Both the large icon and the expanded view use the bitmap fetched from icon URL.
            notificationBuilder.setLargeIcon(icon);
            notificationBuilder.setBigPictureStyle(icon, notificationData.text);
        }
        notificationBuilder.setContentTitle(notificationData.title);
        notificationBuilder.setContentText(notificationData.text);
        notificationBuilder.setContentIntent(createContentIntent(notificationData.destinationUrl));
        notificationBuilder.setSmallIcon(R.drawable.ic_chrome);
        notificationBuilder.setTimeoutAfter(
                PriceTrackingNotificationConfig.getNotificationTimeoutMs());
        if (notificationData.actions != null) {
            for (ActionData action : notificationData.actions) {
                PendingIntentProvider actionClickIntentProvider = createClickIntent(
                        action.actionId, notificationData.destinationUrl, notificationData.offerId);
                notificationBuilder.addAction(0, action.text, actionClickIntentProvider,
                        actionIdToUmaActionType(action.actionId));
            }
        }
        NotificationWrapper notificationWrapper = notificationBuilder.buildNotificationWrapper();
        mNotificationManagerProxy.notify(notificationWrapper);
        mPriceDropNotificationManager.onNotificationPosted(notificationWrapper.getNotification());
    }

    private static @NotificationUmaTracker.ActionType int actionIdToUmaActionType(String actionId) {
        if (PriceDropNotificationManager.ACTION_ID_VISIT_SITE.equals(actionId)) {
            return ActionType.PRICE_DROP_VISIT_SITE;
        }
        if (PriceDropNotificationManager.ACTION_ID_TURN_OFF_ALERT.equals(actionId)) {
            return ActionType.PRICE_DROP_TURN_OFF_ALERT;
        }
        return ActionType.UNKNOWN;
    }

    private PendingIntentProvider createContentIntent(String destinationUrl) {
        Intent intent = mPriceDropNotificationManager.getNotificationClickIntent(destinationUrl);
        return PendingIntentProvider.getActivity(
                mContext, 0, intent, PendingIntent.FLAG_UPDATE_CURRENT);
    }

    private PendingIntentProvider createClickIntent(String actionId, String url, String offerId) {
        Intent intent = mPriceDropNotificationManager.getNotificationActionClickIntent(
                actionId, url, offerId);
        return PendingIntentProvider.getActivity(
                mContext, 0, intent, PendingIntent.FLAG_UPDATE_CURRENT);
    }
}
