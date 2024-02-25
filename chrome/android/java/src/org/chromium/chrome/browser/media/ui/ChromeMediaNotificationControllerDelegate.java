// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.ui;

import android.app.Service;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.media.AudioManager;
import android.os.IBinder;
import android.support.v4.media.session.MediaSessionCompat;
import android.util.SparseArray;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.mediarouter.media.MediaRouter;

import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.base.SplitCompatService;
import org.chromium.chrome.browser.notifications.NotificationConstants;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker;
import org.chromium.chrome.browser.notifications.NotificationWrapperBuilderFactory;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions;
import org.chromium.components.browser_ui.media.MediaNotificationController;
import org.chromium.components.browser_ui.media.MediaNotificationManager;
import org.chromium.components.browser_ui.notifications.ForegroundServiceUtils;
import org.chromium.components.browser_ui.notifications.NotificationMetadata;
import org.chromium.components.browser_ui.notifications.NotificationWrapper;
import org.chromium.components.browser_ui.notifications.NotificationWrapperBuilder;

/** A class that provides Chrome-specific behavior to {@link MediaNotificationController}. */
class ChromeMediaNotificationControllerDelegate implements MediaNotificationController.Delegate {
    private int mNotificationId;

    @VisibleForTesting
    static class NotificationOptions {
        public Class<?> serviceClass;
        public String groupName;

        public NotificationOptions(Class<?> serviceClass, String groupName) {
            this.serviceClass = serviceClass;
            this.groupName = groupName;
        }
    }

    // Maps the notification ids to their corresponding choices of the service, button receiver and
    // group name.
    @VisibleForTesting static SparseArray<NotificationOptions> sMapNotificationIdToOptions;

    static {
        sMapNotificationIdToOptions = new SparseArray<NotificationOptions>();

        sMapNotificationIdToOptions.put(
                PlaybackListenerServiceImpl.NOTIFICATION_ID,
                new NotificationOptions(
                        ChromeMediaNotificationControllerServices.PlaybackListenerService.class,
                        NotificationConstants.GROUP_MEDIA_PLAYBACK));
        sMapNotificationIdToOptions.put(
                PresentationListenerServiceImpl.NOTIFICATION_ID,
                new NotificationOptions(
                        ChromeMediaNotificationControllerServices.PresentationListenerService.class,
                        NotificationConstants.GROUP_MEDIA_PRESENTATION));
        sMapNotificationIdToOptions.put(
                CastListenerServiceImpl.NOTIFICATION_ID,
                new NotificationOptions(
                        ChromeMediaNotificationControllerServices.CastListenerService.class,
                        NotificationConstants.GROUP_MEDIA_REMOTE));
    }

    /**
     * Service used to transform intent requests triggered from the notification into
     * {@code MediaNotificationListener} callbacks. We have to create a separate derived class for
     * each type of notification since one class corresponds to one instance of the service only.
     */
    @VisibleForTesting
    abstract static class ListenerServiceImpl extends SplitCompatService.Impl {
        private int mNotificationId;

        ListenerServiceImpl(int notificationId) {
            mNotificationId = notificationId;
        }

        @Override
        public IBinder onBind(Intent intent) {
            return null;
        }

        @Override
        public void onDestroy() {
            super.onDestroy();
            MediaNotificationController controller = getController();
            if (controller != null) controller.onServiceDestroyed();
            MediaNotificationManager.clear(mNotificationId);
        }

        @Override
        public int onStartCommand(Intent intent, int flags, int startId) {
            if (!processIntent(intent)) {
                // The service has been started with startForegroundService() but the
                // notification hasn't been shown. On O it will lead to the app crash.
                // So show an empty notification before stopping the service.
                MediaNotificationController.finishStartingForegroundServiceOnO(
                        getService(),
                        createNotificationWrapperBuilder(mNotificationId)
                                .buildNotificationWrapper());
                stopListenerService();
            }
            return Service.START_NOT_STICKY;
        }

        @VisibleForTesting
        void stopListenerService() {
            // Call stopForeground to guarantee Android unset the foreground bit.
            ForegroundServiceUtils.getInstance()
                    .stopForeground(getService(), Service.STOP_FOREGROUND_REMOVE);
            getService().stopSelf();
        }

        @VisibleForTesting
        boolean processIntent(Intent intent) {
            MediaNotificationController controller = getController();
            if (controller == null) return false;

            return controller.processIntent(getService(), intent);
        }

        private @Nullable MediaNotificationController getController() {
            return MediaNotificationManager.getController(mNotificationId);
        }
    }

