// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications.scheduler;

import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.drawable.Icon;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.init.BrowserParts;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.init.EmptyBrowserParts;
import org.chromium.chrome.browser.notifications.NotificationIntentInterceptor;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker.SystemNotificationType;
import org.chromium.chrome.browser.notifications.NotificationWrapperBuilderFactory;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions.ChannelId;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxyFactory;
import org.chromium.components.browser_ui.notifications.NotificationMetadata;
import org.chromium.components.browser_ui.notifications.NotificationWrapper;
import org.chromium.components.browser_ui.notifications.NotificationWrapperBuilder;
import org.chromium.components.browser_ui.notifications.PendingIntentProvider;

import java.util.ArrayList;
import java.util.HashMap;

/** Used by notification scheduler to display the notification in Android UI. */
public class DisplayAgent {
    private static final String TAG = "DisplayAgent";
    private static final String DISPLAY_AGENT_TAG = "NotificationSchedulerDisplayAgent";

    private static final String EXTRA_INTENT_TYPE =
            "org.chromium.chrome.browser.notifications.scheduler.EXTRA_INTENT_TYPE";
    private static final String EXTRA_GUID =
            "org.chromium.chrome.browser.notifications.scheduler.EXTRA_GUID";
    private static final String EXTRA_ACTION_BUTTON_TYPE =
            "org.chromium.chrome.browser.notifications.scheduler.EXTRA_ACTION_BUTTON_TYPE";
    private static final String EXTRA_ACTION_BUTTON_ID =
            "org.chromium.chrome.browser.notifications.scheduler.EXTRA_ACTION_BUTTON_ID";
    private static final String EXTRA_SCHEDULER_CLIENT_TYPE =
            "org.chromium.chrome.browser.notifications.scheduler.EXTRA_SCHEDULER_CLIENT_TYPE ";

    /** Contains icon info on the notification. */
    private static class IconBundle {
        public final Bitmap bitmap;
        public final int resourceId;

        public IconBundle() {
            bitmap = null;
            resourceId = 0;
        }

        public IconBundle(Bitmap bitmap) {
            this.bitmap = bitmap;
            this.resourceId = 0;
        }

        public IconBundle(int resourceId) {
            this.bitmap = null;
            this.resourceId = resourceId;
        }
    }

    /** Contains button info on the notification. */
    private static class Button {
        public final String text;
        public final @ActionButtonType int type;
        public final String id;

        public Button(String text, @ActionButtonType int type, String id) {
            this.text = text;
            this.type = type;
            this.id = id;
        }
    }

    /**
     * Contains all data needed to build Android notification in the UI, specified by the client.
     */
    private static class NotificationData {
        public final String title;
        public final String message;
        public HashMap<Integer /*@IconType*/, IconBundle> icons = new HashMap<>();
        public ArrayList<Button> buttons = new ArrayList<>();

        private NotificationData(String title, String message) {
            this.title = title;
            this.message = message;
        }
    }

    @CalledByNative
    private static void addButton(
            NotificationData notificationData,
            @JniType("std::u16string") String text,
            @ActionButtonType int type,
            @JniType("std::string") String id) {
        notificationData.buttons.add(new Button(text, type, id));
    }

    @CalledByNative
    private static void addIcon(
            NotificationData notificationData,
            @IconType int type,
            @JniType("SkBitmap") Bitmap bitmap,
            int resourceId) {
        assert ((bitmap == null && resourceId != 0) || (bitmap != null && resourceId == 0));
        if (resourceId != 0) {
            notificationData.icons.put(type, new IconBundle(resourceId));
        } else {
            notificationData.icons.put(type, new IconBundle(bitmap));
        }
    }

    @CalledByNative
    private static NotificationData buildNotificationData(
            @JniType("std::u16string") String title, @JniType("std::u16string") String message) {
        return new NotificationData(title, message);
    }

    /**
     * Contains data used used by the notification scheduling system internally to build the
     * notification.
     */
    private static class SystemData {
        public @SchedulerClientType int type;
        public final String guid;

        public SystemData(@SchedulerClientType int type, String guid) {
            this.type = type;
            this.guid = guid;
        }
    }

    @CalledByNative
    private static SystemData buildSystemData(
            @SchedulerClientType int type, @JniType("std::string") String guid) {
        return new SystemData(type, guid);
    }

