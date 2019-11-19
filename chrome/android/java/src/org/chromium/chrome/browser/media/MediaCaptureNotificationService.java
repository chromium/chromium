// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import android.app.PendingIntent;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Build;
import android.os.IBinder;
import android.support.annotation.IntDef;
import android.support.annotation.Nullable;
import android.support.v4.app.NotificationCompat;
import android.util.SparseIntArray;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.document.ChromeIntentUtil;
import org.chromium.chrome.browser.notifications.ChromeNotification;
import org.chromium.chrome.browser.notifications.ChromeNotificationBuilder;
import org.chromium.chrome.browser.notifications.NotificationBuilderFactory;
import org.chromium.chrome.browser.notifications.NotificationManagerProxy;
import org.chromium.chrome.browser.notifications.NotificationManagerProxyImpl;
import org.chromium.chrome.browser.notifications.NotificationMetadata;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker;
import org.chromium.chrome.browser.notifications.PendingIntentProvider;
import org.chromium.chrome.browser.notifications.channels.ChannelDefinitions;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabWindowManager;
import org.chromium.content_public.browser.WebContents;

import java.net.MalformedURLException;
import java.net.URL;
import java.util.HashSet;
import java.util.Iterator;
import java.util.Set;

/**
 * Service that creates/destroys the WebRTC notification when media capture starts/stops.
 */
public class MediaCaptureNotificationService extends Service {
    private static final String ACTION_MEDIA_CAPTURE_UPDATE =
            "org.chromium.chrome.browser.media.SCREEN_CAPTURE_UPDATE";
    private static final String ACTION_SCREEN_CAPTURE_STOP =
            "org.chromium.chrome.browser.media.SCREEN_CAPTURE_STOP";

    private static final String NOTIFICATION_NAMESPACE = "MediaCaptureNotificationService";

    private static final String NOTIFICATION_ID_EXTRA = "NotificationId";
    private static final String NOTIFICATION_MEDIA_IS_INCOGNITO = "NotificationIsIncognito";
    private static final String NOTIFICATION_MEDIA_TYPE_EXTRA = "NotificationMediaType";
    private static final String NOTIFICATION_MEDIA_URL_EXTRA = "NotificationMediaUrl";

    private static final String WEBRTC_NOTIFICATION_IDS = "WebRTCNotificationIds";
    private static final String TAG = "MediaCapture";

    @IntDef({MediaType.NO_MEDIA, MediaType.AUDIO_AND_VIDEO, MediaType.VIDEO_ONLY,
            MediaType.AUDIO_ONLY, MediaType.SCREEN_CAPTURE})
    @interface MediaType {
        int NO_MEDIA = 0;
        int AUDIO_AND_VIDEO = 1;
        int VIDEO_ONLY = 2;
        int AUDIO_ONLY = 3;
        int SCREEN_CAPTURE = 4;
    }

    private NotificationManagerProxy mNotificationManager;
    private SharedPreferences mSharedPreferences;
    private final SparseIntArray mNotifications = new SparseIntArray();

    @Override
    public void onCreate() {
        mNotificationManager =
                new NotificationManagerProxyImpl(ContextUtils.getApplicationContext());
        mSharedPreferences = ContextUtils.getAppSharedPreferences();
        super.onCreate();
    }

    /**
     * @param notificationId Unique id of the notification.
     * @param mediaType Media type of the notification.
     * @return Whether the notification has already been created for provided notification id and
     *         mediaType.
     */
    private boolean doesNotificationNeedUpdate(int notificationId, @MediaType int mediaType) {
        return mNotifications.get(notificationId) != mediaType;
    }

    /**
     * @param notificationId Unique id of the notification.
     * @return Whether the notification has already been created for the provided notification id.
     */
    private boolean doesNotificationExist(int notificationId) {
        return mNotifications.indexOfKey(notificationId) >= 0;
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        if (intent == null || intent.getExtras() == null) {
            cancelPreviousWebRtcNotifications();
            stopSelf();
        } else {
            String action = intent.getAction();
            int notificationId = intent.getIntExtra(NOTIFICATION_ID_EXTRA, Tab.INVALID_TAB_ID);
            int mediaType = intent.getIntExtra(NOTIFICATION_MEDIA_TYPE_EXTRA, MediaType.NO_MEDIA);
            String url = intent.getStringExtra(NOTIFICATION_MEDIA_URL_EXTRA);
            boolean isIncognito = intent.getBooleanExtra(NOTIFICATION_MEDIA_IS_INCOGNITO, false);

            if (ACTION_MEDIA_CAPTURE_UPDATE.equals(action)) {
                updateNotification(notificationId, mediaType, url, isIncognito);
            } else if (ACTION_SCREEN_CAPTURE_STOP.equals(action)) {
                // Notify native to stop screen capture when the STOP button in notification
                // is clicked.
                final Tab tab = TabWindowManager.getInstance().getTabById(notificationId);
                if (tab != null) {
                    MediaCaptureDevicesDispatcherAndroid.notifyStopped(tab.getWebContents());
                }
            }
        }
        return super.onStartCommand(intent, flags, startId);
    }

