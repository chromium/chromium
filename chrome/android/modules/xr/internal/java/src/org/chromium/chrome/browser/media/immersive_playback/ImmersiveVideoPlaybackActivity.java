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
import org.chromium.content_public.browser.ImmersiveProjectionType;
import org.chromium.content_public.browser.ImmersiveStereoMode;
import org.chromium.content_public.browser.overlay_window.PlaybackState;

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
        public @Nullable @ImmersiveStereoMode Integer mStereoMode;
        public @Nullable @ImmersiveProjectionType Integer mProjectionType;

        void apply(ImmersiveVideoPlaybackActivity activity) {
            if (mStereoMode != null || mProjectionType != null) {
                int stereoMode = mStereoMode != null ? mStereoMode : ImmersiveStereoMode.MONO;
                int projectionType =
                        mProjectionType != null ? mProjectionType : ImmersiveProjectionType.QUAD;
                activity.setImmersiveVideoOptions(stereoMode, projectionType);
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
            mProjectionType = null;
        }
    }

    private final PendingState mPendingState = new PendingState();

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
                            this,
                            assumeNonNull(getWindowAndroid()),
                            new ImmersiveVideoPlaybackDelegate() {
                                @Override
                                public void togglePlayPause(boolean isPlaying) {
                                    ImmersiveVideoPlaybackActivity.this.togglePlayPause(!isPlaying);
                                }

                                @Override
                                public void seekTo(long positionMs) {
                                    ImmersiveVideoPlaybackActivity.this.seekTo(positionMs);
                                }

                                @Override
                                public void onExitImmersivePlayback() {
                                    finishOverlay(/* closeByNative= */ false);
                                }
                            });

            CompositorView compositorView = mPlaybackCoordinator.show();
            addContentView(compositorView.getView(), new ViewGroup.LayoutParams(0, 0));
            setCompositorView(compositorView);
        }

        mPendingState.apply(this);
    }

    @Override
    protected void cleanup() {
        if (mPlaybackCoordinator != null) {
            mPlaybackCoordinator.dispose();
            mPlaybackCoordinator = null;
        }
        mPendingState.reset();
    }

    @Override
    public void onStopWithNative() {
        super.onStopWithNative();
        if (isFinishing() && isNativeHandleInitialized()) {
            onBackToTab();
            finishOverlay(/* closeByNative= */ false);
        }
    }

    @Override
    protected void onDestroy() {
        if (isNativeHandleInitialized()) {
            finishOverlay(/* closeByNative= */ false);
        }
        super.onDestroy();
    }

    @Override
    public void setImmersiveVideoOptions(
            @ImmersiveStereoMode int stereoMode, @ImmersiveProjectionType int projectionType) {
        if (mPlaybackCoordinator != null) {
            mPlaybackCoordinator.updateVideoLayout(stereoMode, projectionType);
        } else {
            mPendingState.mStereoMode = stereoMode;
            mPendingState.mProjectionType = projectionType;
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

    /**
     * Creates and starts the {@link ImmersiveVideoPlaybackActivity}.
     *
     * @param nativeToken The native token for communication.
     * @param initiatorTab The tab that initiated the playback.
     */
    public static void createActivity(UnguessableToken nativeToken, Object initiatorTab) {
        Activity activity = TabUtils.getActivity((Tab) initiatorTab);
        Context context = activity != null ? activity : ContextUtils.getApplicationContext();
        Intent intent = new Intent(context, ImmersiveVideoPlaybackActivity.class);
        intent.putExtra(NATIVE_TOKEN_KEY, nativeToken);
        intent.putExtra(WEB_CONTENTS_KEY, ((Tab) initiatorTab).getWebContents());
        context.startActivity(intent);
    }
}
