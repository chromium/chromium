// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.usage_stats;

import android.annotation.TargetApi;
import android.app.Notification;
import android.app.NotificationManager;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Icon;
import android.net.Uri;
import android.os.Build;
import android.service.notification.StatusBarNotification;
import android.text.TextUtils;
import android.webkit.URLUtil;

import org.chromium.base.CollectionUtil;
import org.chromium.base.ContextUtils;
import org.chromium.base.Promise;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.notifications.ChromeNotification;
import org.chromium.chrome.browser.notifications.NotificationMetadata;
import org.chromium.chrome.browser.notifications.NotificationPlatformBridge;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.content_public.browser.BrowserStartupController;
import org.chromium.content_public.browser.BrowserStartupController.StartupCallback;

import java.util.ArrayList;
import java.util.List;

/**
 * Class that suspends and revives notifications for suspended websites. All calls must be made on
 * the UI thread.
 */
@JNINamespace("usage_stats")
public class NotificationSuspender {
    private final Profile mProfile;
    private final Context mContext;
    private final NotificationManager mNotificationManager;

    private static boolean isEnabled() {
        return UsageStatsService.isEnabled()
                && ChromeFeatureList.isEnabled(ChromeFeatureList.NOTIFICATION_SUSPENDER);
    }

    /**
     * Suspends the given notification if it originates from a suspended domain.
     * @param notification The notification to suspend.
     * @return A {@link Promise} that resolves to whether the given notification got suspended.
     */
    public static Promise<Boolean> maybeSuspendNotification(ChromeNotification notification) {
        // No need to initialize UsageStatsService if it is disabled.
        if (!isEnabled()) return Promise.fulfilled(false);
        return waitForChromeStartup()
                .then((Void v) -> UsageStatsService.getInstance().getAllSuspendedWebsitesAsync())
                .then((List<String> fqdns) -> {
                    if (!fqdns.contains(getValidFqdnOrEmptyString(notification))) return false;
                    UsageStatsService.getInstance()
                            .getNotificationSuspender()
                            .storeNotificationResources(CollectionUtil.newArrayList(notification));
                    return true;
                });
    }

    public NotificationSuspender(Profile profile) {
        mProfile = profile;
        mContext = ContextUtils.getApplicationContext();
        mNotificationManager =
                (NotificationManager) mContext.getSystemService(Context.NOTIFICATION_SERVICE);
    }

    public void setWebsitesSuspended(List<String> fqdns, boolean suspended) {
        if (fqdns.isEmpty() || !isEnabled()) return;
        if (suspended) {
            storeNotificationResources(getActiveNotificationsForFqdns(fqdns));
        } else {
            unsuspendWebsites(fqdns);
        }
    }

    private void storeNotificationResources(List<ChromeNotification> notifications) {
        if (notifications.isEmpty()) return;

        String[] ids = new String[notifications.size()];
        String[] origins = new String[notifications.size()];
        Bitmap[] resources = new Bitmap[notifications.size() * 3];

        for (int i = 0; i < notifications.size(); ++i) {
            Notification notification = notifications.get(i).getNotification();
            String tag = notifications.get(i).getMetadata().tag;
            ids[i] = tag;
            origins[i] = NotificationPlatformBridge.getOriginFromNotificationTag(tag);
            resources[i * 3 + 0] = getNotificationIcon(notification);
            resources[i * 3 + 1] = getNotificationBadge(notification);
            resources[i * 3 + 2] = getNotificationImage(notification);
            mNotificationManager.cancel(tag, NotificationPlatformBridge.PLATFORM_ID);
        }

        NotificationSuspenderJni.get().storeNotificationResources(
                mProfile, ids, origins, resources);
    }

    private void unsuspendWebsites(List<String> fqdns) {
        if (fqdns.isEmpty()) return;
        // Handle both http and https schemes as native expects origins.
        String[] origins = new String[fqdns.size() * 2];
        for (int i = 0; i < fqdns.size(); ++i) {
            origins[i * 2 + 0] = "http://" + fqdns.get(i);
            origins[i * 2 + 1] = "https://" + fqdns.get(i);
        }
        NotificationSuspenderJni.get().reDisplayNotifications(mProfile, origins);
    }

    @TargetApi(Build.VERSION_CODES.M)
    private List<ChromeNotification> getActiveNotificationsForFqdns(List<String> fqdns) {
        List<ChromeNotification> notifications = new ArrayList<>();

        for (StatusBarNotification notification : mNotificationManager.getActiveNotifications()) {
            if (notification.getId() != NotificationPlatformBridge.PLATFORM_ID) continue;
            String tag = notification.getTag();
            String origin = NotificationPlatformBridge.getOriginFromNotificationTag(tag);
            if (!URLUtil.isHttpUrl(origin) && !URLUtil.isHttpsUrl(origin)) continue;
            if (!fqdns.contains(Uri.parse(origin).getHost())) continue;
            NotificationMetadata metadata =
                    new NotificationMetadata(NotificationUmaTracker.SystemNotificationType.SITES,
                            tag, NotificationPlatformBridge.PLATFORM_ID);
            notifications.add(new ChromeNotification(notification.getNotification(), metadata));
        }

        return notifications;
    }

    @TargetApi(Build.VERSION_CODES.P)
    private Bitmap getBitmapFromIcon(Icon icon) {
        if (icon == null || icon.getType() != Icon.TYPE_BITMAP) return null;
        return ((BitmapDrawable) icon.loadDrawable(mContext)).getBitmap();
    }

    @TargetApi(Build.VERSION_CODES.M)
    private Bitmap getNotificationIcon(Notification notification) {
        return getBitmapFromIcon(notification.getLargeIcon());
    }

    @TargetApi(Build.VERSION_CODES.M)
    private Bitmap getNotificationBadge(Notification notification) {
        return getBitmapFromIcon(notification.getSmallIcon());
    }

    private Bitmap getNotificationImage(Notification notification) {
        return (Bitmap) notification.extras.get(Notification.EXTRA_PICTURE);
    }

    private static String getValidFqdnOrEmptyString(ChromeNotification notification) {
        String tag = notification.getMetadata().tag;
        String origin = NotificationPlatformBridge.getOriginFromNotificationTag(tag);
        if (TextUtils.isEmpty(origin)) return "";
        String host = Uri.parse(origin).getHost();
        return host == null ? "" : host;
    }

    private static Promise<Void> waitForChromeStartup() {
        BrowserStartupController browserStartup =
                BrowserStartupController.get(LibraryProcessType.PROCESS_BROWSER);
        if (browserStartup.isFullBrowserStarted()) return Promise.fulfilled(null);
        Promise<Void> promise = new Promise<>();
        browserStartup.addStartupCompletedObserver(new StartupCallback() {
            @Override
            public void onSuccess() {
                promise.fulfill(null);
            }
            @Override
            public void onFailure() {
                promise.reject(null);
            }
        });
        return promise;
    }

    @NativeMethods
    interface Natives {
        // Stores the given |resources| to be displayed later again. Note that |resources| is
        // expected to have 3 entries (icon, badge, image in that order) for each notification id in
        // |notificationIds|. If a notification does not have a particular resource, pass null
        // instead. |origins| must be the same size as |notificationIds|.
        void storeNotificationResources(
                Profile profile, String[] notificationIds, String[] origins, Bitmap[] resources);

        // Displays all suspended notifications for the given |origins|.
        void reDisplayNotifications(Profile profile, String[] origins);
    }
}
