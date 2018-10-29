// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import android.app.Notification;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ResolveInfo;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.net.Uri;
import android.os.RemoteException;
import android.support.annotation.Nullable;
import android.support.customtabs.trusted.TrustedWebActivityService;
import android.support.customtabs.trusted.TrustedWebActivityServiceConnectionManager;
import android.support.customtabs.trusted.TrustedWebActivityServiceWrapper;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ContextUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.notifications.NotificationBuilderBase;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker;

import java.util.List;
import java.util.Set;

/**
 * Uses a Trusted Web Activity client to display notifications.
 */
public class TrustedWebActivityClient {
    private final TrustedWebActivityServiceConnectionManager mConnection;

    /**
     * Creates a TrustedWebActivityService.
     */
    public TrustedWebActivityClient(TrustedWebActivityServiceConnectionManager connection) {
        mConnection = connection;
    }

    /**
     * Whether a Trusted Web Activity client is available to display notifications of the given
     * scope.
     * @param scope The scope of the Service Worker that triggered the notification.
     * @return Whether a Trusted Web Activity client was found to show the notification.
     */
    public boolean twaExistsForScope(Uri scope) {
        return mConnection.serviceExistsForScope(scope, new Origin(scope).toString());
    }

    /**
     * Displays a notification through a Trusted Web Activity client.
     * @param scope The scope of the Service Worker that triggered the notification.
     * @param platformTag A notification tag.
     * @param platformId A notification id.
     * @param builder A builder for the notification to display.
     *                The Trusted Web Activity client may override the small icon.
     */
    public void notifyNotification(Uri scope, String platformTag, int platformId,
            NotificationBuilderBase builder) {
        Resources res = ContextUtils.getApplicationContext().getResources();
        String channelDisplayName = res.getString(R.string.notification_category_group_general);

        mConnection.execute(scope, new Origin(scope).toString(), service -> {
            fallbackToIconFromServiceIfNecessary(builder, service);

            Notification notification = builder.build();

            boolean success =
                    service.notify(platformTag, platformId, notification, channelDisplayName);

            if (success) {
                NotificationUmaTracker.getInstance().onNotificationShown(
                        NotificationUmaTracker.SystemNotificationType.SITES, notification);
            }
        });
    }

    private void fallbackToIconFromServiceIfNecessary(NotificationBuilderBase builder,
            TrustedWebActivityServiceWrapper service) throws RemoteException {
        if (builder.hasSmallIconForContent() && builder.hasStatusBarIconBitmap()) {
            return;
        }

        int id = service.getSmallIconId();
        if (id == TrustedWebActivityService.NO_ID) {
            return;
        }

        Bitmap bitmap = service.getSmallIconBitmap();
        if (!builder.hasStatusBarIconBitmap()) {
            builder.setStatusBarIconForUntrustedRemoteApp(id, bitmap);
        }
        if (!builder.hasSmallIconForContent()) {
            builder.setContentSmallIconForUntrustedRemoteApp(bitmap);
        }
    }

    /**
     * Cancels a notification through a Trusted Web Activity client.
     * @param scope The scope of the Service Worker that triggered the notification.
     * @param platformTag The tag of the notification to cancel.
     * @param platformId The id of the notification to cancel.
     */
    public void cancelNotification(Uri scope, String platformTag, int platformId) {
        mConnection.execute(scope, new Origin(scope).toString(),
                service -> service.cancel(platformTag, platformId));
    }

    /**
     * Registers the package of a Trusted Web Activity client app to be used to deal with
     * notifications from the given origin. This can be called on any thread, but may hit the disk
     * so should be called on a background thread if possible.
     * @param context A context used to access shared preferences.
     * @param origin The origin to use the client app for.
     * @param clientPackage The package of the client app.
     */
    public static void registerClient(Context context, Origin origin, String clientPackage) {
        TrustedWebActivityServiceConnectionManager
                .registerClient(context, origin.toString(), clientPackage);
    }

    /**
     * Searches through the given list of {@link ResolveInfo} for an Activity belonging to a package
     * that is verified for the given url. If such an Activity is found, an Intent to start that
     * Activity as a Trusted Web Activity is returned. Otherwise {@code null} is returned.
     *
     * If multiple {@link ResolveInfo}s in the list match this criteria, the first will be chosen.
     */
    public static @Nullable Intent createLaunchIntentForTwa(Context appContext, String url,
            List<ResolveInfo> resolveInfosForUrl) {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.TRUSTED_WEB_ACTIVITY)) return null;

        Origin origin = new Origin(url);

        // Trusted Web Activities only work with https so we can shortcut here.
        if (!origin.uri().getScheme().equals(UrlConstants.HTTPS_SCHEME)) return null;

        Set<String> verifiedPackages = TrustedWebActivityServiceConnectionManager
                .getVerifiedPackages(appContext, origin.toString());
        if (verifiedPackages == null || verifiedPackages.size() == 0) return null;

        String twaPackageName = null;
        String twaActivityName = null;
        for (ResolveInfo info : resolveInfosForUrl) {
            if (info.activityInfo == null) continue;

            if (verifiedPackages.contains(info.activityInfo.packageName)) {
                twaPackageName = info.activityInfo.packageName;
                twaActivityName = info.activityInfo.name;
                break;
            }
        }

        if (twaPackageName == null) return null;

        Intent intent = new Intent();
        intent.setData(Uri.parse(url));
        intent.setAction(Intent.ACTION_VIEW);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK
                | ApiCompatibilityUtils.getActivityNewDocumentFlag()
                | Intent.FLAG_ACTIVITY_CLEAR_TOP);
        intent.setComponent(new ComponentName(twaPackageName, twaActivityName));
        return intent;
    }
}
