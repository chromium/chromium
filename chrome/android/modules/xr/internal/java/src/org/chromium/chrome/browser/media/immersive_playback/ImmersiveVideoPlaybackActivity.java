// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.immersive_playback;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.view.ViewGroup;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.DeviceInfo;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.UnguessableToken;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.media.VideoOverlayActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.components.thinwebview.CompositorView;
import org.chromium.content_public.browser.overlay_window.PlaybackState;
import org.chromium.ui.xr.scenecore.XrSurfaceEntityShape;
import org.chromium.ui.xr.scenecore.XrSurfaceEntityStereoMode;

/**
 * An immersive video playback activity which get created when requesting fullscreen from web API on
 * XR devices. The activity will connect to web API through ImmersiveVideoPlaybackWindowAndroid.
 */
@NullMarked
public class ImmersiveVideoPlaybackActivity extends VideoOverlayActivity {
    private static final String TAG = "ImmersiveVideoPlaybackActivity";

    private @Nullable ImmersiveVideoPlaybackCoordinator mPlaybackCoordinator;

    @SuppressWarnings("StaticFieldLeak")
    private static @Nullable ImmersiveVideoPlaybackCoordinator sPlaybackCoordinatorForTesting;

    /** Stores state that arrives before the UI is ready. This is only used on startup. */
    @VisibleForTesting
    /* package */ static class PendingState {
        public @Nullable Integer mVideoWidth;
        public @Nullable Integer mVideoHeight;
        public @Nullable @PlaybackState Integer mPlaybackState;
        public @Nullable Long mDurationMs;
        public @Nullable Long mPositionMs;
        public @Nullable Double mPlaybackRate;
        public @Nullable @XrSurfaceEntityStereoMode Integer mStereoMode;
        public @Nullable @XrSurfaceEntityShape Integer mShape;

        void apply(ImmersiveVideoPlaybackActivity activity) {
            if (mStereoMode != null || mShape != null) {
                int stereo = mStereoMode != null ? mStereoMode : XrSurfaceEntityStereoMode.MONO;
                int shape = mShape != null ? mShape : XrSurfaceEntityShape.QUAD;
                activity.setImmersiveVideoOptions(stereo, shape);
            }
            if (mVideoWidth != null && mVideoHeight != null) {
                activity.updateVideoSize(mVideoWidth, mVideoHeight);
            }
            if (mPlaybackState != null) {
                activity.setPlaybackState(mPlaybackState);
            }
            if (mDurationMs != null && mPositionMs != null && mPlaybackRate != null) {
                activity.setMediaPosition(mDurationMs, mPositionMs, mPlaybackRate);
            }
            reset();
        }

        void reset() {
            mVideoWidth = null;
            mVideoHeight = null;
            mPlaybackState = null;
            mDurationMs = null;
            mPositionMs = null;
            mPlaybackRate = null;
            mStereoMode = null;
            mShape = null;
        }
    }

    private final PendingState mPendingState = new PendingState();

    /** Delegate for media control events. */
    private final ImmersiveVideoControlDelegate mVideoControlDelegate =
            new ImmersiveVideoControlDelegate() {
                @Override
                public void togglePlayPause(boolean isPlaying) {
                    ImmersiveVideoPlaybackActivity.this.togglePlayPause(!isPlaying);
                }

                @Override
                public void seekTo(long positionMs) {
                    ImmersiveVideoPlaybackActivity.this.seekTo(positionMs);
                }
            };

    @Override
    @Initializer
    public void onStart() {
        super.onStart();

        if (isInitiatorTabDestroyed()) {
            return;
        }

        if (!DeviceInfo.isXr()) {
            finishOverlay(/* closeByNative= */ false);
            return;
        }

        finishInitialize();
    }

    @Initializer
    private void finishInitialize() {
        onActivityStart();

        if (!isNativeHandleInitialized()) {
            finishOverlay(/* closeByNative= */ true);
            return;
        }
    }

    @Override
    public void finishNativeInitialization() {
        super.finishNativeInitialization();

        if (sPlaybackCoordinatorForTesting != null) {
            mPlaybackCoordinator = sPlaybackCoordinatorForTesting;
        } else {
            mPlaybackCoordinator =
                    new ImmersiveVideoPlaybackCoordinator(
                            this, assumeNonNull(getWindowAndroid()), mVideoControlDelegate);

            CompositorView compositorView =
                    mPlaybackCoordinator.createXrCompositorView(
                            XrSurfaceEntityStereoMode.MONO, XrSurfaceEntityShape.QUAD);
            addContentView(compositorView.getView(), new ViewGroup.LayoutParams(0, 0));
            setCompositorView(compositorView);
        }

        mPendingState.apply(this);
    }

    @Override
    public void setImmersiveVideoOptions(
            @XrSurfaceEntityStereoMode int stereoMode, @XrSurfaceEntityShape int shape) {
        if (mPlaybackCoordinator != null) {
            mPlaybackCoordinator.updateVideoLayout(stereoMode, shape);
        } else {
            mPendingState.mStereoMode = stereoMode;
            mPendingState.mShape = shape;
        }
    }

    @Override
    protected void cleanup() {
        if (mPlaybackCoordinator != null) {
            mPlaybackCoordinator.destroy();
            mPlaybackCoordinator = null;
        }
        mPendingState.reset();
    }

    @Override
    public void onStopWithNative() {
        super.onStopWithNative();
        if (isNativeHandleInitialized()) {
            onBackToTab();
            finishOverlay(/* closeByNative= */ false);
        }
    }

    @Override
    public void updateVideoSize(int width, int height) {
        if (mPlaybackCoordinator != null) {
            mPlaybackCoordinator.updatePlayerSize(width, height);
        } else {
            mPendingState.mVideoWidth = width;
            mPendingState.mVideoHeight = height;
        }
    }

    @Override
    @VisibleForTesting
    public void setPlaybackState(@PlaybackState int playbackState) {
        if (mPlaybackCoordinator != null) {
            mPlaybackCoordinator.updatePlaybackState(playbackState == PlaybackState.PLAYING);
        } else {
            mPendingState.mPlaybackState = playbackState;
        }
    }

    @Override
    @VisibleForTesting
    public void setMediaPosition(long durationMs, long positionMs, double playbackRate) {
        if (mPlaybackCoordinator != null) {
            mPlaybackCoordinator.updateMediaPosition(durationMs, positionMs, playbackRate);
        } else {
            mPendingState.mDurationMs = durationMs;
            mPendingState.mPositionMs = positionMs;
            mPendingState.mPlaybackRate = playbackRate;
        }
    }

    public PendingState getPendingStateForTesting() {
        return mPendingState;
    }

    public boolean isCoordinatorInitializedForTesting() {
        return mPlaybackCoordinator != null;
    }

    public static void setPlaybackCoordinatorForTesting(
            ImmersiveVideoPlaybackCoordinator coordinator) {
        sPlaybackCoordinatorForTesting = coordinator;
        ResettersForTesting.register(() -> sPlaybackCoordinatorForTesting = null);
    }

    public static void createActivity(UnguessableToken nativeToken, Object initiatorTab) {
        Activity activity = TabUtils.getActivity((Tab) initiatorTab);
        Context context = activity != null ? activity : ContextUtils.getApplicationContext();
        Intent intent = new Intent(context, ImmersiveVideoPlaybackActivity.class);
        intent.putExtra(NATIVE_TOKEN_KEY, nativeToken);
        intent.putExtra(WEB_CONTENTS_KEY, ((Tab) initiatorTab).getWebContents());
        context.startActivity(intent);
    }
}