    /**
     * Cancel all previously existing notifications. Essential while doing a clean start (may be
     * after a browser crash which caused old notifications to exist).
     */
    private void cancelPreviousWebRtcNotifications() {
        Set<String> notificationIds =
                mSharedPreferences.getStringSet(WEBRTC_NOTIFICATION_IDS, null);
        if (notificationIds == null) return;
        Iterator<String> iterator = notificationIds.iterator();
        while (iterator.hasNext()) {
            mNotificationManager.cancel(NOTIFICATION_NAMESPACE, Integer.parseInt(iterator.next()));
        }
        SharedPreferences.Editor sharedPreferenceEditor = mSharedPreferences.edit();
        sharedPreferenceEditor.remove(MediaCaptureNotificationService.WEBRTC_NOTIFICATION_IDS);
        sharedPreferenceEditor.apply();
    }

    /**
     * Updates the extisting notification or creates one if none exist for the provided
     * notificationId and mediaType.
     * @param notificationId Unique id of the notification.
     * @param mediaType Media type of the notification.
     * @param url Url of the current webrtc call.
     */
    private void updateNotification(
            int notificationId, @MediaType int mediaType, String url, boolean isIncognito) {
        if (doesNotificationExist(notificationId)
                && !doesNotificationNeedUpdate(notificationId, mediaType))  {
            return;
        }
        destroyNotification(notificationId);
        if (mediaType != MediaType.NO_MEDIA) {
            createNotification(notificationId, mediaType, url, isIncognito);
        }
        if (mNotifications.size() == 0) stopSelf();
    }

    /**
     * Destroys the notification for the id notificationId.
     * @param notificationId Unique id of the notification.
     */
    private void destroyNotification(int notificationId) {
        if (doesNotificationExist(notificationId)) {
            mNotificationManager.cancel(NOTIFICATION_NAMESPACE, notificationId);
            mNotifications.delete(notificationId);
            updateSharedPreferencesEntry(notificationId, true);
        }
    }

    private static boolean isRunningAtLeastN() {
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.N;
    }

    /**
     * Creates a notification for the provided notificationId and mediaType.
     * @param notificationId Unique id of the notification.
     * @param mediaType Media type of the notification.
     * @param url Url of the current webrtc call.
     */
    private void createNotification(
            int notificationId, @MediaType int mediaType, String url, boolean isIncognito) {
        final String channelId = mediaType == MediaType.SCREEN_CAPTURE
                ? ChannelDefinitions.ChannelId.SCREEN_CAPTURE
                : ChannelDefinitions.ChannelId.MEDIA;

        ChromeNotificationBuilder builder =
                NotificationBuilderFactory
                        .createChromeNotificationBuilder(true /* preferCompat */, channelId,
                                null /*remoteAppPackageName*/,
                                new NotificationMetadata(
                                        NotificationUmaTracker.SystemNotificationType.MEDIA_CAPTURE,
                                        NOTIFICATION_NAMESPACE, notificationId))
                        .setAutoCancel(false)
                        .setOngoing(true)
                        .setSmallIcon(getNotificationIconId(mediaType))
                        .setLocalOnly(true);

        Intent tabIntent = ChromeIntentUtil.createBringTabToFrontIntent(notificationId);
        if (tabIntent != null) {
            PendingIntentProvider contentIntent = PendingIntentProvider.getActivity(
                    ContextUtils.getApplicationContext(), notificationId, tabIntent, 0);
            builder.setContentIntent(contentIntent);
            if (mediaType == MediaType.SCREEN_CAPTURE) {
                // Add a "Stop" button to the screen capture notification and turn the notification
                // into a high priority one.
                builder.setPriorityBeforeO(NotificationCompat.PRIORITY_HIGH);
                builder.setVibrate(new long[0]);
                builder.addAction(R.drawable.ic_stop_white_36dp,
                        ContextUtils.getApplicationContext().getResources().getString(
                                R.string.accessibility_stop),
                        buildStopCapturePendingIntent(notificationId));
            }
        }

        StringBuilder descriptionText =
                new StringBuilder(getNotificationContentText(mediaType, url, isIncognito))
                        .append('.');

        String contentText;
        if (isIncognito) {
            builder.setSubText(ContextUtils.getApplicationContext().getResources().getString(
                    R.string.notification_incognito_tab));
            // App name is automatically added to the title from Android N,
            // but needs to be added explicitly for prior versions.
            String appNamePrefix = isRunningAtLeastN()
                    ? ""
                    : (ContextUtils.getApplicationContext().getString(R.string.app_name) + " - ");
            builder.setContentTitle(appNamePrefix + descriptionText.toString());
            contentText = ContextUtils.getApplicationContext().getResources().getString(
                    R.string.media_notification_link_text_incognito);
        } else {
            if (tabIntent == null) {
                descriptionText.append(" ").append(url);
            } else if (mediaType != MediaType.SCREEN_CAPTURE) {
                descriptionText.append(" ").append(
                        ContextUtils.getApplicationContext().getResources().getString(
                                R.string.media_notification_link_text, url));
            }

            // From Android N, notification by default has the app name and title should not be the
            // same as app name.
            if (Build.VERSION.SDK_INT < Build.VERSION_CODES.N) {
                builder.setContentTitle(
                        ContextUtils.getApplicationContext().getString(R.string.app_name));
            }
            contentText = descriptionText.toString();
        }
        builder.setContentText(contentText);

        ChromeNotification notification = builder.buildWithBigTextStyle(contentText);
        mNotificationManager.notify(notification);
        mNotifications.put(notificationId, mediaType);
        updateSharedPreferencesEntry(notificationId, false);
        NotificationUmaTracker.getInstance().onNotificationShown(
                NotificationUmaTracker.SystemNotificationType.MEDIA_CAPTURE,
                notification.getNotification());
    }

