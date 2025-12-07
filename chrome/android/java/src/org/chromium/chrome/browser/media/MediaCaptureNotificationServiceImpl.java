// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.Manifest;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ServiceInfo;
import android.os.IBinder;

import androidx.core.app.ActivityCompat;

import org.chromium.base.ContextUtils;
import org.chromium.base.DeviceInfo;
import org.chromium.base.IntentUtils;
import org.chromium.base.Log;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.base.SplitCompatService;
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
import org.chromium.content_public.browser.media.capture.ScreenCapture;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.Comparator;
import java.util.HashSet;
import java.util.Iterator;
import java.util.Map;
import java.util.Set;
import java.util.TreeMap;

/** Service that creates/destroys the WebRTC notification when media capture starts/stops. */
@NullMarked
public class MediaCaptureNotificationServiceImpl extends SplitCompatService.Impl {
    private static final String TAG = "MediaCapture";
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
    private final TreeMap<Integer, Set<@MediaType Integer>> mNotificationsType = new TreeMap<>();
    private final TreeMap<Integer, NotificationWrapper> mNotifications =
            new TreeMap<>(Comparator.reverseOrder());

    private boolean mStartedForegroundService;

    @Initializer
    @Override
    public void onCreate() {
        mNotificationManager = BaseNotificationManagerProxyFactory.create();
        mSharedPreferences = ChromeSharedPreferences.getInstance();
        super.onCreate();
    }

    /**
     * @param notificationId Unique id of the notification.
     * @param mediaTypes Media types of the notification.
     * @return Whether the notification has already been created for provided notification id and
     *     mediaTypes.
     */
    private boolean doesNotificationNeedUpdate(
            int notificationId, Set<@MediaType Integer> mediaTypes) {
        return !mediaTypes.equals(mNotificationsType.get(notificationId));
    }

    /**
     * @param notificationId Unique id of the notification.
     * @return Whether the notification has already been created for the provided notification id.
     */
    private boolean doesNotificationExist(int notificationId) {
        return mNotificationsType.containsKey(notificationId);
    }

