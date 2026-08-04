// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.ui;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.os.IBinder;
import android.support.v4.media.session.MediaSessionCompat;
import android.util.SparseArray;

import androidx.annotation.VisibleForTesting;
import androidx.mediarouter.media.MediaRouter;

import org.chromium.base.ContextUtils;
import org.chromium.base.SplitCompatService;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.notifications.NotificationConstants;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker;
import org.chromium.chrome.browser.notifications.NotificationWrapperBuilderFactory;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions;
import org.chromium.components.browser_ui.media.MediaNotificationController;
import org.chromium.components.browser_ui.media.MediaNotificationInfo;
import org.chromium.components.browser_ui.media.MediaNotificationManager;
import org.chromium.components.browser_ui.media.MediaNotificationManager.MediaTypeId;
import org.chromium.components.browser_ui.notifications.ForegroundServiceUtils;
import org.chromium.components.browser_ui.notifications.NotificationMetadata;
import org.chromium.components.browser_ui.notifications.NotificationWrapper;
import org.chromium.components.browser_ui.notifications.NotificationWrapperBuilder;

/** A class that provides Chrome-specific behavior to {@link MediaNotificationController}. */
@NullMarked
class ChromeMediaNotificationControllerDelegate implements MediaNotificationController.Delegate {
    private final int mNotificationId;
    @MediaTypeId private final int mMediaTypeId;

    @VisibleForTesting
    static class NotificationOptions {
        public Class<?> serviceClass;
        public String groupName;

        public NotificationOptions(Class<?> serviceClass, String groupName) {
            this.serviceClass = serviceClass;
            this.groupName = groupName;
        }
    }

    // Maps the media type ids to their corresponding choices of the service, button receiver and
    // group name.
    @VisibleForTesting static SparseArray<NotificationOptions> sMapMediaTypeIdToOptions;

    static {
        sMapMediaTypeIdToOptions = new SparseArray<>();

        sMapMediaTypeIdToOptions.put(
                PlaybackListenerServiceImpl.MEDIA_TYPE_ID,
                new NotificationOptions(
                        ChromeMediaNotificationControllerServices.PlaybackListenerService.class,
                        NotificationConstants.GROUP_MEDIA_PLAYBACK));
        sMapMediaTypeIdToOptions.put(
                PresentationListenerServiceImpl.MEDIA_TYPE_ID,
                new NotificationOptions(
                        ChromeMediaNotificationControllerServices.PresentationListenerService.class,
                        NotificationConstants.GROUP_MEDIA_PRESENTATION));
        sMapMediaTypeIdToOptions.put(
                CastListenerServiceImpl.MEDIA_TYPE_ID,
                new NotificationOptions(
                        ChromeMediaNotificationControllerServices.CastListenerService.class,
                        NotificationConstants.GROUP_MEDIA_REMOTE));
    }

    /**
     * Service used to transform intent requests triggered from the notification into {@code
     * MediaNotificationListener} callbacks. We have to create a separate derived class for each
     * type of notification since one class corresponds to one instance of the service only.
     */
    @VisibleForTesting
    abstract static class ListenerServiceImpl extends SplitCompatService.Impl {
        @MediaTypeId private final int mMediaTypeId;

        ListenerServiceImpl(@MediaTypeId int mediaTypeId) {
            mMediaTypeId = mediaTypeId;
        }

        @Override
        public @Nullable IBinder onBind(Intent intent) {
            return null;
        }

        @Override
        public void onDestroy() {
            super.onDestroy();
            // TODO(crbug.com/522397811): We currently assume each media type shows only one
            // notification.
            MediaNotificationManager.onServiceDestroyed(mMediaTypeId);
            // In single-notification mode, destroying the shared service means the notification
            // for this media type should be removed. In multiple-notification mode, service
            // lifetime
            // is decoupled from individual notifications.
            if (!MediaNotificationManager.isMultipleMediaNotificationsEnabled()) {
                MediaNotificationManager.hideForAllTabs(mMediaTypeId);
            }
        }

        @Override
        public int onStartCommand(@Nullable Intent intent, int flags, int startId) {
            if (!processIntent(intent)) {
                // The service has been started with startForegroundService() but the
                // notification hasn't been shown. On O it will lead to the app crash.
                // So show an empty notification before stopping the service.
                // TODO(crbug.com/522397811): We currently assume each media type shows only one
                // notification. Calling stopListenerService() immediately will kill the service
                // for all active notifications of this type.
                MediaNotificationController.finishStartingForegroundServiceOnO(
                        getService(),
                        createNotificationWrapperBuilder(mMediaTypeId).buildNotificationWrapper());
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
        boolean processIntent(@Nullable Intent intent) {
            MediaNotificationController controller = getController(intent);
            if (controller == null) return false;

            return controller.processIntent(getService(), intent);
        }

        private @Nullable MediaNotificationController getController(@Nullable Intent intent) {
            if (intent != null
                    && intent.hasExtra(MediaNotificationController.EXTRA_NOTIFICATION_ID)) {
                int id =
                        intent.getIntExtra(
                                MediaNotificationController.EXTRA_NOTIFICATION_ID,
                                MediaNotificationInfo.INVALID_ID);
                return MediaNotificationManager.getControllerByNotificationId(id);
            }
            return MediaNotificationManager.getActiveOrFallbackControllerByMediaTypeId(
                    mMediaTypeId);
        }
    }

    /**
     * A {@link ListenerService} for the MediaSession web api. This class is used internally but has
     * to be public to be able to launch the service.
     */
    public static final class PlaybackListenerServiceImpl extends ListenerServiceImpl {
        @MediaTypeId static final int MEDIA_TYPE_ID = R.id.media_playback_notification;

        public PlaybackListenerServiceImpl() {
            super(MEDIA_TYPE_ID);
        }
    }

    /**
     * A {@link ListenerService} for casting. This class is used internally but has to be public to
     * be able to launch the service.
     */
    public static final class PresentationListenerServiceImpl extends ListenerServiceImpl {
        @MediaTypeId static final int MEDIA_TYPE_ID = R.id.presentation_notification;

        public PresentationListenerServiceImpl() {
            super(MEDIA_TYPE_ID);
        }
    }

    /**
     * A {@link ListenerService} for remoting. This class is used internally but has to be public to
     * be able to launch the service.
     */
    public static final class CastListenerServiceImpl extends ListenerServiceImpl {
        @MediaTypeId static final int MEDIA_TYPE_ID = R.id.remote_playback_notification;

        public CastListenerServiceImpl() {
            super(MEDIA_TYPE_ID);
        }
    }

    ChromeMediaNotificationControllerDelegate(int uniqueId, @MediaTypeId int mediaTypeId) {
        mNotificationId = uniqueId;
        mMediaTypeId = mediaTypeId;
    }

    @Override
    public @Nullable Intent createServiceIntent() {
        Class<?> serviceClass =
                assumeNonNull(sMapMediaTypeIdToOptions.get(mMediaTypeId)).serviceClass;
        return (serviceClass != null) ? new Intent(getContext(), serviceClass) : null;
    }

    @Override
    public String getAppName() {
        return getContext().getString(R.string.app_name);
    }

    @Override
    public String getNotificationGroupName() {
        String groupName = assumeNonNull(sMapMediaTypeIdToOptions.get(mMediaTypeId)).groupName;

        assert groupName != null;
        return groupName;
    }

    @Override
    public @MediaTypeId int getMediaTypeId() {
        return mMediaTypeId;
    }

    @Override
    public int getNotificationId() {
        return mNotificationId;
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