    /**
     * A {@link ListenerService} for the MediaSession web api.
     * This class is used internally but has to be public to be able to launch the service.
     */
    public static final class PlaybackListenerServiceImpl extends ListenerServiceImpl {
        static final int NOTIFICATION_ID = R.id.media_playback_notification;

        public PlaybackListenerServiceImpl() {
            super(NOTIFICATION_ID);
        }

        @Override
        public void onCreate() {
            super.onCreate();
            IntentFilter filter = new IntentFilter(AudioManager.ACTION_AUDIO_BECOMING_NOISY);
            ContextUtils.registerProtectedBroadcastReceiver(
                    getService(), mAudioBecomingNoisyReceiver, filter);
        }

        @Override
        public void onDestroy() {
            getService().unregisterReceiver(mAudioBecomingNoisyReceiver);
            super.onDestroy();
        }

        private BroadcastReceiver mAudioBecomingNoisyReceiver =
                new BroadcastReceiver() {
                    @Override
                    public void onReceive(Context context, Intent intent) {
                        if (!AudioManager.ACTION_AUDIO_BECOMING_NOISY.equals(intent.getAction())) {
                            return;
                        }

                        Intent i =
                                new Intent(
                                        getContext(),
                                        ChromeMediaNotificationControllerServices
                                                .PlaybackListenerService.class);
                        i.setAction(intent.getAction());
                        boolean succeeded = true;
                        try {
                            getContext().startService(i);
                        } catch (RuntimeException e) {
                            // This happens occasionally with "cannot start foreground service".
                            // It's not at all clear what causes it; no combination of
                            // multi-window / background
                            // unplugging headphones has managed to repro it locally.  While it
                            // might be possible to trampoline this through an activity like we do
                            // elsewhere for notifications, that's a fairly invasive change
                            // without a local repro.
                            //  So,
                            // for now, just log that this happened and move on.
                            // https://crbug.com/1245017
                            succeeded = false;
                        }
                        RecordHistogram.recordBooleanHistogram(
                                "Media.Android.BecomingNoisy", succeeded);
                    }
                };
    }

    /**
     * A {@link ListenerService} for casting.
     * This class is used internally but has to be public to be able to launch the service.
     */
    public static final class PresentationListenerServiceImpl extends ListenerServiceImpl {
        static final int NOTIFICATION_ID = R.id.presentation_notification;

        public PresentationListenerServiceImpl() {
            super(NOTIFICATION_ID);
        }
    }

    /**
     * A {@link ListenerService} for remoting.
     * This class is used internally but has to be public to be able to launch the service.
     */
    public static final class CastListenerServiceImpl extends ListenerServiceImpl {
        static final int NOTIFICATION_ID = R.id.remote_playback_notification;

        public CastListenerServiceImpl() {
            super(NOTIFICATION_ID);
        }
    }

    ChromeMediaNotificationControllerDelegate(int id) {
        mNotificationId = id;
    }

    @Override
    public Intent createServiceIntent() {
        Class<?> serviceClass = sMapNotificationIdToOptions.get(mNotificationId).serviceClass;
        return (serviceClass != null) ? new Intent(getContext(), serviceClass) : null;
    }

    @Override
    public String getAppName() {
        return getContext().getString(R.string.app_name);
    }

    @Override
    public String getNotificationGroupName() {
        String groupName = sMapNotificationIdToOptions.get(mNotificationId).groupName;

        assert groupName != null;
        return groupName;
    }

    @Override
    public NotificationWrapperBuilder createNotificationWrapperBuilder() {
        return createNotificationWrapperBuilder(mNotificationId);
    }

    @Override
    public void onMediaSessionUpdated(MediaSessionCompat session) {
        // Tell the MediaRouter about the session, so that Chrome can control the volume
        // on the remote cast device (if any).
        MediaRouter.getInstance(getContext()).setMediaSessionCompat(session);
    }

    @Override
    public void logNotificationShown(NotificationWrapper notification) {
        NotificationUmaTracker.getInstance()
                .onNotificationShown(
                        NotificationUmaTracker.SystemNotificationType.MEDIA,
                        notification.getNotification());
    }

    private static NotificationWrapperBuilder createNotificationWrapperBuilder(int notificationId) {
        NotificationMetadata metadata =
                new NotificationMetadata(
                        NotificationUmaTracker.SystemNotificationType.MEDIA,
                        /* notificationTag= */ null,
                        notificationId);
        return NotificationWrapperBuilderFactory.createNotificationWrapperBuilder(
                ChromeChannelDefinitions.ChannelId.MEDIA_PLAYBACK, metadata);
    }

    private static Context getContext() {
        return ContextUtils.getApplicationContext();
    }
}
