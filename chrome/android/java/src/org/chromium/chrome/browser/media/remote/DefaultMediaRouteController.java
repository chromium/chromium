// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.remote;

import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.net.Uri;
import android.os.Bundle;
import android.os.SystemClock;
import android.support.v7.media.MediaControlIntent;
import android.support.v7.media.MediaItemMetadata;
import android.support.v7.media.MediaItemStatus;
import android.support.v7.media.MediaRouteSelector;
import android.support.v7.media.MediaRouter;
import android.support.v7.media.MediaRouter.RouteInfo;
import android.support.v7.media.MediaSessionStatus;

import com.google.android.gms.cast.CastMediaControlIntent;

import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.Log;
import org.chromium.base.annotations.RemovableInRelease;
import org.chromium.base.annotations.UsedByReflection;
import org.chromium.base.task.AsyncTask;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.media.remote.RemoteVideoInfo.PlayerState;

import java.net.URI;
import java.net.URISyntaxException;

import javax.annotation.Nullable;

/**
 * Class that abstracts all communication to and from the Android MediaRoutes. This class is
 * responsible for connecting to the MRs as well as sending commands and receiving status updates
 * from the remote player.
 *
 *  We have two main scenarios for Cast:
 *
 *  - the first cast: user plays the first video on the Chromecast so we start a new session with
 * the player and fling the video
 *
 *  - the consequent cast: users plays another video while the previous one is still playing
 * remotely meaning that we don't have to start the session but to replace the current video with
 * the new one
 *
 *  Casting the first video takes two intents sent to the selected media route:
 * ACTION_START_SESSION and ACTION_PLAY. The first one is sent before anything else. We get the
 * session id from the result bundle of the intent but need to wait until the session becomes
 * active before sending the video URL via the ACTION_PLAY intent.
 *
 *  Casting the second video to the same target device only takes one ACTION_PLAY intent if
 * the session is still active. Otherwise, the scenario is the same as for the first video.
 */
@UsedByReflection("RemoteMediaPlayerController.java")
public class DefaultMediaRouteController extends AbstractMediaRouteController {

    /**
     * Interface for MediaRouter intents result handlers.
     */
    protected interface ResultBundleHandler {
        void onResult(Bundle data);

        void onError(String message, Bundle data);
    }

    private static final String TAG = "MediaFling";

    private static final String ACTION_RECEIVE_SESSION_STATUS_UPDATE =
            "com.google.android.apps.chrome.videofling.RECEIVE_SESSION_STATUS_UPDATE";
    private static final String ACTION_RECEIVE_MEDIA_STATUS_UPDATE =
            "com.google.android.apps.chrome.videofling.RECEIVE_MEDIA_STATUS_UPDATE";
    private static final String MIME_TYPE = "video/mp4";
    private String mCurrentSessionId;
    private String mCurrentItemId;
    private boolean mSeeking;
    private final String mIntentCategory;
    private PendingIntent mSessionStatusUpdateIntent;
    private BroadcastReceiver mSessionStatusBroadcastReceiver;
    private PendingIntent mMediaStatusUpdateIntent;
    private BroadcastReceiver mMediaStatusBroadcastReceiver;

    private String mPreferredTitle;
    private long mStartPositionMillis;
    private final PositionExtrapolator mPositionExtrapolator = new PositionExtrapolator();

    private Uri mLocalVideoUri;

    private int mSessionState = MediaSessionStatus.SESSION_STATE_INVALIDATED;

    private final ApplicationStatus.ApplicationStateListener mApplicationStateListener =
            new ApplicationStatus.ApplicationStateListener() {
                @Override
                public void onApplicationStateChange(int newState) {
                    // HAS_DESTROYED_ACTIVITIES means all Chrome activities have been destroyed.
                    if (newState == ApplicationState.HAS_DESTROYED_ACTIVITIES) {
                        onActivitiesDestroyed();
                    }
                }
            };

    /**
     * Default and only constructor.
     */
    public DefaultMediaRouteController() {
        mIntentCategory = getContext().getPackageName();
    }

