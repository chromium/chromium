// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import android.Manifest;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ServiceInfo;
import android.os.IBinder;
import android.util.SparseIntArray;

import androidx.annotation.Nullable;
import androidx.core.app.ActivityCompat;

import org.chromium.base.ContextUtils;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.build.BuildConfig;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.app.tabmodel.TabWindowManagerSingleton;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker;
import org.chromium.chrome.browser.notifications.NotificationWrapperBuilderFactory;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.MediaCaptureOverlayController;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxyFactory;
import org.chromium.components.browser_ui.notifications.ForegroundServiceUtils;
import org.chromium.components.browser_ui.notifications.NotificationMetadata;
import org.chromium.components.browser_ui.notifications.NotificationWrapper;
import org.chromium.components.browser_ui.notifications.NotificationWrapperBuilder;
import org.chromium.components.browser_ui.notifications.PendingIntentProvider;
import org.chromium.components.webrtc.MediaCaptureNotificationUtil;
import org.chromium.components.webrtc.MediaCaptureNotificationUtil.MediaType;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

import java.util.Comparator;
import java.util.HashSet;
import java.util.Iterator;
import java.util.Map;
import java.util.Set;
import java.util.TreeMap;

/** Service that creates/destroys the WebRTC notification when media capture starts/stops. */
public class MediaCaptureNotificationServiceImpl extends MediaCaptureNotificationService.Impl {
    private static final String ACTION_MEDIA_CAPTURE_UPDATE =
            "org.chromium.chrome.browser.media.SCREEN_CAPTURE_UPDATE";
    private static final String ACTION_SCREEN_CAPTURE_STOP =
            "org.chromium.chrome.browser.media.SCREEN_CAPTURE_STOP";

    private static final String NOTIFICATION_NAMESPACE = "MediaCaptureNotificationService";

    private static final String NOTIFICATION_ID_EXTRA = "NotificationId";
    private static final String NOTIFICATION_MEDIA_IS_INCOGNITO = "NotificationIsIncognito";
    private static final String NOTIFICATION_MEDIA_TYPE_EXTRA = "NotificationMediaType";
    private static final String NOTIFICATION_MEDIA_URL_EXTRA = "NotificationMediaUrl";

    private BaseNotificationManagerProxy mNotificationManager;
    private SharedPreferencesManager mSharedPreferences;
    private final SparseIntArray mNotificationsType = new SparseIntArray();
    private final TreeMap<Integer, NotificationWrapper> mNotifications =
            new TreeMap<>(Comparator.reverseOrder());

    private boolean mStartedForegroundService;

    @Override
    public void onCreate() {
        mNotificationManager =
                BaseNotificationManagerProxyFactory.create(ContextUtils.getApplicationContext());
        mSharedPreferences = ChromeSharedPreferences.getInstance();
        super.onCreate();
    }

    /**
     * @param notificationId Unique id of the notification.
     * @param mediaType Media type of the notification.
     * @return Whether the notification has already been created for provided notification id and
     *     mediaType.
     */
    private boolean doesNotificationNeedUpdate(int notificationId, @MediaType int mediaType) {
        return mNotificationsType.get(notificationId) != mediaType;
    }

