// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.remote;

import android.graphics.Bitmap;

import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.blink_public.platform.modules.remoteplayback.WebRemotePlaybackAvailability;
import org.chromium.chrome.browser.media.remote.RemoteVideoInfo.PlayerState;

/**
 * Acts as a proxy between the remotely playing video and the HTMLMediaElement.
 */
@JNINamespace("remote_media")
public class RemoteMediaPlayerBridge {
    private long mStartPositionMillis;
    private long mNativeRemoteMediaPlayerBridge;

    /**
     * The route controller for the video, null if no appropriate route controller.
     */
    private final MediaRouteController mRouteController;
    private final String mOriginalSourceUrl;
    private final String mOriginalFrameUrl;
    private String mFrameUrl;
    private String mSourceUrl;
    private final String mUserAgent;
    private Bitmap mPosterBitmap;
    private String mCookies;
    private boolean mPauseRequested;
    private boolean mSeekRequested;
    private long mSeekLocation;
    private boolean mIsPlayable;
    private boolean mRouteIsAvailable;

    // mActive is true when the Chrome is playing, or preparing to play, this player's video
    // remotely.
    private boolean mActive;

    private static final String TAG = "MediaFling";

    private final MediaRouteController.MediaStateListener mMediaStateListener =
            new MediaRouteController.MediaStateListener() {
        @Override
        public void onRouteAvailabilityChanged(boolean available) {
            mRouteIsAvailable = available;
            onRouteAvailabilityChange();
        }

        @Override
        public void onRouteDialogCancelled() {
            if (mNativeRemoteMediaPlayerBridge == 0) return;
            nativeOnCancelledRemotePlaybackRequest(mNativeRemoteMediaPlayerBridge);
        }

        @Override
        public void onError() {
            if (mActive && mNativeRemoteMediaPlayerBridge != 0) {
                nativeOnError(mNativeRemoteMediaPlayerBridge);
            }
        }

        @Override
        public void onSeekCompleted() {
            mSeekRequested = false;
            if (mActive && mNativeRemoteMediaPlayerBridge != 0) {
                nativeOnSeekCompleted(mNativeRemoteMediaPlayerBridge);
            }
        }

        @Override
        public void onRouteUnselected() {
            if (mNativeRemoteMediaPlayerBridge == 0) return;
            nativeOnRouteUnselected(mNativeRemoteMediaPlayerBridge);
        }

        @Override
        public void onPlaybackStateChanged(@PlayerState int newState) {
            if (mNativeRemoteMediaPlayerBridge == 0) return;
            if (newState == PlayerState.FINISHED || newState == PlayerState.INVALIDATED) {
                nativeOnPlaybackFinished(mNativeRemoteMediaPlayerBridge);
            } else if (newState == PlayerState.PLAYING) {
                nativeOnPlaying(mNativeRemoteMediaPlayerBridge);
            } else if (newState == PlayerState.PAUSED) {
                mPauseRequested = false;
                nativeOnPaused(mNativeRemoteMediaPlayerBridge);
            }
        }

        @Override
        public String getTitle() {
            if (mNativeRemoteMediaPlayerBridge == 0) return null;
            return nativeGetTitle(mNativeRemoteMediaPlayerBridge);
        }

        @Override
        public Bitmap getPosterBitmap() {
            return mPosterBitmap;
        }

        @Override
        public void pauseLocal() {
            if (mNativeRemoteMediaPlayerBridge == 0) return;
            nativePauseLocal(mNativeRemoteMediaPlayerBridge);
        }

        @Override
        public long getLocalPosition() {
            if (mNativeRemoteMediaPlayerBridge == 0) return 0L;
            return nativeGetLocalPosition(mNativeRemoteMediaPlayerBridge);
        }

        @Override
        public void onCastStarting(String routeName) {
            if (mNativeRemoteMediaPlayerBridge != 0) {
                nativeOnCastStarting(mNativeRemoteMediaPlayerBridge,
                        RemoteMediaPlayerController.instance().getCastingMessage(routeName));
            }
            mActive = true;
        }

        @Override
        public void onCastStarted() {
            if (mNativeRemoteMediaPlayerBridge != 0) {
                nativeOnCastStarted(mNativeRemoteMediaPlayerBridge);
            }
        }

        @Override
        public void onCastStopping() {
            if (mNativeRemoteMediaPlayerBridge != 0) {
                nativeOnCastStopping(mNativeRemoteMediaPlayerBridge);
            }
            mActive = false;
            // Free the poster bitmap to save memory
            mPosterBitmap = null;
        }

        @Override
        public String getSourceUrl() {
            return mSourceUrl;
        }

        @Override
        public String getCookies() {
            return mCookies;
        }

        @Override
        public String getFrameUrl() {
            return mFrameUrl;
        }

        @Override
        public long getStartPositionMillis() {
            return mStartPositionMillis;
        }

        @Override
        public boolean isPauseRequested() {
            return mPauseRequested;
        }

        @Override
        public boolean isSeekRequested() {
            return mSeekRequested;
        }

        @Override
        public long getSeekLocation() {
            return mSeekLocation;
        }
    };

