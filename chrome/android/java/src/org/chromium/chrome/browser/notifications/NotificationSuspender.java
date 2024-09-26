// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import android.app.Notification;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Icon;
import android.net.Uri;
import android.os.Build;
import android.text.TextUtils;

import androidx.annotation.RequiresApi;

import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxyFactory;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.NotificationMetadata;
import org.chromium.components.browser_ui.notifications.NotificationWrapper;
import org.chromium.components.embedder_support.util.UrlConstants;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.stream.Collectors;

/**
 * Class that suspends and revives notifications.
 *
 * All calls must be made on the UI thread, and the full browser must be started before using this
 * class.
 */
public class NotificationSuspender {
    private final Profile mProfile;
    private final Context mContext;
    private final BaseNotificationManagerProxy mNotificationManager;

    public NotificationSuspender(Profile profile) {
        this(
                profile,
                ContextUtils.getApplicationContext(),
                BaseNotificationManagerProxyFactory.create(ContextUtils.getApplicationContext()));
    }

    public NotificationSuspender(
            Profile profile, Context context, BaseNotificationManagerProxy notificationManager) {
        mProfile = profile;
        mContext = context;
        mNotificationManager = notificationManager;
    }

    /**
     * Suspends notifications from the given domains.
     *
     * <p>Suspending means storing the notification resources and canceling the Android
     * notifications themselves, so that they can be re-displayed later.
     *
     * @param fqdns The list of domain strings to suspend notifications from.
     */
    public void suspendNotificationsFromDomains(List<String> fqdns) {
        getActiveNotificationsForOrigins(
                getOriginsForDomains(fqdns),
                (activeNotifications) -> {
                    cancelNotificationsWithIds(storeNotificationResources(activeNotifications));
                });
    }

    /**
     * Stores resources for all notifications from a given origin back into the native
     * NotificationDatabase.
     *
     * <p>This allows re-displaying these notification later.
     *
     * @param callback The origins for which all notification resources to store.
     */
    public void storeNotificationResourcesFromOrigins(
            List<Uri> origins, Callback<List<String>> callback) {
        getActiveNotificationsForOrigins(
                origins,
                (activeNotifications) ->
                        callback.onResult(storeNotificationResources(activeNotifications)));
    }

    /**
     * Stores resources for the given notifications back into the native NotificationDatabase.
     *
     * @param notifications The list of notifications whose resources to store.
     */
    public List<String> storeNotificationResources(List<NotificationWrapper> notifications) {
        if (notifications.isEmpty()) {
            return new ArrayList<String>();
        }

        String[] notificationIds = new String[notifications.size()];
        String[] origins = new String[notifications.size()];
        Bitmap[] resources = new Bitmap[notifications.size() * 3];

        for (int i = 0; i < notifications.size(); ++i) {
            Notification notification = notifications.get(i).getNotification();
            String tag = notifications.get(i).getMetadata().tag;
            // Chromium's `notificationId` is used as the Android notification's tag.
            notificationIds[i] = tag;
            origins[i] = NotificationPlatformBridge.getOriginFromNotificationTag(tag);
            resources[i * 3 + 0] = getNotificationIcon(notification);
            resources[i * 3 + 1] = getNotificationBadge(notification);
            resources[i * 3 + 2] = getNotificationImage(notification);
        }

        NotificationSuspenderJni.get()
                .storeNotificationResources(mProfile, notificationIds, origins, resources);
        return new ArrayList<String>(Arrays.asList(notificationIds));
    }

    /**
     * Unsuspends notifications from the given domains.
     *
     * This means re-displaying the notification using the prevously stored resources.
     *
     * @param fqdns The list of domain strings to unsuspend notifications from.
     */
    public void unsuspendNotificationsFromDomains(List<String> fqdns) {
        unsuspendNotificationsFromOrigins(getOriginsForDomains(fqdns));
    }

    /** Unsuspends notifications from the given origins. */
    public void unsuspendNotificationsFromOrigins(List<Uri> origins) {
        if (origins.isEmpty()) {
            return;
        }

        NotificationSuspenderJni.get()
                .reDisplayNotifications(
                        mProfile,
                        origins.stream()
                                .map((origin) -> origin.toString())
                                .collect(Collectors.toList()));
    }