    /** Receives notification events from Android, like clicks, dismiss, etc. */
    public static final class Receiver extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            final BrowserParts parts =
                    new EmptyBrowserParts() {
                        @Override
                        public void finishNativeInitialization() {
                            handleUserAction(intent);
                        }
                    };

            // Try to load native.
            ChromeBrowserInitializer.getInstance().handlePreNativeStartupAndLoadLibraries(parts);
            ChromeBrowserInitializer.getInstance().handlePostNativeStartup(true, parts);
        }
    }

    private static void handleUserAction(Intent intent) {
        @NotificationIntentInterceptor.IntentType
        int intentType =
                IntentUtils.safeGetIntExtra(
                        intent,
                        EXTRA_INTENT_TYPE,
                        NotificationIntentInterceptor.IntentType.UNKNOWN);
        String guid = IntentUtils.safeGetStringExtra(intent, EXTRA_GUID);
        @SchedulerClientType
        int clientType =
                IntentUtils.safeGetIntExtra(
                        intent, EXTRA_SCHEDULER_CLIENT_TYPE, SchedulerClientType.UNKNOWN);
        switch (intentType) {
            case NotificationIntentInterceptor.IntentType.UNKNOWN:
                break;
            case NotificationIntentInterceptor.IntentType.CONTENT_INTENT:
                DisplayAgentJni.get()
                        .onUserAction(
                                clientType,
                                UserActionType.CLICK,
                                guid,
                                ActionButtonType.UNKNOWN_ACTION,
                                null);
                closeNotification(guid);
                break;
            case NotificationIntentInterceptor.IntentType.DELETE_INTENT:
                DisplayAgentJni.get()
                        .onUserAction(
                                clientType,
                                UserActionType.DISMISS,
                                guid,
                                ActionButtonType.UNKNOWN_ACTION,
                                null);
                break;
            case NotificationIntentInterceptor.IntentType.ACTION_INTENT:
                int actionButtonType =
                        IntentUtils.safeGetIntExtra(
                                intent, EXTRA_ACTION_BUTTON_TYPE, ActionButtonType.UNKNOWN_ACTION);
                String buttonId = IntentUtils.safeGetStringExtra(intent, EXTRA_ACTION_BUTTON_ID);
                DisplayAgentJni.get()
                        .onUserAction(
                                clientType,
                                UserActionType.BUTTON_CLICK,
                                guid,
                                actionButtonType,
                                buttonId);
                closeNotification(guid);
                break;
        }
    }

    private static void closeNotification(String guid) {
        BaseNotificationManagerProxyFactory.create(ContextUtils.getApplicationContext())
                .cancel(DISPLAY_AGENT_TAG, guid.hashCode());
    }

    /** Contains Android platform specific data to construct a notification. */
    private static class AndroidNotificationData {
        public final @ChannelId String channel;
        public final @SystemNotificationType int systemNotificationType;

        public AndroidNotificationData(String channel, int systemNotificationType) {
            this.channel = channel;
            this.systemNotificationType = systemNotificationType;
        }
    }

    private static AndroidNotificationData toAndroidNotificationData(SystemData systemData) {
        @ChannelId String channel = ChannelId.BROWSER;
        @SystemNotificationType int systemNotificationType = SystemNotificationType.UNKNOWN;
        return new AndroidNotificationData(channel, systemNotificationType);
    }

    private static Intent buildIntent(
            Context context,
            @NotificationIntentInterceptor.IntentType int intentType,
            SystemData systemData) {
        Intent intent = new Intent(context, DisplayAgent.Receiver.class);
        intent.putExtra(EXTRA_INTENT_TYPE, intentType);
        intent.putExtra(EXTRA_SCHEDULER_CLIENT_TYPE, systemData.type);
        intent.putExtra(EXTRA_GUID, systemData.guid);
        return intent;
    }

    @CalledByNative
    private static void showNotification(NotificationData notificationData, SystemData systemData) {
        AndroidNotificationData platformData = toAndroidNotificationData(systemData);
        // TODO(xingliu): Plumb platform specific data from native.
        // mode and provide correct notification id. Support buttons.
        Context context = ContextUtils.getApplicationContext();

        NotificationWrapperBuilder builder =
                NotificationWrapperBuilderFactory.createNotificationWrapperBuilder(
                        platformData.channel,
                        new NotificationMetadata(
                                platformData.systemNotificationType,
                                DISPLAY_AGENT_TAG,
                                systemData.guid.hashCode()));
        builder.setContentTitle(notificationData.title);
        builder.setContentText(notificationData.message);

        boolean hasSmallIcon = notificationData.icons.containsKey(IconType.SMALL_ICON);

        if (hasSmallIcon && notificationData.icons.get(IconType.SMALL_ICON).bitmap != null) {
            // Use bitmap as small icon.
            Icon smallIcon =
                    Icon.createWithBitmap(notificationData.icons.get(IconType.SMALL_ICON).bitmap);
            builder.setSmallIcon(smallIcon);
        } else {
            // Use resource Id as small icon, if invalid, use default Chrome icon instead.
            int resourceId = R.drawable.ic_chrome;
            if (hasSmallIcon && notificationData.icons.get(IconType.SMALL_ICON).resourceId != 0) {
                resourceId = notificationData.icons.get(IconType.SMALL_ICON).resourceId;
            }
            builder.setSmallIcon(resourceId);
        }

        if (notificationData.icons.containsKey(IconType.LARGE_ICON)
                && notificationData.icons.get(IconType.LARGE_ICON).bitmap != null) {
            builder.setLargeIcon(notificationData.icons.get(IconType.LARGE_ICON).bitmap);
        }

        // Default content click behavior.
        Intent contentIntent =
                buildIntent(
                        context,
                        NotificationIntentInterceptor.IntentType.CONTENT_INTENT,
                        systemData);
        builder.setContentIntent(
                PendingIntentProvider.getBroadcast(
                        context,
                        getRequestCode(
                                NotificationIntentInterceptor.IntentType.CONTENT_INTENT,
                                systemData.guid),
                        contentIntent,
                        PendingIntent.FLAG_UPDATE_CURRENT));

        // Default dismiss behavior.
        Intent dismissIntent =
                buildIntent(
                        context,
                        NotificationIntentInterceptor.IntentType.DELETE_INTENT,
                        systemData);
        builder.setDeleteIntent(
                PendingIntentProvider.getBroadcast(
                        context,
                        getRequestCode(
                                NotificationIntentInterceptor.IntentType.DELETE_INTENT,
                                systemData.guid),
                        dismissIntent,
                        PendingIntent.FLAG_UPDATE_CURRENT));

        // Add the buttons.
        for (int i = 0; i < notificationData.buttons.size(); i++) {
            Button button = notificationData.buttons.get(i);
            Intent actionIntent =
                    buildIntent(
                            context,
                            NotificationIntentInterceptor.IntentType.ACTION_INTENT,
                            systemData);
            actionIntent.putExtra(EXTRA_ACTION_BUTTON_TYPE, button.type);
            actionIntent.putExtra(EXTRA_ACTION_BUTTON_ID, button.id);

            // TODO(xingliu): Support button icon. See https://crbug.com/983354
            builder.addAction(
                    /* icon_id= */ 0,
                    button.text,
                    PendingIntentProvider.getBroadcast(
                            context,
                            getRequestCode(
                                    NotificationIntentInterceptor.IntentType.ACTION_INTENT,
                                    systemData.guid),
                            actionIntent,
                            PendingIntent.FLAG_UPDATE_CURRENT),
                    NotificationUmaTracker.ActionType.UNKNOWN);
        }

        NotificationWrapper notification = builder.buildNotificationWrapper();
        BaseNotificationManagerProxyFactory.create(ContextUtils.getApplicationContext())
                .notify(notification);
        NotificationUmaTracker.getInstance()
                .onNotificationShown(
                        platformData.systemNotificationType, notification.getNotification());
    }

    /**
     * Returns the request code for a specific intent. Android will not distinguish intents based on
     * extra data. Different intent must have different request code.
     */
    private static int getRequestCode(
            @NotificationIntentInterceptor.IntentType int intentType, String guid) {
        int hash = guid.hashCode();
        hash += 31 * hash + intentType;
        return hash;
    }

    private DisplayAgent() {}

    @NativeMethods
    interface Natives {
        void onUserAction(
                @SchedulerClientType int clientType,
                @UserActionType int actionType,
                @JniType("std::string") String guid,
                @ActionButtonType int type,
                @JniType("std::string") String buttonId);
    }
}