    private RemoteMediaPlayerBridge(long nativeRemoteMediaPlayerBridge, String sourceUrl,
            String frameUrl, String userAgent) {
        Log.d(TAG, "Creating RemoteMediaPlayerBridge");
        mNativeRemoteMediaPlayerBridge = nativeRemoteMediaPlayerBridge;
        mOriginalSourceUrl = sourceUrl;
        mOriginalFrameUrl = frameUrl;
        mUserAgent = userAgent;
        // This will get null if there isn't a mediaRouteController that can play this media.
        mRouteController = RemoteMediaPlayerController.instance()
                .getMediaRouteController(sourceUrl, frameUrl);
    }

    @CalledByNative
    private static RemoteMediaPlayerBridge create(long nativeRemoteMediaPlayerBridge,
            String sourceUrl, String frameUrl, String userAgent) {
        return new RemoteMediaPlayerBridge(
                nativeRemoteMediaPlayerBridge, sourceUrl, frameUrl, userAgent);
    }

    /**
     * Called when a lower layer requests that a video be cast. This will typically be a request
     * from Blink when the cast button is pressed on the default video controls.
     */
    @CalledByNative
    private void requestRemotePlayback(long startPositionMillis) {
        Log.d(TAG, "requestRemotePlayback at t=%d", startPositionMillis);
        if (mRouteController == null) return;
        // Clear out the state
        mPauseRequested = false;
        mSeekRequested = false;
        mStartPositionMillis = startPositionMillis;
        RemoteMediaPlayerController.instance().requestRemotePlayback(
                mMediaStateListener, mRouteController);
    }

    /**
     * Called when a lower layer requests control of a video that is being cast.
     */
    @CalledByNative
    private void requestRemotePlaybackControl() {
        Log.d(TAG, "requestRemotePlaybackControl");
        RemoteMediaPlayerController.instance().requestRemotePlaybackControl(mMediaStateListener);
    }

    /**
     * Called when a lower layer requests to stop casting the video.
     */
    @CalledByNative
    private void requestRemotePlaybackStop() {
        Log.d(TAG, "requestRemotePlaybackStop");
        RemoteMediaPlayerController.instance().requestRemotePlaybackStop(mMediaStateListener);
    }

    @CalledByNative
    private void setNativePlayer() {
        Log.d(TAG, "setNativePlayer");
        if (mRouteController == null) return;
        mActive = true;
    }

    @CalledByNative
    private void onPlayerCreated() {
        Log.d(TAG, "onPlayerCreated");
        if (mRouteController == null) return;
        mRouteController.addMediaStateListener(mMediaStateListener);
    }

    @CalledByNative
    private void onPlayerDestroyed() {
        Log.d(TAG, "onPlayerDestroyed");
        if (mRouteController == null) return;
        mRouteController.removeMediaStateListener(mMediaStateListener);
    }

    /**
     * @param bitmap The bitmap of the poster for the video, null if no poster image exists.
     *
     *         TODO(cimamoglu): Notify the clients (probably through MediaRouteController.Listener)
     *        of the poster image change. This is necessary for when a web page changes the poster
     *        while the client (i.e. only ExpandedControllerActivity for now) is active.
     */
    @CalledByNative
    private void setPosterBitmap(Bitmap bitmap) {
        if (mRouteController == null) return;
        mPosterBitmap = bitmap;
    }

    @CalledByNative
    protected boolean isPlaying() {
        if (mRouteController == null) return false;
        return mRouteController.isPlaying();
    }