    @Override
    public boolean initialize() {
        if (mediaRouterInitializationFailed()) return false;

        ApplicationStatus.registerApplicationStateListener(mApplicationStateListener);

        if (mSessionStatusUpdateIntent == null) {
            Intent sessionStatusUpdateIntent = new Intent(ACTION_RECEIVE_SESSION_STATUS_UPDATE);
            sessionStatusUpdateIntent.addCategory(mIntentCategory);
            mSessionStatusUpdateIntent = PendingIntent.getBroadcast(getContext(), 0,
                    sessionStatusUpdateIntent, PendingIntent.FLAG_UPDATE_CURRENT);
        }

        if (mMediaStatusUpdateIntent == null) {
            Intent mediaStatusUpdateIntent = new Intent(ACTION_RECEIVE_MEDIA_STATUS_UPDATE);
            mediaStatusUpdateIntent.addCategory(mIntentCategory);
            mMediaStatusUpdateIntent = PendingIntent.getBroadcast(getContext(), 0,
                    mediaStatusUpdateIntent, PendingIntent.FLAG_UPDATE_CURRENT);
        }

        return true;
    }

    @Override
    public boolean canPlayMedia(String sourceUrl, String frameUrl) {
        if (mediaRouterInitializationFailed() || sourceUrl == null) return false;

        try {
            String scheme = new URI(sourceUrl).getScheme();
            if (scheme == null) return false;
            return scheme.equals(UrlConstants.HTTP_SCHEME)
                    || scheme.equals(UrlConstants.HTTPS_SCHEME);
        } catch (URISyntaxException e) {
            return false;
        }
    }

    @Override
    public void setRemoteVolume(int delta) {
        boolean canChangeRemoteVolume = (getCurrentRoute().getVolumeHandling()
                == MediaRouter.RouteInfo.PLAYBACK_VOLUME_VARIABLE);
        if (currentRouteSupportsRemotePlayback() && canChangeRemoteVolume) {
            getCurrentRoute().requestUpdateVolume(delta);
        }
    }

    @Override
    public MediaRouteSelector buildMediaRouteSelector() {
        return new MediaRouteSelector.Builder().addControlCategory(
                CastMediaControlIntent.categoryForRemotePlayback(getCastReceiverId())).build();
    }

    protected String getCastReceiverId() {
        return CastMediaControlIntent.DEFAULT_MEDIA_RECEIVER_APPLICATION_ID;
    }

    @Override
    public void resume() {
        if (mCurrentItemId == null) return;

        Intent intent = new Intent(MediaControlIntent.ACTION_RESUME);
        intent.addCategory(MediaControlIntent.CATEGORY_REMOTE_PLAYBACK);
        intent.putExtra(MediaControlIntent.EXTRA_SESSION_ID, mCurrentSessionId);
        sendIntentToRoute(intent, new ResultBundleHandler() {
            @Override
            public void onResult(Bundle data) {
                processMediaStatusBundle(data);
            }

            @Override
            public void onError(String message, Bundle data) {
                release();
            }
        });

        setDisplayedPlayerState(PlayerState.LOADING);
    }

    @Override
    public void pause() {
        if (mCurrentItemId == null) return;

        Intent intent = new Intent(MediaControlIntent.ACTION_PAUSE);
        intent.addCategory(MediaControlIntent.CATEGORY_REMOTE_PLAYBACK);
        intent.putExtra(MediaControlIntent.EXTRA_SESSION_ID, mCurrentSessionId);
        sendIntentToRoute(intent, new ResultBundleHandler() {
            @Override
            public void onResult(Bundle data) {
                processMediaStatusBundle(data);
            }

            @Override
            public void onError(String message, Bundle data) {
                // Do not release the player just because of a failed pause
                // request. This can happen when pausing more than once for
                // example.
            }
        });

        // Update the last known position to the current one so that we don't
        // jump back in time discarding whatever we extrapolated from the last
        // time the position was updated.
        mPositionExtrapolator.onPaused();
        setDisplayedPlayerState(PlayerState.PAUSED);
    }