    /**
     * Builds notification content text for the provided mediaType and url.
     * @param mediaType Media type of the notification.
     * @param url Url of the current webrtc call.
     * @return A string builder initialized to the contents of the specified string.
     */
    private String getNotificationContentText(
            @MediaType int mediaType, String url, boolean hideUserData) {
        if (mediaType == MediaType.SCREEN_CAPTURE) {
            return ContextUtils.getApplicationContext().getResources().getString(hideUserData
                            ? R.string.screen_capture_incognito_notification_text
                            : R.string.screen_capture_notification_text,
                    url);
        }

        int notificationContentTextId = 0;
        if (mediaType == MediaType.AUDIO_AND_VIDEO) {
            notificationContentTextId = hideUserData
                    ? R.string.video_audio_call_incognito_notification_text_2
                    : R.string.video_audio_call_notification_text_2;
        } else if (mediaType == MediaType.VIDEO_ONLY) {
            notificationContentTextId = hideUserData
                    ? R.string.video_call_incognito_notification_text_2
                    : R.string.video_call_notification_text_2;
        } else if (mediaType == MediaType.AUDIO_ONLY) {
            notificationContentTextId = hideUserData
                    ? R.string.audio_call_incognito_notification_text_2
                    : R.string.audio_call_notification_text_2;
        }

        return ContextUtils.getApplicationContext().getResources().getString(
                notificationContentTextId);
    }

    /**
     * @param mediaType Media type of the notification.
     * @return An icon id of the provided mediaType.
     */
    private int getNotificationIconId(@MediaType int mediaType) {
        int notificationIconId = 0;
        if (mediaType == MediaType.AUDIO_AND_VIDEO) {
            notificationIconId = R.drawable.webrtc_video;
        } else if (mediaType == MediaType.VIDEO_ONLY) {
            notificationIconId = R.drawable.webrtc_video;
        } else if (mediaType == MediaType.AUDIO_ONLY) {
            notificationIconId = R.drawable.webrtc_audio;
        } else if (mediaType == MediaType.SCREEN_CAPTURE) {
            notificationIconId = R.drawable.webrtc_video;
        }
        return notificationIconId;
    }

    /**
     * Update shared preferences entry with ids of the visible notifications.
     * @param notificationId Id of the notification.
     * @param remove Boolean describing if the notification was added or removed.
     */
    private void updateSharedPreferencesEntry(int notificationId, boolean remove) {
        Set<String> notificationIds =
                new HashSet<String>(mSharedPreferences.getStringSet(WEBRTC_NOTIFICATION_IDS,
                        new HashSet<String>()));
        if (remove && !notificationIds.isEmpty()
                && notificationIds.contains(String.valueOf(notificationId))) {
            notificationIds.remove(String.valueOf(notificationId));
        } else if (!remove) {
            notificationIds.add(String.valueOf(notificationId));
        }
        SharedPreferences.Editor sharedPreferenceEditor =  mSharedPreferences.edit();
        sharedPreferenceEditor.putStringSet(WEBRTC_NOTIFICATION_IDS, notificationIds);
        sharedPreferenceEditor.apply();
    }