    @CalledByNative
    protected int getCurrentPosition() {
        if (mRouteController == null) return 0;
        return (int) mRouteController.getPosition();
    }

    @CalledByNative
    protected int getDuration() {
        if (mRouteController == null) return 0;
        return (int) mRouteController.getDuration();
    }

    @CalledByNative
    protected void release() {
        // Remove the state change listeners. Release does mean that Chrome is no longer interested
        // in events from the media player.
        if (mRouteController != null) mRouteController.setMediaStateListener(null);
        mActive = false;
    }

    @CalledByNative
    protected void setVolume(double volume) {
    }

    @CalledByNative
    protected void start() throws IllegalStateException {
        mPauseRequested = false;
        if (mRouteController != null && mRouteController.isBeingCast()) mRouteController.resume();
    }

    @CalledByNative
    protected void pause() throws IllegalStateException {
        if (mRouteController != null && mRouteController.isBeingCast()) {
            mRouteController.pause();
        } else {
            mPauseRequested = true;
        }
    }

    @CalledByNative
    protected void seekTo(int msec) throws IllegalStateException {
        if (mRouteController != null && mRouteController.isBeingCast()) {
            mRouteController.seekTo(msec);
        } else {
            mSeekRequested = true;
            mSeekLocation = msec;
        }
    }

    private void onRouteAvailabilityChange() {
        Log.d(TAG, "onRouteAvailabilityChange: " + mRouteIsAvailable + ", " + mIsPlayable);
        if (mNativeRemoteMediaPlayerBridge == 0) return;

        int availability = WebRemotePlaybackAvailability.DEVICE_NOT_AVAILABLE;
        if (!mIsPlayable) {
            availability = WebRemotePlaybackAvailability.SOURCE_NOT_COMPATIBLE;
        } else if (mRouteIsAvailable) {
            availability = WebRemotePlaybackAvailability.DEVICE_AVAILABLE;
        }
        nativeOnRouteAvailabilityChanged(mNativeRemoteMediaPlayerBridge, availability);
    }

    @CalledByNative
    protected void destroy() {
        Log.d(TAG, "destroy");
        if (mRouteController != null) {
            mRouteController.removeMediaStateListener(mMediaStateListener);
        }
        mNativeRemoteMediaPlayerBridge = 0;
    }

    @CalledByNative
    private void setCookies(String cookies) {
        if (mRouteController == null) return;
        mCookies = cookies;
        mRouteController.checkIfPlayableRemotely(mOriginalSourceUrl, mOriginalFrameUrl, cookies,
                mUserAgent, new MediaRouteController.MediaValidationCallback() {
                    @Override
                    public void onResult(
                            boolean isPlayable, String revisedSourceUrl, String revisedFrameUrl) {
                        mIsPlayable = isPlayable;
                        mSourceUrl = revisedSourceUrl;
                        mFrameUrl = revisedFrameUrl;
                        onRouteAvailabilityChange();
                    }
                });
    }

    private native void nativeOnPlaying(long nativeRemoteMediaPlayerBridge);
    private native void nativeOnPaused(long nativeRemoteMediaPlayerBridge);
    private native void nativeOnRouteUnselected(long nativeRemoteMediaPlayerBridge);
    private native void nativeOnPlaybackFinished(long nativeRemoteMediaPlayerBridge);
    private native void nativeOnRouteAvailabilityChanged(
            long nativeRemoteMediaPlayerBridge, int availability);
    private native void nativeOnCancelledRemotePlaybackRequest(long nativeRemoteMediaPlayerBridge);
    private native String nativeGetTitle(long nativeRemoteMediaPlayerBridge);
    private native void nativePauseLocal(long nativeRemoteMediaPlayerBridge);
    private native int nativeGetLocalPosition(long nativeRemoteMediaPlayerBridge);
    private native void nativeOnCastStarting(long nativeRemoteMediaPlayerBridge,
            String castingMessage);
    private native void nativeOnCastStarted(long nativeRemoteMediaPlayerBridge);
    private native void nativeOnCastStopping(long nativeRemoteMediaPlayerBridge);
    private native void nativeOnError(long nativeRemoteMediaPlayerBridge);
    private native void nativeOnSeekCompleted(long nativeRemoteMediaPlayerBridge);
}
