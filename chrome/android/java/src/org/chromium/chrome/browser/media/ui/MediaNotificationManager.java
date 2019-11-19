// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.ui;

import android.app.PendingIntent;
import android.app.Service;
import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Matrix;
import android.graphics.Paint;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.media.AudioManager;
import android.os.Build;
import android.os.Handler;
import android.os.IBinder;
import android.support.v4.app.NotificationCompat;
import android.support.v4.app.NotificationManagerCompat;
import android.support.v4.media.MediaMetadataCompat;
import android.support.v4.media.session.MediaSessionCompat;
import android.support.v4.media.session.PlaybackStateCompat;
import android.support.v7.media.MediaRouter;
import android.text.TextUtils;
import android.util.SparseArray;
import android.view.KeyEvent;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.CollectionUtil;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.SysUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.notifications.ChromeNotification;
import org.chromium.chrome.browser.notifications.ChromeNotificationBuilder;
import org.chromium.chrome.browser.notifications.ForegroundServiceUtils;
import org.chromium.chrome.browser.notifications.NotificationBuilderFactory;
import org.chromium.chrome.browser.notifications.NotificationConstants;
import org.chromium.chrome.browser.notifications.NotificationManagerProxy;
import org.chromium.chrome.browser.notifications.NotificationManagerProxyImpl;
import org.chromium.chrome.browser.notifications.NotificationMetadata;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker;
import org.chromium.chrome.browser.notifications.channels.ChannelDefinitions;
import org.chromium.media_session.mojom.MediaSessionAction;
import org.chromium.services.media_session.MediaMetadata;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * A class for notifications that provide information and optional media controls for a given media.
 * Internally implements a Service for transforming notification Intents into
 * {@link MediaNotificationListener} calls for all registered listeners.
 * There's one service started for a distinct notification id.
 */
public class MediaNotificationManager {
    private static final String TAG = "MediaNotification";

    static final int MINIMAL_MEDIA_IMAGE_SIZE_PX = 114;

    // The media artwork image resolution on high-end devices.
    private static final int HIGH_IMAGE_SIZE_PX = 512;

    // The media artwork image resolution on high-end devices.
    private static final int LOW_IMAGE_SIZE_PX = 256;

    // The maximum number of actions in CompactView media notification.
    private static final int COMPACT_VIEW_ACTIONS_COUNT = 3;

    // The maximum number of actions in BigView media notification.
    private static final int BIG_VIEW_ACTIONS_COUNT = 5;

    // Maps the notification ids to their corresponding notification managers.
    private static SparseArray<MediaNotificationManager> sManagers;

    // Overrides N detection. The production code will use |null|, which uses the Android version
    // code. Otherwise, |isRunningAtLeastN()| will return whatever value is set.
    @VisibleForTesting
    static Boolean sOverrideIsRunningNForTesting;

    // Maps the notification ids to their corresponding choices of the service, button receiver and
    // group name.
    @VisibleForTesting
    static SparseArray<NotificationOptions> sMapNotificationIdToOptions;

    static {
        sManagers = new SparseArray<MediaNotificationManager>();

        sMapNotificationIdToOptions = new SparseArray<NotificationOptions>();

        sMapNotificationIdToOptions.put(PlaybackListenerService.NOTIFICATION_ID,
                new NotificationOptions(PlaybackListenerService.class,
                        PlaybackMediaButtonReceiver.class,
                        NotificationConstants.GROUP_MEDIA_PLAYBACK));
        sMapNotificationIdToOptions.put(PresentationListenerService.NOTIFICATION_ID,
                new NotificationOptions(PresentationListenerService.class,
                        PresentationMediaButtonReceiver.class,
                        NotificationConstants.GROUP_MEDIA_PRESENTATION));
        sMapNotificationIdToOptions.put(CastListenerService.NOTIFICATION_ID,
                new NotificationOptions(CastListenerService.class, CastMediaButtonReceiver.class,
                        NotificationConstants.GROUP_MEDIA_REMOTE));
    }

    private final NotificationUmaTracker mNotificationUmaTracker;

    private int mNotificationId;

    // ListenerService running for the notification. Only non-null when showing.
    @VisibleForTesting
    ListenerService mService;

    private SparseArray<MediaButtonInfo> mActionToButtonInfo;

    @VisibleForTesting
    ChromeNotificationBuilder mNotificationBuilder;

    @VisibleForTesting
    Bitmap mDefaultNotificationLargeIcon;

    // |mMediaNotificationInfo| should be not null if and only if the notification is showing.
    @VisibleForTesting
    MediaNotificationInfo mMediaNotificationInfo;

    @VisibleForTesting
    MediaSessionCompat mMediaSession;

    @VisibleForTesting
    Throttler mThrottler;

    @VisibleForTesting
    static class Throttler {
        @VisibleForTesting
        static final int THROTTLE_MILLIS = 500;

        @VisibleForTesting
        MediaNotificationManager mManager;

        private final Handler mHandler;

        @VisibleForTesting
        Throttler(@NonNull MediaNotificationManager manager) {
            mManager = manager;
            mHandler = new Handler();
        }

        // When |mTask| is non-null, it will always be queued in mHandler. When |mTask| is non-null,
        // all notification updates will be throttled and their info will be stored as
        // mLastPendingInfo. When |mTask| fires, it will call {@link showNotification()} with
        // the latest queued notification info.
        @VisibleForTesting
        Runnable mTask;

        // The last pending info. If non-null, it will be the latest notification info.
        // Otherwise, the latest notification info will be |mManager.mMediaNotificationInfo|.
        @VisibleForTesting
        MediaNotificationInfo mLastPendingInfo;

        /**
         * Queue |mediaNotificationInfo| for update. In unthrottled state (i.e. |mTask| != null),
         * the notification will be updated immediately and enter the throttled state. In
         * unthrottled state, the method will only update the pending notification info, which will
         * be used for updating the notification when |mTask| is fired.
         *
         * @param mediaNotificationInfo The notification info to be queued.
         */
        public void queueNotification(MediaNotificationInfo mediaNotificationInfo) {
            assert mediaNotificationInfo != null;

            MediaNotificationInfo latestMediaNotificationInfo =
                    mLastPendingInfo != null ? mLastPendingInfo : mManager.mMediaNotificationInfo;

            if (shouldIgnoreMediaNotificationInfo(
                        latestMediaNotificationInfo, mediaNotificationInfo)) {
                return;
            }

            if (mTask == null) {
                showNotificationImmediately(mediaNotificationInfo);
            } else {
                mLastPendingInfo = mediaNotificationInfo;
            }
        }

        /**
         * Clears the pending notification and enter unthrottled state.
         */
        public void clearPendingNotifications() {
            mHandler.removeCallbacks(mTask);
            mLastPendingInfo = null;
            mTask = null;
        }