    /**
     * Retrieves the fully-qualified domain name of the website that displayed a notification.
     *
     * @param notification The notification whose originating domain to return.
     */
    public static String getValidFqdnOrEmptyString(NotificationWrapper notification) {
        String tag = notification.getMetadata().tag;
        String origin = NotificationPlatformBridge.getOriginFromNotificationTag(tag);
        if (TextUtils.isEmpty(origin)) return "";
        String host = Uri.parse(origin).getHost();
        return host == null ? "" : host;
    }

    /**
     * Cancels the notifications with the given notification IDs.
     *
     * @param notificationIds The IDs of notifications to cancel.
     */
    public void cancelNotificationsWithIds(List<String> notificationIds) {
        for (String notificationId : notificationIds) {
            mNotificationManager.cancel(
                    /* tag= */ notificationId, NotificationPlatformBridge.PLATFORM_ID);
        }
    }

    private List<Uri> getOriginsForDomains(List<String> fqdns) {
        final String[] notificationSchemes = {UrlConstants.HTTPS_SCHEME, UrlConstants.HTTP_SCHEME};
        ArrayList<Uri> origins = new ArrayList<Uri>();
        for (String fqdn : fqdns) {
            for (String scheme : notificationSchemes) {
                origins.add(new Uri.Builder().scheme(scheme).authority(fqdn).build());
            }
        }
        return origins;
    }

    public void getActiveNotificationsForOrigins(
            List<Uri> origins, Callback<List<NotificationWrapper>> callback) {
        List<NotificationWrapper> notifications = new ArrayList<>();

        if (origins.isEmpty()) {
            callback.onResult(notifications);
            return;
        }

        mNotificationManager.getActiveNotifications(
                (activeNotifications) -> {
                    for (NotificationManagerProxy.StatusBarNotificationProxy notification :
                            activeNotifications) {
                        if (notification.getId() != NotificationPlatformBridge.PLATFORM_ID) {
                            continue;
                        }
                        String tag = notification.getTag();
                        String origin =
                                NotificationPlatformBridge.getOriginFromNotificationTag(tag);
                        if (origin == null || !origins.contains(Uri.parse(origin))) continue;
                        NotificationMetadata metadata =
                                new NotificationMetadata(
                                        NotificationUmaTracker.SystemNotificationType.SITES,
                                        tag,
                                        NotificationPlatformBridge.PLATFORM_ID);
                        notifications.add(
                                new NotificationWrapper(notification.getNotification(), metadata));
                    }
                    callback.onResult(notifications);
                });
    }

    @RequiresApi(Build.VERSION_CODES.P)
    private Bitmap getBitmapFromIcon(Icon icon) {
        if (icon == null || icon.getType() != Icon.TYPE_BITMAP) return null;
        return ((BitmapDrawable) icon.loadDrawable(mContext)).getBitmap();
    }

    private Bitmap getNotificationIcon(Notification notification) {
        return getBitmapFromIcon(notification.getLargeIcon());
    }

    private Bitmap getNotificationBadge(Notification notification) {
        return getBitmapFromIcon(notification.getSmallIcon());
    }

    private Bitmap getNotificationImage(Notification notification) {
        return (Bitmap) notification.extras.get(Notification.EXTRA_PICTURE);
    }

    @NativeMethods
    interface Natives {
        // Stores the given |resources| to be displayed later again. Note that |resources| is
        // expected to have 3 entries (icon, badge, image in that order) for each notification id in
        // |notificationIds|. If a notification does not have a particular resource, pass null
        // instead. |origins| must be the same size as |notificationIds|.
        void storeNotificationResources(
                @JniType("Profile*") Profile profile,
                String[] notificationIds,
                String[] origins,
                Bitmap[] resources);

        // Displays all suspended notifications for the given |origins|.
        void reDisplayNotifications(
                @JniType("Profile*") Profile profile,
                @JniType("std::vector<std::string>") List<String> origins);
    }
}