    /**
     * Plays the given Uri on the currently selected player. This will replace any currently playing
     * video
     *
     * @param preferredTitle the preferred title of the current playback session to display
     * @param startPositionMillis from which to start playing.
     */
    private void playUri(@Nullable final String preferredTitle, final long startPositionMillis) {
        RecordCastAction.castMediaType(MediaUrlResolver.getMediaType(mLocalVideoUri));
        installBroadcastReceivers();

        // If the session is already started (meaning we are casting a video already), we simply
        // load the new URL with one ACTION_PLAY intent.
        if (mCurrentSessionId != null) {
            Log.d(TAG, "Playing a new url: %s", mLocalVideoUri);

            // We keep the same session so only clear the playing item status.
            clearItemState();
            startPlayback(preferredTitle, startPositionMillis);
            return;
        }

        Log.d(TAG, "Sending stream to app: %s", getCastReceiverId());
        Log.d(TAG, "Url: %s", mLocalVideoUri);

        startSession(true, null, new ResultBundleHandler() {
            @Override
            public void onResult(Bundle data) {
                configureNewSession(data);

                mPreferredTitle = preferredTitle;
                updateTitle(mPreferredTitle);
                mStartPositionMillis = startPositionMillis;
                // Make sure we get a session status. If the session becomes active
                // immediately then the broadcast session status can arrive before we have
                // the session id, so this ensures we get it whatever happens.
                getSessionStatus(mCurrentSessionId);
            }

            @Override
            public void onError(String message, Bundle data) {
                release();
                RecordCastAction.castDefaultPlayerResult(false);
            }
        });
    }

    /**
     * Send a start session intent.
     *
     * @param relaunch Whether we should relaunch the cast application.
     * @param resultBundleHandler BundleHandler to handle reply.
     */
    private void startSession(boolean relaunch, String sessionId,
            ResultBundleHandler resultBundleHandler) {
        Intent intent = new Intent(MediaControlIntent.ACTION_START_SESSION);
        intent.addCategory(MediaControlIntent.CATEGORY_REMOTE_PLAYBACK);

        intent.putExtra(CastMediaControlIntent.EXTRA_CAST_STOP_APPLICATION_WHEN_SESSION_ENDS, true);
        intent.putExtra(MediaControlIntent.EXTRA_SESSION_STATUS_UPDATE_RECEIVER,
                mSessionStatusUpdateIntent);
        intent.putExtra(CastMediaControlIntent.EXTRA_CAST_APPLICATION_ID, getCastReceiverId());
        intent.putExtra(CastMediaControlIntent.EXTRA_CAST_RELAUNCH_APPLICATION, relaunch);
        if (sessionId != null) intent.putExtra(MediaControlIntent.EXTRA_SESSION_ID, sessionId);

        addIntentExtraForDebugLogging(intent);
        sendIntentToRoute(intent, resultBundleHandler);
    }

    @RemovableInRelease
    private void addIntentExtraForDebugLogging(Intent intent) {
        if (Log.isLoggable(TAG, Log.DEBUG)) {
            intent.putExtra(CastMediaControlIntent.EXTRA_DEBUG_LOGGING_ENABLED, true);
        }
    }

    private void getSessionStatus(String sessionId) {
        Intent intent = new Intent(MediaControlIntent.ACTION_GET_SESSION_STATUS);
        intent.addCategory(MediaControlIntent.CATEGORY_REMOTE_PLAYBACK);

        intent.putExtra(MediaControlIntent.EXTRA_SESSION_ID, sessionId);

        sendIntentToRoute(intent, new ResultBundleHandler() {
            @Override
            public void onResult(Bundle data) {
                logBundle("getSessionStatus result :", data);
                processSessionStatusBundle(data);
            }

            @Override
            public void onError(String message, Bundle data) {
                release();
            }
        });
    }

