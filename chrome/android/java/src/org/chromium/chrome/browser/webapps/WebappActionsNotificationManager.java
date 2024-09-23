// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;

import androidx.annotation.NonNull;
import androidx.core.app.NotificationCompat;

import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.blink.mojom.DisplayMode;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.WebappExtras;
import org.chromium.chrome.browser.customtabs.BaseCustomTabActivity;
import org.chromium.chrome.browser.customtabs.CustomTabLocator;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabProvider;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.PauseResumeWithNativeObserver;
import org.chromium.chrome.browser.notifications.NotificationConstants;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker;
import org.chromium.chrome.browser.notifications.NotificationWrapperBuilderFactory;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions;
import org.chromium.chrome.browser.share.ShareDelegate.ShareOrigin;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxyFactory;
import org.chromium.components.browser_ui.notifications.NotificationMetadata;
import org.chromium.components.browser_ui.notifications.NotificationWrapper;
import org.chromium.components.browser_ui.notifications.PendingIntentProvider;
import org.chromium.ui.base.Clipboard;

import java.lang.ref.WeakReference;

import javax.inject.Inject;

/**
 * Manages the notification shown by Chrome when running standalone Web Apps. It accomplishes
 * number of goals:
 * - Presents the current URL.
 * - Exposes 'Share' and 'Open in Chrome' actions.
 * - Messages that Web App runs in Chrome.
 */
@ActivityScope
class WebappActionsNotificationManager implements PauseResumeWithNativeObserver {
    private static final String ACTION_SHARE =
            "org.chromium.chrome.browser.webapps.NOTIFICATION_ACTION_SHARE";
    private static final String ACTION_OPEN_IN_CHROME =
            "org.chromium.chrome.browser.webapps.NOTIFICATION_ACTION_OPEN_IN_CHROME";
    private static final String ACTION_FOCUS =
            "org.chromium.chrome.browser.webapps.NOTIFICATION_ACTION_FOCUS";

    private final CustomTabActivityTabProvider mTabProvider;
    private final BrowserServicesIntentDataProvider mIntentDataProvider;

    @Inject
    public WebappActionsNotificationManager(
            CustomTabActivityTabProvider tabProvider,
            BrowserServicesIntentDataProvider intentDataProvider,
            ActivityLifecycleDispatcher lifecycleDispatcher) {
        mTabProvider = tabProvider;
        mIntentDataProvider = intentDataProvider;
        lifecycleDispatcher.register(this);
    }

    @Override
    public void onResumeWithNative() {
        maybeShowNotification(mTabProvider.getTab(), mIntentDataProvider);
    }

    @Override
    public void onPauseWithNative() {
        cancelNotification();
    }

    private static void maybeShowNotification(
            Tab tab, BrowserServicesIntentDataProvider intentDataProvider) {
        WebappExtras webappExtras = intentDataProvider.getWebappExtras();
        if (tab == null || webappExtras == null) return;

        // All features provided by the notification are also available in the minimal-ui toolbar.
        if (webappExtras.displayMode == DisplayMode.MINIMAL_UI) {
            return;
        }

        Context appContext = ContextUtils.getApplicationContext();
        NotificationWrapper notification = createNotification(appContext, tab, webappExtras);
        BaseNotificationManagerProxyFactory.create(appContext).notify(notification);

        NotificationUmaTracker.getInstance()
                .onNotificationShown(
                        NotificationUmaTracker.SystemNotificationType.WEBAPP_ACTIONS,
                        notification.getNotification());
    }