    @Override
    public void onDestroy() {
        cancelPreviousWebRtcNotifications();
        super.onDestroy();
    }

    @Override
    public boolean onUnbind(Intent intent) {
        cancelPreviousWebRtcNotifications();
        return super.onUnbind(intent);
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    /**
     * @param webContents the webContents for the tab. Used to query the media capture state.
     * @return A constant identifying what media is being captured.
     */
    private static int getMediaType(@Nullable WebContents webContents) {
        if (webContents == null) {
            return MediaType.NO_MEDIA;
        }

        if (MediaCaptureDevicesDispatcherAndroid.isCapturingScreen(webContents)) {
            return MediaType.SCREEN_CAPTURE;
        }

        boolean audio = MediaCaptureDevicesDispatcherAndroid.isCapturingAudio(webContents);
        boolean video = MediaCaptureDevicesDispatcherAndroid.isCapturingVideo(webContents);
        if (audio && video) {
            return MediaType.AUDIO_AND_VIDEO;
        } else if (audio) {
            return MediaType.AUDIO_ONLY;
        } else if (video) {
            return MediaType.VIDEO_ONLY;
        } else {
            return MediaType.NO_MEDIA;
        }
    }

    private static boolean shouldStartService(
            Context context, @MediaType int mediaType, int tabId) {
        if (mediaType != MediaType.NO_MEDIA) return true;
        SharedPreferences sharedPreferences =
                ContextUtils.getAppSharedPreferences();
        Set<String> notificationIds =
                sharedPreferences.getStringSet(WEBRTC_NOTIFICATION_IDS, null);
        if (notificationIds != null
                && !notificationIds.isEmpty()
                && notificationIds.contains(String.valueOf(tabId))) {
            return true;
        }
        return false;
    }

    /**
     * Send an intent to MediaCaptureNotificationService to either create, update or destroy the
     * notification identified by tabId.
     * @param tabId Unique notification id.
     * @param webContents The webContents of the tab; used to get the current media type.
     * @param fullUrl Url of the current webrtc call.
     */
    public static void updateMediaNotificationForTab(
            Context context, int tabId, @Nullable WebContents webContents, String fullUrl) {
        @MediaType
        int mediaType = getMediaType(webContents);
        if (!shouldStartService(context, mediaType, tabId)) return;
        Intent intent = new Intent(context, MediaCaptureNotificationService.class);
        intent.setAction(ACTION_MEDIA_CAPTURE_UPDATE);
        intent.putExtra(NOTIFICATION_ID_EXTRA, tabId);
        String baseUrl = fullUrl;
        try {
            URL url = new URL(fullUrl);
            baseUrl = url.getProtocol() + "://" + url.getHost();
        } catch (MalformedURLException e) {
            Log.w(TAG, "Error parsing the webrtc url, %s ", fullUrl);
        }
        intent.putExtra(NOTIFICATION_MEDIA_URL_EXTRA, baseUrl);
        intent.putExtra(NOTIFICATION_MEDIA_TYPE_EXTRA, mediaType);
        if (TabWindowManager.getInstance().getTabById(tabId) != null) {
            intent.putExtra(NOTIFICATION_MEDIA_IS_INCOGNITO,
                    TabWindowManager.getInstance().getTabById(tabId).isIncognito());
        }
        context.startService(intent);
    }

    /**
     * Clear any previous media notifications.
     */
    public static void clearMediaNotifications() {
        SharedPreferences sharedPreferences =
                ContextUtils.getAppSharedPreferences();
        Set<String> notificationIds =
                sharedPreferences.getStringSet(WEBRTC_NOTIFICATION_IDS, null);
        if (notificationIds == null || notificationIds.isEmpty()) return;

        Context context = ContextUtils.getApplicationContext();
        context.startService(new Intent(context, MediaCaptureNotificationService.class));
    }

    /**
     * Build PendingIntent for the actions of screen capture notification.
     */
    private PendingIntent buildStopCapturePendingIntent(int notificationId) {
        Intent intent = new Intent(this, MediaCaptureNotificationService.class);
        intent.setAction(ACTION_SCREEN_CAPTURE_STOP);
        intent.putExtra(NOTIFICATION_ID_EXTRA, notificationId);
        return PendingIntent.getService(ContextUtils.getApplicationContext(), notificationId,
                intent, PendingIntent.FLAG_UPDATE_CURRENT);
    }
}