    private void startPlayback(
            @Nullable final String preferredTitle, final long startPositionMillis) {
        setUnprepared();
        Intent intent = new Intent(MediaControlIntent.ACTION_PLAY);
        intent.addCategory(MediaControlIntent.CATEGORY_REMOTE_PLAYBACK);
        intent.setDataAndType(mLocalVideoUri, MIME_TYPE);
        intent.putExtra(MediaControlIntent.EXTRA_SESSION_ID, mCurrentSessionId);
        intent.putExtra(MediaControlIntent.EXTRA_ITEM_STATUS_UPDATE_RECEIVER,
                mMediaStatusUpdateIntent);
        intent.putExtra(MediaControlIntent.EXTRA_ITEM_CONTENT_POSITION, startPositionMillis);

        if (preferredTitle != null) {
            Bundle metadata = new Bundle();
            metadata.putString(MediaItemMetadata.KEY_TITLE, preferredTitle);
            intent.putExtra(MediaControlIntent.EXTRA_ITEM_METADATA, metadata);
        }

        sendIntentToRoute(intent, new ResultBundleHandler() {
            @Override
            public void onResult(Bundle data) {
                mCurrentItemId = data.getString(MediaControlIntent.EXTRA_ITEM_ID);
                processMediaStatusBundle(data);
                RecordCastAction.castDefaultPlayerResult(true);
            }

            @Override
            public void onError(String message, Bundle data) {
                release();
                RecordCastAction.castDefaultPlayerResult(false);
            }
        });
    }

    @Override
    public long getPosition() {
        return mPositionExtrapolator.getPosition();
    }

    @Override
    public long getDuration() {
        return mPositionExtrapolator.getDuration();
    }

    @Override
    public void seekTo(long msec) {
        if (msec == getPosition()) return;
        // Update the position now since the MRP will update it only once the video is playing
        // remotely. In particular, if the video is paused, the MRP doesn't send the command until
        // the video is resumed.
        mPositionExtrapolator.onSeek(msec);
        mSeeking = true;
        Intent intent = new Intent(MediaControlIntent.ACTION_SEEK);
        intent.addCategory(MediaControlIntent.CATEGORY_REMOTE_PLAYBACK);
        intent.putExtra(MediaControlIntent.EXTRA_SESSION_ID, mCurrentSessionId);
        intent.putExtra(MediaControlIntent.EXTRA_ITEM_ID, mCurrentItemId);
        intent.putExtra(MediaControlIntent.EXTRA_ITEM_CONTENT_POSITION, msec);
        sendIntentToRoute(intent, new ResultBundleHandler() {
            @Override
            public void onResult(Bundle data) {
                if (getMediaStateListener() != null) getMediaStateListener().onSeekCompleted();
                processMediaStatusBundle(data);
            }

            @Override
            public void onError(String message, Bundle data) {
                release();
            }
        });
    }

    @Override
    public void release() {
        super.release();

        for (UiListener listener : getUiListeners()) {
            listener.onRouteUnselected(this);
        }
        if (getMediaStateListener() != null) getMediaStateListener().onRouteUnselected();
        setMediaStateListener(null);

        if (mediaRouterInitializationFailed()) return;
        if (mCurrentSessionId == null) {
            // This can happen if we disconnect after a failure (because the
            // media could not be casted).
            disconnect();
            return;
        }

        Intent stopIntent = new Intent(MediaControlIntent.ACTION_STOP);
        stopIntent.addCategory(MediaControlIntent.CATEGORY_REMOTE_PLAYBACK);
        stopIntent.putExtra(MediaControlIntent.EXTRA_SESSION_ID, mCurrentSessionId);

        sendIntentToRoute(stopIntent, new ResultBundleHandler() {
            @Override
            public void onResult(Bundle data) {
                processMediaStatusBundle(data);
            }

            @Override
            public void onError(String message, Bundle data) {}
        });

        Intent endSessionIntent = new Intent(MediaControlIntent.ACTION_END_SESSION);
        endSessionIntent.addCategory(MediaControlIntent.CATEGORY_REMOTE_PLAYBACK);
        endSessionIntent.putExtra(MediaControlIntent.EXTRA_SESSION_ID, mCurrentSessionId);

        sendIntentToRoute(endSessionIntent, new ResultBundleHandler() {
            @Override
            public void onResult(Bundle data) {
                logMediaSessionStatus(data);

                for (UiListener listener : getUiListeners()) {
                    listener.onPlaybackStateChanged(PlayerState.FINISHED);
                }

                if (getMediaStateListener() != null) {
                    getMediaStateListener().onPlaybackStateChanged(PlayerState.FINISHED);
                }
                recordRemainingTimeUMA();
                disconnect();
            }

            @Override
            public void onError(String message, Bundle data) {
                disconnect();
            }
        });
    }