        @VisibleForTesting
        void showNotificationImmediately(MediaNotificationInfo mediaNotificationInfo) {
            // If no notification hasn't been updated in the last THROTTLE_MILLIS, update
            // immediately and queue a task for blocking further updates.
            mManager.showNotification(mediaNotificationInfo);
            mTask = new Runnable() {
                @Override
                public void run() {
                    if (mLastPendingInfo != null) {
                        // If any notification info is pended during the throttling time window,
                        // update the notification.
                        showNotificationImmediately(mLastPendingInfo);
                        mLastPendingInfo = null;
                    } else {
                        // Otherwise, clear the task so further update is unthrottled.
                        mTask = null;
                    }
                }
            };
            if (!mHandler.postDelayed(mTask, THROTTLE_MILLIS)) {
                Log.w(TAG, "Failed to post the throttler task.");
                mTask = null;
            }
        }
    }

    private final MediaSessionCompat.Callback mMediaSessionCallback =
            new MediaSessionCompat.Callback() {
                @Override
                public void onPlay() {
                    MediaNotificationManager.this.onPlay(
                            MediaNotificationListener.ACTION_SOURCE_MEDIA_SESSION);
                }

                @Override
                public void onPause() {
                    MediaNotificationManager.this.onPause(
                            MediaNotificationListener.ACTION_SOURCE_MEDIA_SESSION);
                }

                @Override
                public void onSkipToPrevious() {
                    MediaNotificationManager.this.onMediaSessionAction(
                            MediaSessionAction.PREVIOUS_TRACK);
                }

                @Override
                public void onSkipToNext() {
                    MediaNotificationManager.this.onMediaSessionAction(
                            MediaSessionAction.NEXT_TRACK);
                }

                @Override
                public void onFastForward() {
                    MediaNotificationManager.this.onMediaSessionAction(
                            MediaSessionAction.SEEK_FORWARD);
                }

                @Override
                public void onRewind() {
                    MediaNotificationManager.this.onMediaSessionAction(
                            MediaSessionAction.SEEK_BACKWARD);
                }

                @Override
                public void onSeekTo(long pos) {
                    MediaNotificationManager.this.onMediaSessionSeekTo(pos);
                }
            };

    @VisibleForTesting
    static class NotificationOptions {
        public Class<?> serviceClass;
        public Class<?> receiverClass;
        public String groupName;

        public NotificationOptions(
                Class<?> serviceClass, Class<?> receiverClass, String groupName) {
            this.serviceClass = serviceClass;
            this.receiverClass = receiverClass;
            this.groupName = groupName;
        }
    }

