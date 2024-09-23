// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.price_tracking;

import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.text.TextUtils;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.Log;
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
    private static final String TAG = "PriceTrackNotif";
    public static final String NOTIFICATION_TAG = "price_drop";

    static class NotificationData {
        public NotificationData(
                CharSequence title,
                CharSequence text,
                String iconUrl,
                String destinationUrl,
                String offerId,
                String productClusterId,
                List<ActionData> actions) {
            this.title = title;
            this.text = text;
            this.iconUrl = iconUrl;
            this.destinationUrl = destinationUrl;
            this.offerId = offerId;
            this.productClusterId = productClusterId;
            this.actions = actions;
        }

        /** Title of the notification. The displayable product name or title. */
        public final CharSequence title;

        /** Content text of the notification. Could be product description. */
        public final CharSequence text;

        /**
         * The URL of a representative image for the product, hosted on Google server. Used as both
         * large icon and the image in the expanded view.
         */
        public final String iconUrl;

        /** The URL that leads to the shopping item. */
        public final String destinationUrl;

        /** Associated offer ID. */
        public final String offerId;

        /** Associated cluster ID. */
        public final String productClusterId;

        /** A list of button actions. */
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
    private final Profile mProfile;
    private ImageFetcher mImageFetcher;
    private final NotificationManagerProxy mNotificationManagerProxy;
    private PriceDropNotificationManager mPriceDropNotificationManager;

    /**
     * Creates a {@link PriceDropNotifier} instance.
     *
     * @param context The Android context.
     * @param profile The {@link Profile} associated with price drop registration.
     */
    public static PriceDropNotifier create(Context context, Profile profile) {
        return new PriceDropNotifier(context, profile, new NotificationManagerProxyImpl(context));
    }

    @VisibleForTesting
    PriceDropNotifier(
            Context context, Profile profile, NotificationManagerProxy notificationManager) {
        mContext = context;
        mProfile = profile;
        mNotificationManagerProxy = notificationManager;
        mPriceDropNotificationManager =
                PriceDropNotificationManagerFactory.create(
                        mContext, mProfile, mNotificationManagerProxy);
    }

    /**
     * Shows a price drop notification.
     *
     * @param notificationData Information about the notification contents.
     */
    public void showNotification(final NotificationData notificationData) {
        maybeFetchIcon(
                notificationData,
                bitmap -> {
                    showWithIcon(notificationData, bitmap);
                });
    }

    @VisibleForTesting
    protected ImageFetcher getImageFetcher() {
        if (mImageFetcher == null) {
            mImageFetcher =
                    ImageFetcherFactory.createImageFetcher(
                            ImageFetcherConfig.NETWORK_ONLY, mProfile.getProfileKey());
        }
        return mImageFetcher;
    }

    @VisibleForTesting
    protected NotificationWrapperBuilder getNotificationBuilder(
            @SystemNotificationType int notificationType, int notificationId) {
        return NotificationWrapperBuilderFactory.createNotificationWrapperBuilder(
                ChannelId.PRICE_DROP_DEFAULT,
                new NotificationMetadata(notificationType, NOTIFICATION_TAG, notificationId));
    }

    private void maybeFetchIcon(
            final NotificationData notificationData, Callback<Bitmap> callback) {
        if (notificationData.iconUrl == null) {
            callback.onResult(null);
            return;
        }

        ImageFetcher.Params params =
                ImageFetcher.Params.create(
                        notificationData.iconUrl, ImageFetcher.PRICE_DROP_NOTIFICATION);
        getImageFetcher()
                .fetchImage(
                        params,
                        bitmap -> {
                            callback.onResult(bitmap);
                        });
    }

    private void showWithIcon(NotificationData notificationData, @Nullable Bitmap icon) {
        int notificationId = getNotificationId(notificationData.offerId);
        @SystemNotificationType int notificationType = getUmaNotificationType(notificationData);
        if (mPriceDropNotificationManager.hasReachedMaxAllowedNotificationNumber(
                notificationType)) {
            Log.e(
                    TAG,
                    "Unable to show this notification"
                            + " because we have reached the max allowed number.");
            return;
        }
        NotificationWrapperBuilder notificationBuilder =
                getNotificationBuilder(notificationType, notificationId);
        if (icon != null) {
            // Both the large icon and the expanded view use the bitmap fetched from icon URL.
            notificationBuilder.setLargeIcon(icon);
            notificationBuilder.setBigPictureStyle(icon, notificationData.text);
        }
        notificationBuilder.setContentTitle(notificationData.title);
        notificationBuilder.setContentText(notificationData.text);
        notificationBuilder.setContentIntent(
                createContentIntent(notificationData.destinationUrl, notificationId));
        notificationBuilder.setSmallIcon(R.drawable.ic_chrome);
        notificationBuilder.setTimeoutAfter(
                PriceTrackingNotificationConfig.getNotificationTimeoutMs());
        notificationBuilder.setAutoCancel(true);
        if (notificationData.actions != null) {
            for (ActionData action : notificationData.actions) {
                PendingIntentProvider actionClickIntentProvider =
                        createClickIntent(
                                action.actionId,
                                notificationData.destinationUrl,
                                notificationData.offerId,
                                notificationData.productClusterId,
                                notificationId);
                notificationBuilder.addAction(
                        0,
                        action.text,
                        actionClickIntentProvider,
                        actionIdToUmaActionType(action.actionId));
            }
        }
        NotificationWrapper notificationWrapper = notificationBuilder.buildNotificationWrapper();
        mNotificationManagerProxy.notify(notificationWrapper);
        NotificationUmaTracker.getInstance()
                .onNotificationShown(notificationType, notificationWrapper.getNotification());
        mPriceDropNotificationManager.updateNotificationTimestamps(notificationType, true);
    }

    private static @NotificationUmaTracker.ActionType int actionIdToUmaActionType(String actionId) {
        if (PriceDropNotificationManagerImpl.ACTION_ID_VISIT_SITE.equals(actionId)) {
            return ActionType.PRICE_DROP_VISIT_SITE;
        }
        if (PriceDropNotificationManagerImpl.ACTION_ID_TURN_OFF_ALERT.equals(actionId)) {
            return ActionType.PRICE_DROP_TURN_OFF_ALERT;
        }
        return ActionType.UNKNOWN;
    }

    private PendingIntentProvider createContentIntent(String destinationUrl, int notificationId) {
        Intent intent =
                mPriceDropNotificationManager.getNotificationClickIntent(
                        destinationUrl, notificationId);
        return PendingIntentProvider.getActivity(
                mContext, 0, intent, PendingIntent.FLAG_UPDATE_CURRENT);
    }

    private PendingIntentProvider createClickIntent(
            String actionId, String url, String offerId, String clusterId, int notificationId) {
        Intent intent =
                mPriceDropNotificationManager.getNotificationActionClickIntent(
                        actionId, url, offerId, clusterId, notificationId);
        return PendingIntentProvider.getActivity(
                mContext, 0, intent, PendingIntent.FLAG_UPDATE_CURRENT);
    }

    private int getNotificationId(String offerId) {
        assert !TextUtils.isEmpty(offerId);
        return offerId.hashCode();
    }

    private @SystemNotificationType int getUmaNotificationType(NotificationData notificationData) {
        return TextUtils.isEmpty(notificationData.productClusterId)
                ? SystemNotificationType.PRICE_DROP_ALERTS_CHROME_MANAGED
                : SystemNotificationType.PRICE_DROP_ALERTS_USER_MANAGED;
    }

    void setPriceDropNotificationManagerForTesting(PriceDropNotificationManager manager) {
        mPriceDropNotificationManager = manager;
    }
}