    /**
     * Disconnect from the remote screen without stopping the media playing. use release() for
     * disconnect + stop.
     */
    private void disconnect() {
        clearStreamState();
        clearMediaRoute();

        if (mSessionStatusBroadcastReceiver != null) {
            getContext().unregisterReceiver(mSessionStatusBroadcastReceiver);
            mSessionStatusBroadcastReceiver = null;
        }
        if (mMediaStatusBroadcastReceiver != null) {
            getContext().unregisterReceiver(mMediaStatusBroadcastReceiver);
            mMediaStatusBroadcastReceiver = null;
        }
        clearConnectionFailureCallback();

        stopWatchingRouteSelection();
        removeAllListeners();
    }

    @Override
    protected void onRouteSelectedEvent(MediaRouter router, RouteInfo route) {
        Log.d(TAG, "Selected route %s", route);
        if (!route.isSelected()) return;

        RecordCastAction.castPlayRequested();

        RecordCastAction.remotePlaybackDeviceSelected(RecordCastAction.DeviceType.CAST_GENERIC);
        installBroadcastReceivers();

        if (getMediaStateListener() == null) {
            showCastError(route.getName());
            release();
            return;
        }

        if (route != getCurrentRoute()) {
            registerRoute(route);
            clearStreamState();
        }
        mPositionExtrapolator.clear();

        notifyRouteSelected(route);
    }

    /*
     * Although our custom implementation of the disconnect button doesn't need this, it is needed
     * when the route is released due to, for example, another application stealing the route, or
     * when we switch to a YouTube video on the same device.
     */
    @Override
    protected void onRouteUnselectedEvent(MediaRouter router, RouteInfo route) {
        Log.d(TAG, "Unselected route %s", route);
        // Preserve our best guess as to the final position; this is needed to reset the
        // local position while switching back to local playback.
        mPositionExtrapolator.onPaused();
        if (getCurrentRoute() != null && route.getId().equals(getCurrentRoute().getId())) {
            clearStreamState();
        }
    }

    private void installBroadcastReceivers() {
        if (mSessionStatusBroadcastReceiver == null) {
            mSessionStatusBroadcastReceiver = new BroadcastReceiver() {
                @Override
                public void onReceive(Context context, Intent intent) {
                    logIntent("Got a session broadcast intent from the MRP: ", intent);
                    Bundle statusBundle = intent.getExtras();

                    // Ignore null status bundles.
                    if (statusBundle == null) return;

                    // Ignore the status of old sessions.
                    String sessionId = statusBundle.getString(MediaControlIntent.EXTRA_SESSION_ID);
                    if (mCurrentSessionId == null || !mCurrentSessionId.equals(sessionId)) return;

                    processSessionStatusBundle(statusBundle);
                }
            };
            IntentFilter sessionBroadcastIntentFilter =
                    new IntentFilter(ACTION_RECEIVE_SESSION_STATUS_UPDATE);
            sessionBroadcastIntentFilter.addCategory(mIntentCategory);
            getContext().registerReceiver(mSessionStatusBroadcastReceiver,
                    sessionBroadcastIntentFilter);
        }

        if (mMediaStatusBroadcastReceiver == null) {
            mMediaStatusBroadcastReceiver = new BroadcastReceiver() {
                @Override
                public void onReceive(Context context, Intent intent) {
                    logIntent("Got a broadcast intent from the MRP: ", intent);

                    processMediaStatusBundle(intent.getExtras());
                }
            };
            IntentFilter mediaBroadcastIntentFilter =
                    new IntentFilter(ACTION_RECEIVE_MEDIA_STATUS_UPDATE);
            mediaBroadcastIntentFilter.addCategory(mIntentCategory);
            getContext().registerReceiver(mMediaStatusBroadcastReceiver,
                    mediaBroadcastIntentFilter);
        }
    }