    @Override
    public int onStartCommand(@Nullable Intent intent, int flags, int startId) {
        if (intent == null || intent.getExtras() == null) {
            cancelPreviousWebRtcNotifications();
            getService().stopSelf();
        } else {
            String action = intent.getAction();
            int notificationId = intent.getIntExtra(NOTIFICATION_ID_EXTRA, Tab.INVALID_TAB_ID);
            ArrayList<Integer> mediaTypesList =
                    intent.getIntegerArrayListExtra(NOTIFICATION_MEDIA_TYPE_EXTRA);
            Set<@MediaType Integer> mediaTypes =
                    mediaTypesList != null ? new HashSet<>(mediaTypesList) : new HashSet<>();
            String url = intent.getStringExtra(NOTIFICATION_MEDIA_URL_EXTRA);
            boolean isIncognito = intent.getBooleanExtra(NOTIFICATION_MEDIA_IS_INCOGNITO, false);

            if (ACTION_MEDIA_CAPTURE_UPDATE.equals(action)) {
                updateNotification(notificationId, mediaTypes, url, isIncognito, startId);
            } else if (ACTION_SCREEN_CAPTURE_STOP.equals(action)) {
                // Notify native to stop screen capture when the STOP button in notification
                // is clicked.
                final int tabId = getTabIdFromNotificationId(notificationId);
                final Tab tab = TabWindowManagerSingleton.getInstance().getTabById(tabId);
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
     * Updates the existing notification or creates one if none exist for the provided
     * notificationId and mediaTypes.
     *
     * @param notificationId Unique id of the notification.
     * @param mediaTypes Media types of the notification.
     * @param url Url of the current webrtc call.
     * @param startId Id for the service start request
     */
    private void updateNotification(
            int notificationId,
            Set<@MediaType Integer> mediaTypes,
            @Nullable String url,
            boolean isIncognito,
            int startId) {
        if (doesNotificationExist(notificationId)
                && !doesNotificationNeedUpdate(notificationId, mediaTypes)) {
            return;
        }
        destroyNotification(notificationId, mediaTypes);
        if (!mediaTypes.isEmpty()) {
            createNotification(notificationId, mediaTypes, url, isIncognito);
        }
        if (mNotificationsType.size() == 0) {
            getService().stopSelf(startId);
        }
    }

    private static boolean hasCapturingMediaType(@Nullable Set<@MediaType Integer> mediaTypes) {
        if (mediaTypes == null) {
            return false;
        }
        for (@MediaType int type : mediaTypes) {
            if (MediaCaptureNotificationUtil.isCapture(type)) {
                return true;
            }
        }
        return false;
    }

    /**
     * Destroys the notification for the id notificationId.
     *
     * @param notificationId Unique id of the notification.
     */
    private void destroyNotification(int notificationId, Set<@MediaType Integer> mediaTypes) {
        if (doesNotificationExist(notificationId)) {
            final var oldMediaTypes = mNotificationsType.get(notificationId);
            if (hasCapturingMediaType(oldMediaTypes)) {
                final int tabId = getTabIdFromNotificationId(notificationId);
                final Tab tab = TabWindowManagerSingleton.getInstance().getTabById(tabId);
                if (tab != null) {
                    WindowAndroid window =
                            assumeNonNull(tab.getWebContents()).getTopLevelNativeWindow();
                    MediaCaptureOverlayController overlayController =
                            MediaCaptureOverlayController.from(window);
                    if (overlayController != null) {
                        overlayController.stopCapture(tab);
                    }
                }
            }
            if (DeviceInfo.isDesktop()) {
                if (mNotifications.size() > 1 && mNotifications.firstKey() == notificationId) {
                    // For large screen device, we use the previous notification to update
                    // foreground
                    // service when the latest notification is going to be removed.
                    Map.Entry<Integer, NotificationWrapper> previousNotification =
                            mNotifications.higherEntry(notificationId);
                    startOrUpdateForegroundService(
                            previousNotification.getKey(),
                            previousNotification.getValue(),
                            mediaTypes);
                }
            }
            mNotificationManager.cancel(NOTIFICATION_NAMESPACE, notificationId);
            mNotificationsType.remove(notificationId);
            if (DeviceInfo.isDesktop()) {
                mNotifications.remove(notificationId);
            }
            updateSharedPreferencesEntry(notificationId, true);
        }
    }

    private @MediaType int getPrimaryMediaType(Set<@MediaType Integer> mediaTypes) {
        // We preferentially put the tab/window/screen capture types first because they need a stop
        // intent attached to the notification. We only support one notification for desktop for
        // now, so we pick the broadest scope capture preferentially as the primary media type for
        // e.g. setting the notification text.
        if (mediaTypes.contains(MediaType.SCREEN_CAPTURE)) {
            return MediaType.SCREEN_CAPTURE;
        }
        if (mediaTypes.contains(MediaType.WINDOW_CAPTURE)) {
            return MediaType.WINDOW_CAPTURE;
        }
        if (mediaTypes.contains(MediaType.TAB_CAPTURE)) {
            return MediaType.TAB_CAPTURE;
        }
        if (mediaTypes.contains(MediaType.AUDIO_AND_VIDEO)) {
            return MediaType.AUDIO_AND_VIDEO;
        }
        if (mediaTypes.contains(MediaType.VIDEO_ONLY)) {
            return MediaType.VIDEO_ONLY;
        }
        if (mediaTypes.contains(MediaType.AUDIO_ONLY)) {
            return MediaType.AUDIO_ONLY;
        }
        return MediaType.NO_MEDIA;
    }

    private void createNotification(
            int notificationId,
            Set<@MediaType Integer> mediaTypes,
            @Nullable String url,
            boolean isIncognito) {
        final String channelId =
                hasCapturingMediaType(mediaTypes)
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
                hasCapturingMediaType(mediaTypes)
                        ? buildStopCapturePendingIntent(notificationId)
                        : null;
        NotificationWrapper notification =
                MediaCaptureNotificationUtil.createNotification(
                        builder,
                        getPrimaryMediaType(mediaTypes),
                        isIncognito ? null : url,
                        appContext.getString(R.string.app_name),
                        contentIntent,
                        stopIntent);
        if (DeviceInfo.isDesktop()) {
            // For large screen device, we use the latest notification to start or update
            // the foreground service.
            startOrUpdateForegroundService(notificationId, notification, mediaTypes);
        } else {
            mNotificationManager.notify(notification);
        }
        mNotificationsType.put(notificationId, mediaTypes);
        if (DeviceInfo.isDesktop()) {
            mNotifications.put(notificationId, notification);
        }
        updateSharedPreferencesEntry(notificationId, false);
        NotificationUmaTracker.getInstance()
                .onNotificationShown(
                        NotificationUmaTracker.SystemNotificationType.MEDIA_CAPTURE,
                        notification.getNotification());

        if (hasCapturingMediaType(mediaTypes)) {
            final int tabId = getTabIdFromNotificationId(notificationId);
            final Tab tab = TabWindowManagerSingleton.getInstance().getTabById(tabId);
            if (tab != null) {
                WindowAndroid window =
                        assumeNonNull(tab.getWebContents()).getTopLevelNativeWindow();
                MediaCaptureOverlayController overlayController =
                        MediaCaptureOverlayController.from(window);
                if (overlayController != null) {
                    overlayController.startCapture(tab);
                }
            }
        }
    }

    private void startOrUpdateForegroundService(
            int notificationId,
            NotificationWrapper notification,
            Set<@MediaType Integer> newMediaTypes) {
        Set<@MediaType Integer> allMediaTypes = new HashSet<>(newMediaTypes);
        for (Set<@MediaType Integer> types : mNotificationsType.values()) {
            allMediaTypes.addAll(types);
        }

        int foregroundServiceType = 0;
        if (ActivityCompat.checkSelfPermission(getService(), Manifest.permission.CAMERA)
                == PackageManager.PERMISSION_GRANTED) {
            foregroundServiceType |= ServiceInfo.FOREGROUND_SERVICE_TYPE_CAMERA;
        }
        if (ActivityCompat.checkSelfPermission(getService(), Manifest.permission.RECORD_AUDIO)
                == PackageManager.PERMISSION_GRANTED) {
            foregroundServiceType |= ServiceInfo.FOREGROUND_SERVICE_TYPE_MICROPHONE;
        }
        if (allMediaTypes.contains(MediaType.TAB_CAPTURE)) {
            foregroundServiceType |= ServiceInfo.FOREGROUND_SERVICE_TYPE_MEDIA_PLAYBACK;
        }
        if (allMediaTypes.contains(MediaType.SCREEN_CAPTURE)
                || allMediaTypes.contains(MediaType.WINDOW_CAPTURE)) {
            foregroundServiceType |= ServiceInfo.FOREGROUND_SERVICE_TYPE_MEDIA_PROJECTION;
        }
        ForegroundServiceUtils.getInstance()
                .startForeground(
                        getService(),
                        notificationId,
                        notification.getNotification(),
                        foregroundServiceType);

        mStartedForegroundService = true;
        boolean isRunningMediaProjection =
                (foregroundServiceType & ServiceInfo.FOREGROUND_SERVICE_TYPE_MEDIA_PROJECTION) != 0;
        ScreenCapture.onForegroundServiceRunning(isRunningMediaProjection);
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
            mStartedForegroundService = false;
            ScreenCapture.onForegroundServiceRunning(false);
        }
        super.onDestroy();
    }

    @Override
    public boolean onUnbind(Intent intent) {
        cancelPreviousWebRtcNotifications();
        return super.onUnbind(intent);
    }

    @Override
    public @Nullable IBinder onBind(Intent intent) {
        return null;
    }

    /**
     * @param webContents the webContents for the tab. Used to query the media capture state.
     * @return A set of {@link MediaType} identifying what media is being captured.
     */
    private static Set<@MediaType Integer> getMediaTypes(@Nullable WebContents webContents) {
        Set<@MediaType Integer> mediaTypes = new HashSet<>();
        if (webContents == null) {
            return mediaTypes;
        }

        if (MediaCaptureDevicesDispatcherAndroid.isCapturingTab(webContents)) {
            mediaTypes.add(MediaType.TAB_CAPTURE);
        }

        if (MediaCaptureDevicesDispatcherAndroid.isCapturingWindow(webContents)) {
            mediaTypes.add(MediaType.WINDOW_CAPTURE);
        }

        if (MediaCaptureDevicesDispatcherAndroid.isCapturingScreen(webContents)) {
            mediaTypes.add(MediaType.SCREEN_CAPTURE);
        }

        boolean audio = MediaCaptureDevicesDispatcherAndroid.isCapturingAudio(webContents);
        boolean video = MediaCaptureDevicesDispatcherAndroid.isCapturingVideo(webContents);
        if (audio && video) {
            mediaTypes.add(MediaType.AUDIO_AND_VIDEO);
        } else if (audio) {
            mediaTypes.add(MediaType.AUDIO_ONLY);
        } else if (video) {
            mediaTypes.add(MediaType.VIDEO_ONLY);
        }

        return mediaTypes;
    }

    private static boolean shouldStartService(
            Set<@MediaType Integer> mediaTypes, int notificationId) {
        if (!mediaTypes.isEmpty()) {
            return true;
        }
        SharedPreferencesManager sharedPreferences = ChromeSharedPreferences.getInstance();
        Set<String> notificationIds =
                sharedPreferences.readStringSet(
                        ChromePreferenceKeys.MEDIA_WEBRTC_NOTIFICATION_IDS, null);
        if (notificationIds != null
                && !notificationIds.isEmpty()
                && notificationIds.contains(String.valueOf(notificationId))) {
            return true;
        }
        return false;
    }

    /**
     * Send an intent to MediaCaptureNotificationService to either create, update or destroy the
     * notification identified by tabId.
     *
     * @param tabId Unique tab id.
     * @param webContents The webContents of the tab; used to get the current media type.
     * @param url Url of the current webrtc call.
     */
    public static void updateMediaNotificationForTab(
            Context context, int tabId, @Nullable WebContents webContents, GURL url) {
        // On desktop, we currently only use a single notification for all tabs and hang all
        // foreground services off that notification.
        Set<@MediaType Integer> mediaTypes = getMediaTypes(webContents);
        final int notificationId = getNotificationIdFromTabId(tabId);
        if (!shouldStartService(mediaTypes, notificationId)) {
            return;
        }
        Intent intent = new Intent(context, MediaCaptureNotificationService.class);
        intent.setAction(ACTION_MEDIA_CAPTURE_UPDATE);
        intent.putExtra(NOTIFICATION_ID_EXTRA, notificationId);
        intent.putExtra(NOTIFICATION_MEDIA_URL_EXTRA, url.getSpec());
        intent.putIntegerArrayListExtra(NOTIFICATION_MEDIA_TYPE_EXTRA, new ArrayList<>(mediaTypes));
        Tab tab = TabWindowManagerSingleton.getInstance().getTabById(tabId);
        if (tab != null) {
            intent.putExtra(NOTIFICATION_MEDIA_IS_INCOGNITO, tab.isIncognito());
        }
        try {
            context.startService(intent);
        } catch (IllegalStateException e) {
            Log.e(TAG, "Unable to start service for update: " + e);
        }
    }

    /** Clear any previous media notifications. */
    public static void clearMediaNotifications() {
        SharedPreferencesManager sharedPreferences = ChromeSharedPreferences.getInstance();
        Set<String> notificationIds =
                sharedPreferences.readStringSet(
                        ChromePreferenceKeys.MEDIA_WEBRTC_NOTIFICATION_IDS, null);
        if (notificationIds == null || notificationIds.isEmpty()) return;

        Context context = ContextUtils.getApplicationContext();
        try {
            context.startService(new Intent(context, MediaCaptureNotificationService.class));
        } catch (IllegalStateException e) {
            Log.e(TAG, "Unable to start service for clear: " + e);
        }
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
                PendingIntent.FLAG_UPDATE_CURRENT
                        | IntentUtils.getPendingIntentMutabilityFlag(false));
    }

    // The tab IDs start from 0, but the notification ID for the foreground service cannot
    // be 0. Therefore, set the notification ID to tab ID + 1.
    private static int getNotificationIdFromTabId(int tabId) {
        return tabId + 1;
    }

    private static int getTabIdFromNotificationId(int notificationId) {
        return notificationId - 1;
    }
}