    // On O, if startForegroundService() was called, the app MUST call startForeground on the
    // created service no matter what or it will crash. Show the minimal notification. The caller is
    // responsible for hiding it afterwards.
    private static void finishStartingForegroundService(ListenerService s) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) return;
        NotificationMetadata metadata =
                new NotificationMetadata(NotificationUmaTracker.SystemNotificationType.MEDIA,
                        null /* notificationTag */, s.getNotificationId());
        ChromeNotificationBuilder builder =
                NotificationBuilderFactory.createChromeNotificationBuilder(true /* preferCompat */,
                        ChannelDefinitions.ChannelId.MEDIA, null /* remoteAppPackageName */,
                        metadata);
        ForegroundServiceUtils.getInstance().startForeground(s, s.getNotificationId(),
                builder.buildChromeNotification().getNotification(), 0 /* foregroundServiceType */);
    }

    /**
     * Service used to transform intent requests triggered from the notification into
     * {@code MediaNotificationListener} callbacks. We have to create a separate derived class for
     * each type of notification since one class corresponds to one instance of the service only.
     */
    @VisibleForTesting
    abstract static class ListenerService extends Service {
        @VisibleForTesting
        static final String ACTION_PLAY = "MediaNotificationManager.ListenerService.PLAY";
        @VisibleForTesting
        static final String ACTION_PAUSE = "MediaNotificationManager.ListenerService.PAUSE";
        @VisibleForTesting
        static final String ACTION_STOP = "MediaNotificationManager.ListenerService.STOP";
        @VisibleForTesting
        static final String ACTION_SWIPE = "MediaNotificationManager.ListenerService.SWIPE";
        @VisibleForTesting
        static final String ACTION_CANCEL = "MediaNotificationManager.ListenerService.CANCEL";
        @VisibleForTesting
        static final String ACTION_PREVIOUS_TRACK =
                "MediaNotificationManager.ListenerService.PREVIOUS_TRACK";
        @VisibleForTesting
        static final String ACTION_NEXT_TRACK =
                "MediaNotificationManager.ListenerService.NEXT_TRACK";
        @VisibleForTesting
        static final String ACTION_SEEK_FORWARD =
                "MediaNotificationManager.ListenerService.SEEK_FORWARD";
        @VisibleForTesting
        static final String ACTION_SEEK_BACKWARD =
                "MediaNotificationmanager.ListenerService.SEEK_BACKWARD";

        @Override
        public IBinder onBind(Intent intent) {
            return null;
        }

        @Override
        public void onDestroy() {
            super.onDestroy();

            MediaNotificationManager manager = getManager();
            if (manager == null) return;

            manager.onServiceDestroyed();
        }

        @Override
        public int onStartCommand(Intent intent, int flags, int startId) {
            if (!processIntent(intent)) {
                // The service has been started with startForegroundService() but the
                // notification hasn't been shown. On O it will lead to the app crash.
                // So show an empty notification before stopping the service.
                finishStartingForegroundService(this);
                stopListenerService();
            }

            return START_NOT_STICKY;
        }

        protected abstract int getNotificationId();

        @Nullable
        private MediaNotificationManager getManager() {
            return MediaNotificationManager.getManager(getNotificationId());
        }

        @VisibleForTesting
        void stopListenerService() {
            // Call stopForeground to guarantee  Android unset the foreground bit.
            ForegroundServiceUtils.getInstance().stopForeground(
                    this, Service.STOP_FOREGROUND_REMOVE);
            stopSelf();
        }

        @VisibleForTesting
        boolean processIntent(Intent intent) {
            if (intent == null) return false;

            MediaNotificationManager manager = getManager();
            if (manager == null || manager.mMediaNotificationInfo == null) return false;

            if (intent.getAction() == null) {
                // The intent comes from  {@link AppHooks#startForegroundService}.
                manager.onServiceStarted(this);
            } else {
                // The intent comes from the notification. In this case, {@link onServiceStarted()}
                // does need to be called.
                processAction(intent, manager);
            }
            return true;
        }

        @VisibleForTesting
        void processAction(Intent intent, MediaNotificationManager manager) {
            String action = intent.getAction();

            // Before Android L, instead of using the MediaSession callback, the system will fire
            // ACTION_MEDIA_BUTTON intents which stores the information about the key event.
            if (Intent.ACTION_MEDIA_BUTTON.equals(action)) {
                KeyEvent event = (KeyEvent) intent.getParcelableExtra(Intent.EXTRA_KEY_EVENT);
                if (event == null) return;
                if (event.getAction() != KeyEvent.ACTION_DOWN) return;
                switch (event.getKeyCode()) {
                    case KeyEvent.KEYCODE_MEDIA_PLAY:
                        manager.onPlay(
                                MediaNotificationListener.ACTION_SOURCE_MEDIA_SESSION);
                        break;
                    case KeyEvent.KEYCODE_MEDIA_PAUSE:
                        manager.onPause(
                                MediaNotificationListener.ACTION_SOURCE_MEDIA_SESSION);
                        break;
                    case KeyEvent.KEYCODE_HEADSETHOOK:
                    case KeyEvent.KEYCODE_MEDIA_PLAY_PAUSE:
                        if (manager.mMediaNotificationInfo.isPaused) {
                            manager.onPlay(MediaNotificationListener.ACTION_SOURCE_MEDIA_SESSION);
                        } else {
                            manager.onPause(
                                    MediaNotificationListener.ACTION_SOURCE_MEDIA_SESSION);
                        }
                        break;
                    case KeyEvent.KEYCODE_MEDIA_PREVIOUS:
                        manager.onMediaSessionAction(MediaSessionAction.PREVIOUS_TRACK);
                        break;
                    case KeyEvent.KEYCODE_MEDIA_NEXT:
                        manager.onMediaSessionAction(MediaSessionAction.NEXT_TRACK);
                        break;
                    case KeyEvent.KEYCODE_MEDIA_FAST_FORWARD:
                        manager.onMediaSessionAction(MediaSessionAction.SEEK_FORWARD);
                        break;
                    case KeyEvent.KEYCODE_MEDIA_REWIND:
                        manager.onMediaSessionAction(MediaSessionAction.SEEK_BACKWARD);
                        break;
                    default:
                        break;
                }
            } else if (ACTION_STOP.equals(action)
                    || ACTION_SWIPE.equals(action)
                    || ACTION_CANCEL.equals(action)) {
                manager.onStop(
                        MediaNotificationListener.ACTION_SOURCE_MEDIA_NOTIFICATION);
                stopListenerService();
            } else if (ACTION_PLAY.equals(action)) {
                manager.onPlay(MediaNotificationListener.ACTION_SOURCE_MEDIA_NOTIFICATION);
            } else if (ACTION_PAUSE.equals(action)) {
                manager.onPause(MediaNotificationListener.ACTION_SOURCE_MEDIA_NOTIFICATION);
            } else if (AudioManager.ACTION_AUDIO_BECOMING_NOISY.equals(action)) {
                manager.onPause(MediaNotificationListener.ACTION_SOURCE_HEADSET_UNPLUG);
            } else if (ACTION_PREVIOUS_TRACK.equals(action)) {
                manager.onMediaSessionAction(MediaSessionAction.PREVIOUS_TRACK);
            } else if (ACTION_NEXT_TRACK.equals(action)) {
                manager.onMediaSessionAction(MediaSessionAction.NEXT_TRACK);
            } else if (ACTION_SEEK_FORWARD.equals(action)) {
                manager.onMediaSessionAction(MediaSessionAction.SEEK_FORWARD);
            } else if (ACTION_SEEK_BACKWARD.equals(action)) {
                manager.onMediaSessionAction(MediaSessionAction.SEEK_BACKWARD);
            }
        }
    }

    /**
     * This class is used internally but have to be public to be able to launch the service.
     */
    public static final class PlaybackListenerService extends ListenerService {
        private static final int NOTIFICATION_ID = R.id.media_playback_notification;

        @Override
        public void onCreate() {
            super.onCreate();
            IntentFilter filter = new IntentFilter(AudioManager.ACTION_AUDIO_BECOMING_NOISY);
            registerReceiver(mAudioBecomingNoisyReceiver, filter);
        }

        @Override
        public void onDestroy() {
            unregisterReceiver(mAudioBecomingNoisyReceiver);
            super.onDestroy();
        }

        @Override
        protected int getNotificationId() {
            return NOTIFICATION_ID;
        }

        private BroadcastReceiver mAudioBecomingNoisyReceiver = new BroadcastReceiver() {
                @Override
                public void onReceive(Context context, Intent intent) {
                    if (!AudioManager.ACTION_AUDIO_BECOMING_NOISY.equals(intent.getAction())) {
                        return;
                    }

                    Intent i = new Intent(getContext(), PlaybackListenerService.class);
                    i.setAction(intent.getAction());
                    getContext().startService(i);
                }
            };
    }

    /**
     * This class is used internally but have to be public to be able to launch the service.
     */
    public static final class PresentationListenerService extends ListenerService {
        private static final int NOTIFICATION_ID = R.id.presentation_notification;

        @Override
        protected int getNotificationId() {
            return NOTIFICATION_ID;
        }
    }

    /**
     * This class is used internally but have to be public to be able to launch the service.
     */
    public static final class CastListenerService extends ListenerService {
        private static final int NOTIFICATION_ID = R.id.remote_notification;

        @Override
        protected int getNotificationId() {
            return NOTIFICATION_ID;
        }
    }

    // Three classes to specify the right notification id in the intent.

    /**
     * This class is used internally but have to be public to be able to launch the service.
     */
    public static final class PlaybackMediaButtonReceiver extends MediaButtonReceiver {
        @Override
        public Class<?> getServiceClass() {
            return PlaybackListenerService.class;
        }
    }

    /**
     * This class is used internally but have to be public to be able to launch the service.
     */
    public static final class PresentationMediaButtonReceiver extends MediaButtonReceiver {
        @Override
        public Class<?> getServiceClass() {
            return PresentationListenerService.class;
        }
    }

    /**
     * This class is used internally but have to be public to be able to launch the service.
     */
    public static final class CastMediaButtonReceiver extends MediaButtonReceiver {
        @Override
        public Class<?> getServiceClass() {
            return CastListenerService.class;
        }
    }

    @VisibleForTesting
    Intent createIntent() {
        Class<?> serviceClass = sMapNotificationIdToOptions.get(mNotificationId).serviceClass;

        return (serviceClass != null) ? new Intent(getContext(), serviceClass) : null;
    }

    private PendingIntent createPendingIntent(String action) {
        Intent intent = createIntent().setAction(action);
        return PendingIntent.getService(getContext(), 0, intent, PendingIntent.FLAG_CANCEL_CURRENT);
    }

    private Class<?> getButtonReceiverClass() {
        Class<?> receiverClass = sMapNotificationIdToOptions.get(mNotificationId).receiverClass;

        assert receiverClass != null;
        return receiverClass;
    }

    // Returns the notification group name used to prevent automatic grouping.
    private String getNotificationGroupName() {
        String groupName = sMapNotificationIdToOptions.get(mNotificationId).groupName;

        assert groupName != null;
        return groupName;
    }

    /**
     * Shows the notification with media controls with the specified media info. Replaces/updates
     * the current notification if already showing. Does nothing if |mediaNotificationInfo| hasn't
     * changed from the last one. If |mediaNotificationInfo.isPaused| is true and the tabId
     * mismatches |mMediaNotificationInfo.isPaused|, it is also no-op.
     *
     * @param notificationInfo information to show in the notification
     */
    public static void show(MediaNotificationInfo notificationInfo) {
        MediaNotificationManager manager = sManagers.get(notificationInfo.id);
        if (manager == null) {
            manager = new MediaNotificationManager(
                    NotificationUmaTracker.getInstance(), notificationInfo.id);
            sManagers.put(notificationInfo.id, manager);
        }

        manager.mThrottler.queueNotification(notificationInfo);
    }

    /**
     * Hides the notification for the specified tabId and notificationId
     *
     * @param tabId the id of the tab that showed the notification or invalid tab id.
     * @param notificationId the id of the notification to hide for this tab.
     */
    public static void hide(int tabId, int notificationId) {
        MediaNotificationManager manager = getManager(notificationId);
        if (manager == null) return;

        manager.hideNotification(tabId);
    }

    /**
     * Hides notifications with the specified id for all tabs if shown.
     *
     * @param notificationId the id of the notification to hide for all tabs.
     */
    public static void clear(int notificationId) {
        MediaNotificationManager manager = getManager(notificationId);
        if (manager == null) return;

        manager.clearNotification();
        sManagers.remove(notificationId);
    }

    /**
     * Hides notifications with all known ids for all tabs if shown.
     */
    public static void clearAll() {
        for (int i = 0; i < sManagers.size(); ++i) {
            MediaNotificationManager manager = sManagers.valueAt(i);
            manager.clearNotification();
        }
        sManagers.clear();
    }

    /**
     * Activates the Android MediaSession. This method is used to activate Android MediaSession more
     * often because some old version of Android might send events to the latest active session
     * based on when setActive(true) was called and regardless of the current playback state.
     * @param tabId the id of the tab requesting to reactivate the Android MediaSession.
     * @param notificationId the id of the notification to reactivate Android MediaSession for.
     */
    public static void activateAndroidMediaSession(int tabId, int notificationId) {
        MediaNotificationManager manager = getManager(notificationId);
        if (manager == null) return;
        manager.activateAndroidMediaSession(tabId);
    }

    /**
     * Downscale |icon| for display in the notification if needed. Returns null if |icon| is null.
     * If |icon| is larger than {@link getIdealMediaImageSize()}, scale it down to
     * {@link getIdealMediaImageSize()} and return. Otherwise return the original |icon|.
     * @param icon The icon to be scaled.
     */
    @Nullable
    public static Bitmap downscaleIconToIdealSize(@Nullable Bitmap icon) {
        if (icon == null) return null;

        int targetSize = getIdealMediaImageSize();

        Matrix m = new Matrix();
        int dominantLength = Math.max(icon.getWidth(), icon.getHeight());

        if (dominantLength < getIdealMediaImageSize()) return icon;

        // Move the center to (0,0).
        m.postTranslate(icon.getWidth() / -2.0f, icon.getHeight() / -2.0f);
        // Scale to desired size.
        float scale = 1.0f * targetSize / dominantLength;
        m.postScale(scale, scale);
        // Move to the desired place.
        m.postTranslate(targetSize / 2.0f, targetSize / 2.0f);

        // Draw the image.
        Bitmap paddedBitmap = Bitmap.createBitmap(targetSize, targetSize, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(paddedBitmap);
        Paint paint = new Paint(Paint.FILTER_BITMAP_FLAG);
        canvas.drawBitmap(icon, m, paint);
        return paddedBitmap;
    }

    /**
     * @returns The ideal size of the media image.
     */
    public static int getIdealMediaImageSize() {
        if (SysUtils.isLowEndDevice()) {
            return LOW_IMAGE_SIZE_PX;
        }
        return HIGH_IMAGE_SIZE_PX;
    }

    /**
     * @returns Whether |icon| is suitable as the media image, i.e. bigger than the minimal size.
     * @param icon The icon to be checked.
     */
    public static boolean isBitmapSuitableAsMediaImage(Bitmap icon) {
        return icon != null && icon.getWidth() >= MINIMAL_MEDIA_IMAGE_SIZE_PX
                && icon.getHeight() >= MINIMAL_MEDIA_IMAGE_SIZE_PX;
    }

    @VisibleForTesting
    static MediaNotificationManager getManager(int notificationId) {
        return sManagers.get(notificationId);
    }

    @VisibleForTesting
    static boolean hasManagerForTesting(int notificationId) {
        return getManager(notificationId) != null;
    }

    @VisibleForTesting
    static void setManagerForTesting(int notificationId, MediaNotificationManager manager) {
        sManagers.put(notificationId, manager);
    }

    private static boolean isRunningAtLeastN() {
        return (sOverrideIsRunningNForTesting != null)
                ? sOverrideIsRunningNForTesting
                : Build.VERSION.SDK_INT >= Build.VERSION_CODES.N;
    }

    /**
     * The class containing all the information for adding a button in the notification for an
     * action.
     */
    private static final class MediaButtonInfo {
        /** The resource ID of this media button icon. */
        public int iconResId;

        /** The resource ID of this media button description. */
        public int descriptionResId;

        /** The intent string to be fired when this media button is clicked. */
        public String intentString;

        public MediaButtonInfo(int buttonResId, int descriptionResId, String intentString) {
            this.iconResId = buttonResId;
            this.descriptionResId = descriptionResId;
            this.intentString = intentString;
        }
    }

    @VisibleForTesting
    MediaNotificationManager(NotificationUmaTracker umaTracker, int notificationId) {
        mNotificationUmaTracker = umaTracker;
        mNotificationId = notificationId;

        mActionToButtonInfo = new SparseArray<>();

        mActionToButtonInfo.put(MediaSessionAction.PLAY,
                new MediaButtonInfo(R.drawable.ic_play_arrow_white_36dp,
                        R.string.accessibility_play, ListenerService.ACTION_PLAY));
        mActionToButtonInfo.put(MediaSessionAction.PAUSE,
                new MediaButtonInfo(R.drawable.ic_pause_white_36dp, R.string.accessibility_pause,
                        ListenerService.ACTION_PAUSE));
        mActionToButtonInfo.put(MediaSessionAction.STOP,
                new MediaButtonInfo(R.drawable.ic_stop_white_36dp, R.string.accessibility_stop,
                        ListenerService.ACTION_STOP));
        mActionToButtonInfo.put(MediaSessionAction.PREVIOUS_TRACK,
                new MediaButtonInfo(R.drawable.ic_skip_previous_white_36dp,
                        R.string.accessibility_previous_track,
                        ListenerService.ACTION_PREVIOUS_TRACK));
        mActionToButtonInfo.put(MediaSessionAction.NEXT_TRACK,
                new MediaButtonInfo(R.drawable.ic_skip_next_white_36dp,
                        R.string.accessibility_next_track, ListenerService.ACTION_NEXT_TRACK));
        mActionToButtonInfo.put(MediaSessionAction.SEEK_FORWARD,
                new MediaButtonInfo(R.drawable.ic_fast_forward_white_36dp,
                        R.string.accessibility_seek_forward, ListenerService.ACTION_SEEK_FORWARD));
        mActionToButtonInfo.put(MediaSessionAction.SEEK_BACKWARD,
                new MediaButtonInfo(R.drawable.ic_fast_rewind_white_36dp,
                        R.string.accessibility_seek_backward,
                        ListenerService.ACTION_SEEK_BACKWARD));

        mThrottler = new Throttler(this);
    }

    /**
     * Registers the started {@link Service} with the manager and creates the notification.
     *
     * @param service the service that was started
     */
    @VisibleForTesting
    void onServiceStarted(ListenerService service) {
        if (mService == service) return;

        mService = service;
        updateNotification(true /*serviceStarting*/, true /*shouldLogNotification*/);
    }

    /**
     * Handles the service destruction destruction.
     */
    @VisibleForTesting
    void onServiceDestroyed() {
        mService = null;
        if (mMediaNotificationInfo != null) clear(mMediaNotificationInfo.id);
    }

    @VisibleForTesting
    void onPlay(int actionSource) {
        // MediaSessionCompat calls this sometimes when `mMediaNotificationInfo`
        // is no longer available. It's unclear if it is a Support Library issue
        // or something that isn't properly cleaned up but given that the
        // crashes are rare and the fix is simple, null check was enough.
        if (mMediaNotificationInfo == null || !mMediaNotificationInfo.isPaused) return;
        mMediaNotificationInfo.listener.onPlay(actionSource);
    }

    @VisibleForTesting
    void onPause(int actionSource) {
        // MediaSessionCompat calls this sometimes when `mMediaNotificationInfo`
        // is no longer available. It's unclear if it is a Support Library issue
        // or something that isn't properly cleaned up but given that the
        // crashes are rare and the fix is simple, null check was enough.
        if (mMediaNotificationInfo == null || mMediaNotificationInfo.isPaused) return;
        mMediaNotificationInfo.listener.onPause(actionSource);
    }

    @VisibleForTesting
    void onStop(int actionSource) {
        // MediaSessionCompat calls this sometimes when `mMediaNotificationInfo`
        // is no longer available. It's unclear if it is a Support Library issue
        // or something that isn't properly cleaned up but given that the
        // crashes are rare and the fix is simple, null check was enough.
        if (mMediaNotificationInfo == null) return;
        mMediaNotificationInfo.listener.onStop(actionSource);
    }

    @VisibleForTesting
    void onMediaSessionAction(int action) {
        // MediaSessionCompat calls this sometimes when `mMediaNotificationInfo`
        // is no longer available. It's unclear if it is a Support Library issue
        // or something that isn't properly cleaned up but given that the
        // crashes are rare and the fix is simple, null check was enough.
        if (mMediaNotificationInfo == null) return;
        mMediaNotificationInfo.listener.onMediaSessionAction(action);
    }

    @VisibleForTesting
    void onMediaSessionSeekTo(long pos) {
        // MediaSessionCompat calls this sometimes when `mMediaNotificationInfo`
        // is no longer available. It's unclear if it is a Support Library issue
        // or something that isn't properly cleaned up but given that the
        // crashes are rare and the fix is simple, null check was enough.
        if (mMediaNotificationInfo == null) return;
        mMediaNotificationInfo.listener.onMediaSessionSeekTo(pos);
    }

    @VisibleForTesting
    void showNotification(MediaNotificationInfo mediaNotificationInfo) {
        if (shouldIgnoreMediaNotificationInfo(mMediaNotificationInfo, mediaNotificationInfo)) {
            return;
        }

        mMediaNotificationInfo = mediaNotificationInfo;

        // If there's no pending service start request, don't try to start service. If there is a
        // pending service start request but the service haven't started yet, only update the
        // |mMediaNotificationInfo|. The service will update the notification later once it's
        // started.
        if (mService == null && mediaNotificationInfo.isPaused) return;

        if (mService == null) {
            updateMediaSession();
            updateNotificationBuilder();
            ForegroundServiceUtils.getInstance().startForegroundService(createIntent());
        } else {
            updateNotification(false, false);
        }
    }

    private static boolean shouldIgnoreMediaNotificationInfo(
            MediaNotificationInfo oldInfo, MediaNotificationInfo newInfo) {
        return newInfo.equals(oldInfo)
                || ((newInfo.isPaused && oldInfo != null && newInfo.tabId != oldInfo.tabId));
    }

    @VisibleForTesting
    void clearNotification() {
        mThrottler.clearPendingNotifications();
        if (mMediaNotificationInfo == null) return;

        NotificationManagerCompat manager = NotificationManagerCompat.from(getContext());
        manager.cancel(mMediaNotificationInfo.id);

        if (mMediaSession != null) {
            mMediaSession.setMediaButtonReceiver(null);
            mMediaSession.setCallback(null);
            mMediaSession.setActive(false);
            mMediaSession.release();
            mMediaSession = null;
        }
        if (mService != null) {
            ForegroundServiceUtils.getInstance().stopForeground(
                    mService, Service.STOP_FOREGROUND_REMOVE);
            mService.stopSelf();
        }
        mMediaNotificationInfo = null;
        mNotificationBuilder = null;
    }

    private void hideNotification(int tabId) {
        if (mMediaNotificationInfo == null || tabId != mMediaNotificationInfo.tabId) return;
        clearNotification();
    }

    @NonNull
    @VisibleForTesting
    MediaMetadataCompat createMetadata() {
        // Can't return null as {@link MediaSessionCompat#setMetadata()} will crash in some versions
        // of the Android compat library.
        MediaMetadataCompat.Builder metadataBuilder = new MediaMetadataCompat.Builder();
        if (mMediaNotificationInfo.isPrivate) return metadataBuilder.build();

        metadataBuilder.putString(MediaMetadataCompat.METADATA_KEY_TITLE,
                mMediaNotificationInfo.metadata.getTitle());
        metadataBuilder.putString(MediaMetadataCompat.METADATA_KEY_ARTIST,
                mMediaNotificationInfo.origin);

        if (!TextUtils.isEmpty(mMediaNotificationInfo.metadata.getArtist())) {
            metadataBuilder.putString(MediaMetadataCompat.METADATA_KEY_ARTIST,
                    mMediaNotificationInfo.metadata.getArtist());
        }
        if (!TextUtils.isEmpty(mMediaNotificationInfo.metadata.getAlbum())) {
            metadataBuilder.putString(MediaMetadataCompat.METADATA_KEY_ALBUM,
                    mMediaNotificationInfo.metadata.getAlbum());
        }
        if (mMediaNotificationInfo.mediaSessionImage != null) {
            metadataBuilder.putBitmap(MediaMetadataCompat.METADATA_KEY_ALBUM_ART,
                                      mMediaNotificationInfo.mediaSessionImage);
        }
        if (mMediaNotificationInfo.mediaPosition != null) {
            metadataBuilder.putLong(MediaMetadataCompat.METADATA_KEY_DURATION,
                    mMediaNotificationInfo.mediaPosition.getDuration());
        }

        return metadataBuilder.build();
    }

    @VisibleForTesting
    void updateNotification(boolean serviceStarting, boolean shouldLogNotification) {
        if (mService == null) return;

        if (mMediaNotificationInfo == null) {
            if (serviceStarting) {
                finishStartingForegroundService(mService);
                ForegroundServiceUtils.getInstance().stopForeground(
                        mService, Service.STOP_FOREGROUND_REMOVE);
            }
            return;
        }
        updateMediaSession();
        updateNotificationBuilder();

        ChromeNotification notification = mNotificationBuilder.buildChromeNotification();

        // On O, finish starting the foreground service nevertheless, or Android will
        // crash Chrome.
        boolean foregroundedService = false;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O && serviceStarting) {
            ForegroundServiceUtils.getInstance().startForeground(mService,
                    mMediaNotificationInfo.id, notification.getNotification(),
                    0 /*foregroundServiceType*/);
            foregroundedService = true;
        }

        // We keep the service as a foreground service while the media is playing. When it is not,
        // the service isn't stopped but is no longer in foreground, thus at a lower priority.
        // While the service is in foreground, the associated notification can't be swipped away.
        // Moving it back to background allows the user to remove the notification.
        if (mMediaNotificationInfo.supportsSwipeAway() && mMediaNotificationInfo.isPaused) {
            ForegroundServiceUtils.getInstance().stopForeground(
                    mService, Service.STOP_FOREGROUND_DETACH);
            NotificationManagerProxy manager = new NotificationManagerProxyImpl(getContext());
            manager.notify(notification);
        } else if (!foregroundedService) {
            ForegroundServiceUtils.getInstance().startForeground(mService,
                    mMediaNotificationInfo.id, notification.getNotification(),
                    0 /*foregroundServiceType*/);
        }
        if (shouldLogNotification) {
            mNotificationUmaTracker.onNotificationShown(
                    NotificationUmaTracker.SystemNotificationType.MEDIA,
                    notification.getNotification());
        }
    }

    @VisibleForTesting
    void updateNotificationBuilder() {
        assert (mMediaNotificationInfo != null);
        NotificationMetadata metadata =
                new NotificationMetadata(NotificationUmaTracker.SystemNotificationType.MEDIA,
                        null /* notificationTag */, mMediaNotificationInfo.id);
        mNotificationBuilder = NotificationBuilderFactory.createChromeNotificationBuilder(
                true /* preferCompat */, ChannelDefinitions.ChannelId.MEDIA,
                null /* remoteAppPackageName*/, metadata);
        setMediaStyleLayoutForNotificationBuilder(mNotificationBuilder);

        // TODO(zqzhang): It's weird that setShowWhen() doesn't work on K. Calling setWhen() to
        // force removing the time.
        mNotificationBuilder.setShowWhen(false).setWhen(0);
        mNotificationBuilder.setSmallIcon(mMediaNotificationInfo.notificationSmallIcon);
        mNotificationBuilder.setAutoCancel(false);
        mNotificationBuilder.setLocalOnly(true);
        mNotificationBuilder.setGroup(getNotificationGroupName());
        mNotificationBuilder.setGroupSummary(true);

        if (mMediaNotificationInfo.supportsSwipeAway()) {
            mNotificationBuilder.setOngoing(!mMediaNotificationInfo.isPaused);
            mNotificationBuilder.setDeleteIntent(createPendingIntent(ListenerService.ACTION_SWIPE));
        }

        // The intent will currently only be null when using a custom tab.
        // TODO(avayvod) work out what we should do in this case. See https://crbug.com/585395.
        if (mMediaNotificationInfo.contentIntent != null) {
            mNotificationBuilder.setContentIntent(PendingIntent.getActivity(getContext(),
                    mMediaNotificationInfo.tabId, mMediaNotificationInfo.contentIntent,
                    PendingIntent.FLAG_UPDATE_CURRENT));
            // Set FLAG_UPDATE_CURRENT so that the intent extras is updated, otherwise the
            // intent extras will stay the same for the same tab.
        }

        mNotificationBuilder.setVisibility(
                mMediaNotificationInfo.isPrivate ? NotificationCompat.VISIBILITY_PRIVATE
                                                 : NotificationCompat.VISIBILITY_PUBLIC);
    }

    @VisibleForTesting
    void updateMediaSession() {
        if (!mMediaNotificationInfo.supportsPlayPause()) return;

        if (mMediaSession == null) mMediaSession = createMediaSession();

        activateAndroidMediaSession(mMediaNotificationInfo.tabId);

        try {
            // Tell the MediaRouter about the session, so that Chrome can control the volume
            // on the remote cast device (if any).
            // Pre-MR1 versions of JB do not have the complete MediaRouter APIs,
            // so getting the MediaRouter instance will throw an exception.
            MediaRouter.getInstance(getContext()).setMediaSessionCompat(mMediaSession);
        } catch (NoSuchMethodError e) {
            // Do nothing. Chrome can't be casting without a MediaRouter, so there is nothing
            // to do here.
        }

        mMediaSession.setMetadata(createMetadata());

        mMediaSession.setPlaybackState(createPlaybackState());
    }

    @VisibleForTesting
    PlaybackStateCompat createPlaybackState() {
        PlaybackStateCompat.Builder playbackStateBuilder =
                new PlaybackStateCompat.Builder().setActions(computeMediaSessionActions());

        int state = mMediaNotificationInfo.isPaused ? PlaybackStateCompat.STATE_PAUSED
                                                    : PlaybackStateCompat.STATE_PLAYING;

        if (mMediaNotificationInfo.mediaPosition != null) {
            playbackStateBuilder.setState(state, mMediaNotificationInfo.mediaPosition.getPosition(),
                    mMediaNotificationInfo.mediaPosition.getPlaybackRate(),
                    mMediaNotificationInfo.mediaPosition.getLastUpdatedTime());
        } else {
            playbackStateBuilder.setState(
                    state, PlaybackStateCompat.PLAYBACK_POSITION_UNKNOWN, 1.0f);
        }

        return playbackStateBuilder.build();
    }

    private long computeMediaSessionActions() {
        assert mMediaNotificationInfo != null;

        long actions = PlaybackStateCompat.ACTION_PLAY | PlaybackStateCompat.ACTION_PAUSE;
        if (mMediaNotificationInfo.mediaSessionActions.contains(
                    MediaSessionAction.PREVIOUS_TRACK)) {
            actions |= PlaybackStateCompat.ACTION_SKIP_TO_PREVIOUS;
        }
        if (mMediaNotificationInfo.mediaSessionActions.contains(MediaSessionAction.NEXT_TRACK)) {
            actions |= PlaybackStateCompat.ACTION_SKIP_TO_NEXT;
        }
        if (mMediaNotificationInfo.mediaSessionActions.contains(MediaSessionAction.SEEK_FORWARD)) {
            actions |= PlaybackStateCompat.ACTION_FAST_FORWARD;
        }
        if (mMediaNotificationInfo.mediaSessionActions.contains(MediaSessionAction.SEEK_BACKWARD)) {
            actions |= PlaybackStateCompat.ACTION_REWIND;
        }
        if (mMediaNotificationInfo.mediaSessionActions.contains(MediaSessionAction.SEEK_TO)) {
            actions |= PlaybackStateCompat.ACTION_SEEK_TO;
        }
        return actions;
    }

    private MediaSessionCompat createMediaSession() {
        Context context = getContext();
        MediaSessionCompat mediaSession =
                new MediaSessionCompat(context, context.getString(R.string.app_name),
                        new ComponentName(context, getButtonReceiverClass()), null);
        mediaSession.setFlags(MediaSessionCompat.FLAG_HANDLES_MEDIA_BUTTONS
                | MediaSessionCompat.FLAG_HANDLES_TRANSPORT_CONTROLS);
        mediaSession.setCallback(mMediaSessionCallback);

        // TODO(mlamouri): the following code is to work around a bug that hopefully
        // MediaSessionCompat will handle directly. see b/24051980.
        try {
            mediaSession.setActive(true);
        } catch (NullPointerException e) {
            // Some versions of KitKat do not support AudioManager.registerMediaButtonIntent
            // with a PendingIntent. They will throw a NullPointerException, in which case
            // they should be able to activate a MediaSessionCompat with only transport
            // controls.
            mediaSession.setActive(false);
            mediaSession.setFlags(MediaSessionCompat.FLAG_HANDLES_TRANSPORT_CONTROLS);
            mediaSession.setActive(true);
        }
        return mediaSession;
    }

    private void activateAndroidMediaSession(int tabId) {
        if (mMediaNotificationInfo == null) return;
        if (mMediaNotificationInfo.tabId != tabId) return;
        if (!mMediaNotificationInfo.supportsPlayPause() || mMediaNotificationInfo.isPaused) return;
        if (mMediaSession == null) return;
        mMediaSession.setActive(true);
    }

    private void setMediaStyleLayoutForNotificationBuilder(ChromeNotificationBuilder builder) {
        setMediaStyleNotificationText(builder);
        if (!mMediaNotificationInfo.supportsPlayPause()) {
            // Non-playback (Cast) notification will not use MediaStyle, so not
            // setting the large icon is fine.
            builder.setLargeIcon(null);
            // Notifications in incognito shouldn't show an icon to avoid leaking information.
        } else if (mMediaNotificationInfo.notificationLargeIcon != null
                && !mMediaNotificationInfo.isPrivate) {
            builder.setLargeIcon(mMediaNotificationInfo.notificationLargeIcon);
        } else if (!isRunningAtLeastN()) {
            if (mDefaultNotificationLargeIcon == null
                    && mMediaNotificationInfo.defaultNotificationLargeIcon != 0) {
                mDefaultNotificationLargeIcon = downscaleIconToIdealSize(
                        BitmapFactory.decodeResource(getContext().getResources(),
                                mMediaNotificationInfo.defaultNotificationLargeIcon));
            }
            builder.setLargeIcon(mDefaultNotificationLargeIcon);
        }

        addNotificationButtons(builder);
    }

    private void addNotificationButtons(ChromeNotificationBuilder builder) {
        Set<Integer> actions = new HashSet<>();

        // TODO(zqzhang): handle other actions when play/pause is not supported? See
        // https://crbug.com/667500
        if (mMediaNotificationInfo.supportsPlayPause()) {
            actions.addAll(mMediaNotificationInfo.mediaSessionActions);
            if (mMediaNotificationInfo.isPaused) {
                actions.remove(MediaSessionAction.PAUSE);
                actions.add(MediaSessionAction.PLAY);
            } else {
                actions.remove(MediaSessionAction.PLAY);
                actions.add(MediaSessionAction.PAUSE);
            }
        }

        if (mMediaNotificationInfo.supportsStop()) {
            actions.add(MediaSessionAction.STOP);
        } else {
            actions.remove(MediaSessionAction.STOP);
        }

        List<Integer> bigViewActions = computeBigViewActions(actions);

        for (int action : bigViewActions) {
            MediaButtonInfo buttonInfo = mActionToButtonInfo.get(action);
            builder.addAction(buttonInfo.iconResId,
                    getContext().getResources().getString(buttonInfo.descriptionResId),
                    createPendingIntent(buttonInfo.intentString));
        }

        // Only apply MediaStyle when NotificationInfo supports play/pause.
        if (mMediaNotificationInfo.supportsPlayPause()) {
            builder.setMediaStyle(mMediaSession, computeCompactViewActionIndices(bigViewActions),
                    createPendingIntent(ListenerService.ACTION_CANCEL), true);
        }
    }

    private Bitmap drawableToBitmap(Drawable drawable) {
        if (!(drawable instanceof BitmapDrawable)) return null;

        BitmapDrawable bitmapDrawable = (BitmapDrawable) drawable;
        return bitmapDrawable.getBitmap();
    }

    private void setMediaStyleNotificationText(ChromeNotificationBuilder builder) {
        if (mMediaNotificationInfo.isPrivate) {
            // Notifications in incognito shouldn't show what is playing to avoid leaking
            // information.
            if (isRunningAtLeastN()) {
                builder.setContentTitle(getContext().getResources().getString(
                        R.string.media_notification_incognito));
                builder.setSubText(
                        getContext().getResources().getString(R.string.notification_incognito_tab));
            } else {
                // App name is automatically added to the title from Android N,
                // but needs to be added explicitly for prior versions.
                builder.setContentTitle(getContext().getString(R.string.app_name))
                        .setContentText(getContext().getResources().getString(
                                R.string.media_notification_incognito));
            }
            return;
        }

        builder.setContentTitle(mMediaNotificationInfo.metadata.getTitle());
        String artistAndAlbumText = getArtistAndAlbumText(mMediaNotificationInfo.metadata);
        if (isRunningAtLeastN() || !artistAndAlbumText.isEmpty()) {
            builder.setContentText(artistAndAlbumText);
            builder.setSubText(mMediaNotificationInfo.origin);
        } else {
            // Leaving ContentText empty looks bad, so move origin up to the ContentText.
            builder.setContentText(mMediaNotificationInfo.origin);
        }
    }

    private String getArtistAndAlbumText(MediaMetadata metadata) {
        String artist = (metadata.getArtist() == null) ? "" : metadata.getArtist();
        String album = (metadata.getAlbum() == null) ? "" : metadata.getAlbum();
        if (artist.isEmpty() || album.isEmpty()) {
            return artist + album;
        }
        return artist + " - " + album;
    }

    /**
     * Compute the actions to be shown in BigView media notification.
     *
     * The method assumes STOP cannot coexist with switch track actions and seeking actions. It also
     * assumes PLAY and PAUSE cannot coexist.
     */
    private List<Integer> computeBigViewActions(Set<Integer> actions) {
        // STOP cannot coexist with switch track actions and seeking actions.
        assert !actions.contains(MediaSessionAction.STOP)
                || !(actions.contains(MediaSessionAction.PREVIOUS_TRACK)
                        && actions.contains(MediaSessionAction.NEXT_TRACK)
                        && actions.contains(MediaSessionAction.SEEK_BACKWARD)
                        && actions.contains(MediaSessionAction.SEEK_FORWARD));
        // PLAY and PAUSE cannot coexist.
        assert !actions.contains(MediaSessionAction.PLAY)
                || !actions.contains(MediaSessionAction.PAUSE);

        int[] actionByOrder = {
                MediaSessionAction.PREVIOUS_TRACK,
                MediaSessionAction.SEEK_BACKWARD,
                MediaSessionAction.PLAY,
                MediaSessionAction.PAUSE,
                MediaSessionAction.SEEK_FORWARD,
                MediaSessionAction.NEXT_TRACK,
                MediaSessionAction.STOP,
        };

        // Sort the actions based on the expected ordering in the UI.
        List<Integer> sortedActions = new ArrayList<>();
        for (int action : actionByOrder) {
            if (actions.contains(action)) sortedActions.add(action);
        }

        // There can't be move actions than BIG_VIEW_ACTIONS_COUNT. We do this check after we have
        // sorted the actions since there may be more actions that we do not support.
        assert sortedActions.size() <= BIG_VIEW_ACTIONS_COUNT;

        return sortedActions;
    }

    /**
     * Compute the actions to be shown in CompactView media notification.
     *
     * The method assumes STOP cannot coexist with switch track actions and seeking actions. It also
     * assumes PLAY and PAUSE cannot coexist.
     *
     * Actions in pairs are preferred if there are more actions than |COMPACT_VIEW_ACTIONS_COUNT|.
     */
    @VisibleForTesting
    static int[] computeCompactViewActionIndices(List<Integer> actions) {
        // STOP cannot coexist with switch track actions and seeking actions.
        assert !actions.contains(MediaSessionAction.STOP)
                || !(actions.contains(MediaSessionAction.PREVIOUS_TRACK)
                        && actions.contains(MediaSessionAction.NEXT_TRACK)
                        && actions.contains(MediaSessionAction.SEEK_BACKWARD)
                        && actions.contains(MediaSessionAction.SEEK_FORWARD));
        // PLAY and PAUSE cannot coexist.
        assert !actions.contains(MediaSessionAction.PLAY)
                || !actions.contains(MediaSessionAction.PAUSE);

        if (actions.size() <= COMPACT_VIEW_ACTIONS_COUNT) {
            // If the number of actions is less than |COMPACT_VIEW_ACTIONS_COUNT|, just return an
            // array of 0, 1, ..., |actions.size()|-1.
            int[] actionsArray = new int[actions.size()];
            for (int i = 0; i < actions.size(); ++i) actionsArray[i] = i;
            return actionsArray;
        }

        if (actions.contains(MediaSessionAction.STOP)) {
            List<Integer> compactActions = new ArrayList<>();
            if (actions.contains(MediaSessionAction.PLAY)) {
                compactActions.add(actions.indexOf(MediaSessionAction.PLAY));
            }
            compactActions.add(actions.indexOf(MediaSessionAction.STOP));
            return CollectionUtil.integerListToIntArray(compactActions);
        }

        int[] actionsArray = new int[COMPACT_VIEW_ACTIONS_COUNT];
        if (actions.contains(MediaSessionAction.PREVIOUS_TRACK)
                && actions.contains(MediaSessionAction.NEXT_TRACK)) {
            actionsArray[0] = actions.indexOf(MediaSessionAction.PREVIOUS_TRACK);
            if (actions.contains(MediaSessionAction.PLAY)) {
                actionsArray[1] = actions.indexOf(MediaSessionAction.PLAY);
            } else {
                actionsArray[1] = actions.indexOf(MediaSessionAction.PAUSE);
            }
            actionsArray[2] = actions.indexOf(MediaSessionAction.NEXT_TRACK);
            return actionsArray;
        }

        assert actions.contains(MediaSessionAction.SEEK_BACKWARD)
                && actions.contains(MediaSessionAction.SEEK_FORWARD);
        actionsArray[0] = actions.indexOf(MediaSessionAction.SEEK_BACKWARD);
        if (actions.contains(MediaSessionAction.PLAY)) {
            actionsArray[1] = actions.indexOf(MediaSessionAction.PLAY);
        } else {
            actionsArray[1] = actions.indexOf(MediaSessionAction.PAUSE);
        }
        actionsArray[2] = actions.indexOf(MediaSessionAction.SEEK_FORWARD);

        return actionsArray;
    }

    private static Context getContext() {
        return ContextUtils.getApplicationContext();
    }
}