    /**
     * @param notificationId Unique id of the notification.
     * @return Whether the notification has already been created for the provided notification id.
     */
    private boolean doesNotificationExist(int notificationId) {
        return mNotificationsType.indexOfKey(notificationId) >= 0;
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        if (intent == null || intent.getExtras() == null) {
            cancelPreviousWebRtcNotifications();
            getService().stopSelf();
        } else {
            String action = intent.getAction();
            int notificationId = intent.getIntExtra(NOTIFICATION_ID_EXTRA, Tab.INVALID_TAB_ID);
            int mediaType = intent.getIntExtra(NOTIFICATION_MEDIA_TYPE_EXTRA, MediaType.NO_MEDIA);
            String url = intent.getStringExtra(NOTIFICATION_MEDIA_URL_EXTRA);
            boolean isIncognito = intent.getBooleanExtra(NOTIFICATION_MEDIA_IS_INCOGNITO, false);

            if (ACTION_MEDIA_CAPTURE_UPDATE.equals(action)) {
                updateNotification(notificationId, mediaType, url, isIncognito, startId);
            } else if (ACTION_SCREEN_CAPTURE_STOP.equals(action)) {
                // Notify native to stop screen capture when the STOP button in notification
                // is clicked.
                final Tab tab = TabWindowManagerSingleton.getInstance().getTabById(notificationId);
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
                mSharedPreferences.readStringSet(
                        ChromePreferenceKeys.MEDIA_WEBRTC_NOTIFICATION_IDS, null);
        if (notificationIds == null) return;
        Iterator<String> iterator = notificationIds.iterator();
        while (iterator.hasNext()) {
            mNotificationManager.cancel(NOTIFICATION_NAMESPACE, Integer.parseInt(iterator.next()));
        }
        mSharedPreferences.removeKey(ChromePreferenceKeys.MEDIA_WEBRTC_NOTIFICATION_IDS);
    }

    /**
     * Updates the extisting notification or creates one if none exist for the provided
     * notificationId and mediaType.
     * @param notificationId Unique id of the notification.
     * @param mediaType Media type of the notification.
     * @param url Url of the current webrtc call.
     * @param startId Id for the service start request
     */
    private void updateNotification(
            int notificationId,
            @MediaType int mediaType,
            String url,
            boolean isIncognito,
            int startId) {
        if (doesNotificationExist(notificationId)
                && !doesNotificationNeedUpdate(notificationId, mediaType)) {
            return;
        }
        destroyNotification(notificationId);
        if (mediaType != MediaType.NO_MEDIA) {
            createNotification(notificationId, mediaType, url, isIncognito);
        }
        if (mNotificationsType.size() == 0) getService().stopSelf(startId);
    }

    /**
     * Destroys the notification for the id notificationId.
     *
     * @param notificationId Unique id of the notification.
     */
    private void destroyNotification(int notificationId) {
        if (doesNotificationExist(notificationId)) {
            if (mNotificationsType.get(notificationId) == MediaType.SCREEN_CAPTURE) {
                final Tab tab = TabWindowManagerSingleton.getInstance().getTabById(notificationId);
                if (tab != null) {
                    WindowAndroid window = tab.getWebContents().getTopLevelNativeWindow();
                    MediaCaptureOverlayController overlayController =
                            MediaCaptureOverlayController.from(window);
                    if (overlayController != null) {
                        overlayController.stopCapture(tab);
                    }
                }
            }
            if (BuildConfig.IS_DESKTOP_ANDROID) {
                if (mNotifications.size() > 1 && mNotifications.firstKey() == notificationId) {
                    // For large screen device, we use the previous notification to update
                    // foreground
                    // service when the latest notification is going to be removed.
                    Map.Entry<Integer, NotificationWrapper> previousNotification =
                            mNotifications.higherEntry(notificationId);
                    startOrUpdateForegroundService(
                            previousNotification.getKey(), previousNotification.getValue());
                }
            }
            mNotificationManager.cancel(NOTIFICATION_NAMESPACE, notificationId);
            mNotificationsType.delete(notificationId);
            if (BuildConfig.IS_DESKTOP_ANDROID) {
                mNotifications.remove(notificationId);
            }
            updateSharedPreferencesEntry(notificationId, true);
        }
    }

    private void createNotification(
            int notificationId, @MediaType int mediaType, String url, boolean isIncognito) {
        final String channelId =
                mediaType == MediaType.SCREEN_CAPTURE
                        ? ChromeChannelDefinitions.ChannelId.SCREEN_CAPTURE
                        : ChromeChannelDefinitions.ChannelId.WEBRTC_CAM_AND_MIC;

        Context appContext = ContextUtils.getApplicationContext();
        NotificationWrapperBuilder builder =
                NotificationWrapperBuilderFactory.createNotificationWrapperBuilder(
                        channelId,
                        new NotificationMetadata(
                                NotificationUmaTracker.SystemNotificationType.MEDIA_CAPTURE,
                                NOTIFICATION_NAMESPACE,
                                notificationId));

        Intent tabIntent =
                IntentHandler.createTrustedBringTabToFrontIntent(
                        notificationId, IntentHandler.BringToFrontSource.NOTIFICATION);
        PendingIntentProvider contentIntent =
                tabIntent == null
                        ? null
                        : PendingIntentProvider.getActivity(
                                appContext, notificationId, tabIntent, 0);
        // Add a "Stop" button to the screen capture notification and turn the notification
        // into a high priority one.
        PendingIntent stopIntent =
                mediaType == MediaType.SCREEN_CAPTURE
                        ? buildStopCapturePendingIntent(notificationId)
                        : null;
        NotificationWrapper notification =
                MediaCaptureNotificationUtil.createNotification(
                        builder,
                        mediaType,
                        isIncognito ? null : url,
                        appContext.getString(R.string.app_name),
                        contentIntent,
                        stopIntent);
        if (BuildConfig.IS_DESKTOP_ANDROID) {
            // For large screen device, we use the latest notification to start or update
            // the foreground service.
            startOrUpdateForegroundService(notificationId, notification);
        } else {
            mNotificationManager.notify(notification);
        }
        mNotificationsType.put(notificationId, mediaType);
        if (BuildConfig.IS_DESKTOP_ANDROID) {
            mNotifications.put(notificationId, notification);
        }
        updateSharedPreferencesEntry(notificationId, false);
        NotificationUmaTracker.getInstance()
                .onNotificationShown(
                        NotificationUmaTracker.SystemNotificationType.MEDIA_CAPTURE,
                        notification.getNotification());

        if (mediaType == MediaType.SCREEN_CAPTURE) {
            final Tab tab = TabWindowManagerSingleton.getInstance().getTabById(notificationId);
            if (tab != null) {
                WindowAndroid window = tab.getWebContents().getTopLevelNativeWindow();
                MediaCaptureOverlayController overlayController =
                        MediaCaptureOverlayController.from(window);
                if (overlayController != null) {
                    overlayController.startCapture(tab);
                }
            }
        }
    }

    private void startOrUpdateForegroundService(
            int notificationId, NotificationWrapper notification) {
        int foregroundServiceType = 0;
        if (ActivityCompat.checkSelfPermission(getService(), Manifest.permission.CAMERA)
                == PackageManager.PERMISSION_GRANTED) {
            foregroundServiceType |= ServiceInfo.FOREGROUND_SERVICE_TYPE_CAMERA;
        }
        if (ActivityCompat.checkSelfPermission(getService(), Manifest.permission.RECORD_AUDIO)
                == PackageManager.PERMISSION_GRANTED) {
            foregroundServiceType |= ServiceInfo.FOREGROUND_SERVICE_TYPE_MICROPHONE;
        }
        ForegroundServiceUtils.getInstance()
                .startForeground(
                        getService(),
                        notificationId,
                        notification.getNotification(),
                        foregroundServiceType);
        mStartedForegroundService = true;
    }

    /**
     * Update shared preferences entry with ids of the visible notifications.
     *
     * @param notificationId Id of the notification.
     * @param remove Boolean describing if the notification was added or removed.
     */
    private void updateSharedPreferencesEntry(int notificationId, boolean remove) {
        Set<String> notificationIds =
                new HashSet<>(
                        mSharedPreferences.readStringSet(
                                ChromePreferenceKeys.MEDIA_WEBRTC_NOTIFICATION_IDS,
                                new HashSet<>()));
        if (remove
                && !notificationIds.isEmpty()
                && notificationIds.contains(String.valueOf(notificationId))) {
            notificationIds.remove(String.valueOf(notificationId));
        } else if (!remove) {
            notificationIds.add(String.valueOf(notificationId));
        }
        mSharedPreferences.writeStringSet(
                ChromePreferenceKeys.MEDIA_WEBRTC_NOTIFICATION_IDS, notificationIds);
    }

    @Override
    public void onDestroy() {
        cancelPreviousWebRtcNotifications();
        if (mStartedForegroundService) {
            ForegroundServiceUtils.getInstance()
                    .stopForeground(getService(), Service.STOP_FOREGROUND_REMOVE);
        }
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
        SharedPreferencesManager sharedPreferences = ChromeSharedPreferences.getInstance();
        Set<String> notificationIds =
                sharedPreferences.readStringSet(
                        ChromePreferenceKeys.MEDIA_WEBRTC_NOTIFICATION_IDS, null);
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
     * @param url Url of the current webrtc call.
     */
    public static void updateMediaNotificationForTab(
            Context context, int tabId, @Nullable WebContents webContents, GURL url) {
        @MediaType int mediaType = getMediaType(webContents);
        if (!shouldStartService(context, mediaType, tabId)) return;
        Intent intent = new Intent(context, MediaCaptureNotificationService.class);
        intent.setAction(ACTION_MEDIA_CAPTURE_UPDATE);
        intent.putExtra(NOTIFICATION_ID_EXTRA, tabId);
        intent.putExtra(NOTIFICATION_MEDIA_URL_EXTRA, url.getSpec());
        intent.putExtra(NOTIFICATION_MEDIA_TYPE_EXTRA, mediaType);
        if (TabWindowManagerSingleton.getInstance().getTabById(tabId) != null) {
            intent.putExtra(
                    NOTIFICATION_MEDIA_IS_INCOGNITO,
                    TabWindowManagerSingleton.getInstance().getTabById(tabId).isIncognito());
        }
        context.startService(intent);
    }

    /** Clear any previous media notifications. */
    public static void clearMediaNotifications() {
        SharedPreferencesManager sharedPreferences = ChromeSharedPreferences.getInstance();
        Set<String> notificationIds =
                sharedPreferences.readStringSet(
                        ChromePreferenceKeys.MEDIA_WEBRTC_NOTIFICATION_IDS, null);
        if (notificationIds == null || notificationIds.isEmpty()) return;

        Context context = ContextUtils.getApplicationContext();
        context.startService(new Intent(context, MediaCaptureNotificationService.class));
    }

    /** Build PendingIntent for the actions of screen capture notification. */
    private PendingIntent buildStopCapturePendingIntent(int notificationId) {
        Intent intent = new Intent(getService(), MediaCaptureNotificationService.class);
        intent.setAction(ACTION_SCREEN_CAPTURE_STOP);
        intent.putExtra(NOTIFICATION_ID_EXTRA, notificationId);
        return PendingIntent.getService(
                ContextUtils.getApplicationContext(),
                notificationId,
                intent,
                PendingIntent.FLAG_UPDATE_CURRENT);
    }
}