    private static NotificationWrapper createNotification(
            Context appContext, Tab tab, @NonNull WebappExtras webappExtras) {
        // The pending intents target an activity (instead of a service or a broadcast receiver) so
        // that the notification tray closes when a user taps the one of the notification action
        // links.
        PendingIntentProvider focusIntent =
                createPendingIntentWithAction(appContext, tab, ACTION_FOCUS);
        PendingIntentProvider openInChromeIntent =
                createPendingIntentWithAction(appContext, tab, ACTION_OPEN_IN_CHROME);
        PendingIntentProvider shareIntent =
                createPendingIntentWithAction(appContext, tab, ACTION_SHARE);

        NotificationMetadata metadata =
                new NotificationMetadata(
                        NotificationUmaTracker.SystemNotificationType.WEBAPP_ACTIONS,
                        /* notificationTag= */ null,
                        NotificationConstants.NOTIFICATION_ID_WEBAPP_ACTIONS);
        return NotificationWrapperBuilderFactory.createNotificationWrapperBuilder(
                        ChromeChannelDefinitions.ChannelId.WEBAPP_ACTIONS, metadata)
                .setSmallIcon(R.drawable.ic_chrome)
                .setContentTitle(webappExtras.shortName)
                .setContentText(appContext.getString(R.string.webapp_tap_to_copy_url))
                .setShowWhen(false)
                .setAutoCancel(false)
                .setOngoing(true)
                .setPriorityBeforeO(NotificationCompat.PRIORITY_MIN)
                .setContentIntent(focusIntent)
                .addAction(
                        R.drawable.ic_share_white_24dp,
                        appContext.getResources().getString(R.string.share),
                        shareIntent,
                        NotificationUmaTracker.ActionType.WEB_APP_ACTION_SHARE)
                .addAction(
                        R.drawable.ic_exit_to_app_white_24dp,
                        appContext.getResources().getString(R.string.menu_open_in_chrome),
                        openInChromeIntent,
                        NotificationUmaTracker.ActionType.WEB_APP_ACTION_OPEN_IN_CHROME)
                .buildNotificationWrapper();
    }

    /** Creates an intent which targets {@link WebappLauncherActivity} with the passed-in action. */
    private static PendingIntentProvider createPendingIntentWithAction(
            Context context, Tab tab, String action) {
        Intent intent = new Intent(action);
        intent.setClass(context, WebappLauncherActivity.class);
        IntentHandler.setTabId(intent, tab.getId());
        IntentUtils.addTrustedIntentExtras(intent);
        return PendingIntentProvider.getActivity(
                context,
                0,
                intent,
                PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_ONE_SHOT);
    }

    public static void cancelNotification() {
        NotificationManager nm =
                (NotificationManager)
                        ContextUtils.getApplicationContext()
                                .getSystemService(Context.NOTIFICATION_SERVICE);
        nm.cancel(NotificationConstants.NOTIFICATION_ID_WEBAPP_ACTIONS);
    }

    public static boolean handleNotificationAction(Intent intent) {
        if (!IntentHandler.wasIntentSenderChrome(intent)) return false;

        int tabId = IntentHandler.getTabId(intent);
        WeakReference<BaseCustomTabActivity> customTabActivityRef =
                CustomTabLocator.findCustomTabActivityWithTabId(tabId);
        if (customTabActivityRef == null) return false;

        BaseCustomTabActivity customTabActivity = customTabActivityRef.get();
        if (customTabActivity == null) return false;

        if (ACTION_SHARE.equals(intent.getAction())) {
            // Not routing through onMenuOrKeyboardAction to control UMA String.
            Tab tab = customTabActivity.getActivityTab();
            customTabActivity
                    .getShareDelegateSupplier()
                    .get()
                    .share(tab, false, ShareOrigin.WEBAPP_NOTIFICATION);
            RecordUserAction.record("Webapp.NotificationShare");
            return true;
        } else if (ACTION_OPEN_IN_CHROME.equals(intent.getAction())) {
            customTabActivity.onMenuOrKeyboardAction(
                    R.id.open_in_browser_id, /* fromMenu= */ false);
            return true;
        } else if (ACTION_FOCUS.equals(intent.getAction())) {
            Tab tab = customTabActivity.getActivityTab();
            if (tab != null) {
                Clipboard.getInstance().copyUrlToClipboard(tab.getOriginalUrl());
            }
            RecordUserAction.record("Webapp.NotificationFocused");
            return true;
        }
        return false;
    }
}