    /**
     * Called when the main activity receives an onDestroy() call.
     */
    protected void onActivitiesDestroyed() {
        ApplicationStatus.unregisterApplicationStateListener(mApplicationStateListener);
        release();
    }

    /**
     * Clear the session and the currently playing item (if any).
     */
    protected void clearStreamState() {
        mLocalVideoUri = null;
        mCurrentSessionId = null;
        clearItemState();
    }

    @Override
    protected void clearItemState() {
        // Note: do not clear the stream position, since this is still needed so
        // that we can reset the local stream position to match.
        super.clearItemState();
        mCurrentItemId = null;
        mPositionExtrapolator.clear();
        mSeeking = false;
    }

    private void processSessionStatusBundle(Bundle statusBundle) {
        MediaSessionStatus status = MediaSessionStatus.fromBundle(
                statusBundle.getBundle(MediaControlIntent.EXTRA_SESSION_STATUS));
        int sessionState = status.getSessionState();

        // If no change do nothing
        if (sessionState == mSessionState) return;
        mSessionState = sessionState;

        switch (sessionState) {
            case MediaSessionStatus.SESSION_STATE_ACTIVE:
                if (mLocalVideoUri != null) {
                    startPlayback(mPreferredTitle, mStartPositionMillis);
                }
                break;

            case MediaSessionStatus.SESSION_STATE_ENDED:
            case MediaSessionStatus.SESSION_STATE_INVALIDATED:
                for (UiListener listener : getUiListeners()) {
                    listener.onPlaybackStateChanged(PlayerState.INVALIDATED);
                }
                if (getMediaStateListener() != null) {
                    getMediaStateListener().onPlaybackStateChanged(PlayerState.INVALIDATED);
                }
                // Record the remaining time UMA first, otherwise the playback state will be cleared
                // in release().
                recordRemainingTimeUMA();
                // Set the current session id to null so we don't send the stop intent.
                mCurrentSessionId = null;
                release();
                break;

            default:
                break;
        }
    }

    private void processMediaStatusBundle(Bundle statusBundle) {
        if (statusBundle == null) return;
        logBundle("processMediaStatusBundle: ", statusBundle);

        String itemId = statusBundle.getString(MediaControlIntent.EXTRA_ITEM_ID);
        if (itemId == null || !itemId.equals(mCurrentItemId)) return;

        // Extract item metadata, if available.
        if (statusBundle.containsKey(MediaControlIntent.EXTRA_ITEM_METADATA)) {
            Bundle metadataBundle =
                    (Bundle) statusBundle.getParcelable(MediaControlIntent.EXTRA_ITEM_METADATA);
            updateTitle(metadataBundle.getString(MediaItemMetadata.KEY_TITLE, mPreferredTitle));
        }

        // Extract the item status, if available.
        if (statusBundle.containsKey(MediaControlIntent.EXTRA_ITEM_STATUS)) {
            Bundle itemStatusBundle =
                    (Bundle) statusBundle.getParcelable(MediaControlIntent.EXTRA_ITEM_STATUS);
            MediaItemStatus itemStatus = MediaItemStatus.fromBundle(itemStatusBundle);

            logBundle("Received item status: ", itemStatusBundle);

            updateState(itemStatus.getPlaybackState());

            // Update the PositionExtrapolator that the playback state has changed.
            if (itemStatus.getPlaybackState() == MediaItemStatus.PLAYBACK_STATE_PLAYING) {
                mPositionExtrapolator.onResumed();
            } else if (itemStatus.getPlaybackState() == MediaItemStatus.PLAYBACK_STATE_FINISHED) {
                mPositionExtrapolator.onFinished();
            } else {
                mPositionExtrapolator.onPaused();
            }

            if ((getRemotePlayerState() == PlayerState.PAUSED)
                    || (getRemotePlayerState() == PlayerState.PLAYING)
                    || (getRemotePlayerState() == PlayerState.LOADING)) {
                this.mCurrentItemId = itemId;

                // duration can possibly be -1 if it's unknown, so cap to 0
                long duration = Math.max(itemStatus.getContentDuration(), 0);
                // update the position using the remote player's position
                // duration can possibly be -1 if it's unknown, so cap to 0
                long position = Math.min(Math.max(itemStatus.getContentPosition(), 0), duration);
                // TODO(zqzhang): The GMS core currently uses SystemClock.uptimeMillis() as
                // timestamp, which does not conform to the MediaRouter support library docs. See
                // b/28378525 and
                // http://developer.android.com/reference/android/support/v7/media/MediaItemStatus.html#getTimestamp().
                // Override the timestamp with elapsedRealtime() by assuming the delay between the
                // GMS core produces the MediaItemStatus and the code reaches here is short enough.
                // long timestamp = itemStatus.getTimestamp();
                long timestamp = SystemClock.elapsedRealtime();
                notifyDurationUpdated(duration);
                notifyPositionUpdated(position);
                mPositionExtrapolator.onPositionInfoUpdated(duration, position, timestamp);

                if (mSeeking) {
                    mSeeking = false;
                    if (getMediaStateListener() != null) getMediaStateListener().onSeekCompleted();
                }
            }
            logExtraHttpInfo(itemStatus.getExtras());
        }
    }

    /**
     * Send the given intent to the current route. The result will be returned in the given
     * ResultBundleHandler. This function will also check to see if the current route can handle the
     * intent before sending it.
     *
     * @param intent the intent to send to the current route.
     * @param bundleHandler contains the result of sending the intent
     */
    private void sendIntentToRoute(final Intent intent, final ResultBundleHandler bundleHandler) {
        if (getCurrentRoute() == null) {
            logIntent("sendIntentToRoute ", intent);
            Log.d(TAG, "The current route is null.");
            if (bundleHandler != null) bundleHandler.onError(null, null);
            return;
        }

        if (!getCurrentRoute().supportsControlRequest(intent)) {
            logIntent("sendIntentToRoute ", intent);
            Log.d(TAG, "The intent is not supported by the route: %s", getCurrentRoute());
            if (bundleHandler != null) bundleHandler.onError(null, null);
            return;
        }

        sendControlIntent(intent, bundleHandler);
    }

    private void sendControlIntent(final Intent intent, final ResultBundleHandler bundleHandler) {
        Log.d(TAG, "Sending intent to %s %s", getCurrentRoute().getName(),
                getCurrentRoute().getId());
        logIntent("sendControlIntent ", intent);
        if (getCurrentRoute().isDefault()) {
            Log.d(TAG, "Route is default, not sending");
            return;
        }

        getCurrentRoute().sendControlRequest(intent, new MediaRouter.ControlRequestCallback() {
            @Override
            public void onResult(Bundle data) {
                if (data != null && bundleHandler != null) bundleHandler.onResult(data);
            }

            @Override
            public void onError(String message, Bundle data) {
                logControlRequestError(intent, message, data);
                int errorCode = 0;
                if (data != null) {
                    errorCode = data.getInt(CastMediaControlIntent.EXTRA_ERROR_CODE);
                }

                sendErrorToListeners(errorCode);

                if (bundleHandler != null) bundleHandler.onError(message, data);
            }
        });
    }

    private void notifyDurationUpdated(long durationMillis) {
        for (UiListener listener : getUiListeners()) {
            listener.onDurationUpdated(durationMillis);
        }
    }

    private void notifyPositionUpdated(long position) {
        for (UiListener listener : getUiListeners()) {
            listener.onPositionChanged(position);
        }
    }

    private void recordRemainingTimeUMA() {
        long duration = getDuration();
        long remainingTime = Math.max(0, duration - getPosition());
        // Duration has already been cleared.
        if (getDuration() <= 0) return;
        RecordCastAction.castEndedTimeRemaining(duration, remainingTime);
    }

    private String bundleToString(Bundle bundle) {
        if (bundle == null) return "";

        StringBuilder extras = new StringBuilder();
        extras.append("[");
        for (String key : bundle.keySet()) {
            Object value = bundle.get(key);
            String valueText = value == null ? "null" : value.toString();
            if (value instanceof Bundle) valueText = bundleToString((Bundle) value);
            extras.append(key).append("=").append(valueText).append(",");
        }
        extras.append("]");
        return extras.toString();
    }

    @Override
    protected void startCastingVideo() {
        MediaStateListener listener = getMediaStateListener();
        if (listener == null) return;

        String url = listener.getSourceUrl();

        Log.d(TAG, "startCastingVideo called, url = %s", url);

        // checkIfPlayableRemotely will have rejected null URLs.
        assert url != null;

        RecordCastAction.castDomainAndRegistry(listener.getFrameUrl().toString());

        mLocalVideoUri = Uri.parse(url);
        mStartPositionMillis = listener.getStartPositionMillis();
        playUri(listener.getTitle(), mStartPositionMillis);
    }

    private void configureNewSession(Bundle data) {
        mCurrentSessionId = data.getString(MediaControlIntent.EXTRA_SESSION_ID);
        mSessionState = MediaSessionStatus.SESSION_STATE_INVALIDATED;
        Log.d(TAG, "Got a session id: %s", mCurrentSessionId);
    }

    @Override
    public void checkIfPlayableRemotely(final String sourceUrl, final String frameUrl,
            final String cookies, String userAgent, final MediaValidationCallback callback) {
        new MediaUrlResolver(new MediaUrlResolver.Delegate() {

            @Override
            public Uri getUri() {
                return Uri.parse(sourceUrl);
            }

            @Override
            public String getCookies() {
                return cookies;
            }

            @Override
            public void deliverResult(Uri uri, boolean playable) {
                callback.onResult(playable, uri.toString(), frameUrl);
            }

            // Some webpages have >100 media files, which causes the THREAD_POOL_EXECUTOR to get
            // flooded with requests and causes a crash. We use SERIAL_EXECUTOR instead, to allow
            // for an unlimited number of tasks. See https://crbug.com/873941.
        }, userAgent).executeOnExecutor(AsyncTask.SERIAL_EXECUTOR);
    }

    @Override
    public String getUriPlaying() {
        if (mLocalVideoUri == null) return null;
        return mLocalVideoUri.toString();
    }

    @RemovableInRelease
    private void logBundle(String message, Bundle bundle) {
        Log.d(TAG, message + bundleToString(bundle));
    }

    @RemovableInRelease
    private void logControlRequestError(Intent intent, String message, Bundle data) {
        // The intent may contain some PII so we don't want to log it in the released
        // version by default.
        Log.e(TAG, String.format(
                "Error sending control request %s %s. Data: %s Error: %s", intent,
                bundleToString(intent.getExtras()), bundleToString(data), message));
    }

    @RemovableInRelease
    private void logExtraHttpInfo(Bundle extras) {
        if (extras != null) {
            if (extras.containsKey(MediaItemStatus.EXTRA_HTTP_STATUS_CODE)) {
                int httpStatus = extras.getInt(MediaItemStatus.EXTRA_HTTP_STATUS_CODE);
                Log.d(TAG, "HTTP status: %s", httpStatus);
            }
            if (extras.containsKey(MediaItemStatus.EXTRA_HTTP_RESPONSE_HEADERS)) {
                Bundle headers = extras.getBundle(MediaItemStatus.EXTRA_HTTP_RESPONSE_HEADERS);
                Log.d(TAG, "HTTP headers: %s", bundleToString(headers));
            }
        }
    }

    @RemovableInRelease
    private void logIntent(String prefix, Intent intent) {
        Log.d(TAG, prefix + intent + " extras: " + bundleToString(intent.getExtras()));
    }

    @RemovableInRelease
    private void logMediaSessionStatus(Bundle data) {
        MediaSessionStatus status = MediaSessionStatus.fromBundle(
                data.getBundle(MediaControlIntent.EXTRA_SESSION_STATUS));
        int sessionState = status.getSessionState();
        Log.d(TAG, "Session state after ending session: %s", sessionState);
    }
}
