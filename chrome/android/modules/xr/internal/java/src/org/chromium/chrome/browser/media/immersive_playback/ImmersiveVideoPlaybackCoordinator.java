// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.immersive_playback;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.media.immersive_playback.ImmersiveVideoPlaybackTypeUtils.mapProjectionType;
import static org.chromium.chrome.browser.media.immersive_playback.ImmersiveVideoPlaybackTypeUtils.mapStereoMode;

import android.app.Activity;
import android.os.Build;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.DeviceInfo;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.media.immersive_playback.components.ImmersiveVideoControlAutoHideManager;
import org.chromium.chrome.browser.media.immersive_playback.components.ImmersiveVideoControlCoordinator;
import org.chromium.chrome.browser.media.immersive_playback.components.ImmersiveVideoFormatCoordinator;
import org.chromium.chrome.browser.media.immersive_playback.components.ImmersiveVideoPlayerCoordinator;
import org.chromium.chrome.browser.media.immersive_playback.components.ImmersiveVideoPoseManager;
import org.chromium.chrome.browser.xr.scenecore.XrModule;
import org.chromium.components.thinwebview.CompositorView;
import org.chromium.content_public.browser.ImmersiveProjectionType;
import org.chromium.content_public.browser.ImmersiveStereoMode;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.xr.scenecore.XrSceneCoreSessionManager;

/** Coordinator for the XR immersive video player. */
@NullMarked
public class ImmersiveVideoPlaybackCoordinator
        implements ImmersiveVideoControlCoordinator.Delegate,
                ImmersiveVideoFormatCoordinator.Delegate,
                ImmersiveVideoPlayerCoordinator.Delegate,
                ImmersiveVideoPoseManager.Delegate {
    private final ImmersiveVideoPlaybackDelegate mPlaybackDelegate;
    private final ImmersiveVideoPlayerCoordinator mPlayerCoordinator;
    private final ImmersiveVideoControlCoordinator mControlCoordinator;
    private final ImmersiveVideoFormatCoordinator mFormatCoordinator;
    private final ImmersiveVideoControlAutoHideManager mAutoHideManager;
    private final ImmersiveVideoPoseManager mPoseManager;
    private @ImmersiveStereoMode int mStereoMode = ImmersiveStereoMode.MONO;
    private @ImmersiveProjectionType int mProjectionType = ImmersiveProjectionType.QUAD;

    private static XrSceneCoreSessionManager getXrSceneCoreSessionManager(Activity activity) {
        assert DeviceInfo.isXr();
        assert Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE;
        assert XrModule.isInstalled() : "XR module must be installed on XR devices.";
        return assumeNonNull(XrModule.getImpl().getXrSceneCoreSessionManager(activity));
    }

    /**
     * Creates a new {@link ImmersiveVideoPlaybackCoordinator}.
     *
     * @param activity The {@link Activity} context.
     * @param windowAndroid The {@link WindowAndroid} for the activity.
     * @param delegate The {@link ImmersiveVideoPlaybackDelegate} for media controls.
     */
    public ImmersiveVideoPlaybackCoordinator(
            Activity activity,
            WindowAndroid windowAndroid,
            ImmersiveVideoPlaybackDelegate delegate) {
        this(activity, windowAndroid, delegate, getXrSceneCoreSessionManager(activity));
    }

    @VisibleForTesting
    ImmersiveVideoPlaybackCoordinator(
            Activity activity,
            WindowAndroid windowAndroid,
            ImmersiveVideoPlaybackDelegate playbackDelegate,
            XrSceneCoreSessionManager xrSessionManager) {
        mPlayerCoordinator = createPlayerCoordinator(activity, windowAndroid, xrSessionManager);
        mPoseManager = new ImmersiveVideoPoseManager(this);
        mControlCoordinator =
                new ImmersiveVideoControlCoordinator(activity, xrSessionManager, this);
        mFormatCoordinator = new ImmersiveVideoFormatCoordinator(activity, xrSessionManager, this);
        mAutoHideManager = new ImmersiveVideoControlAutoHideManager(this::hideControlPanel);
        mPlaybackDelegate = playbackDelegate;
    }

    // =========================================================================
    // Public API / Lifecycle
    // =========================================================================

    /**
     * Shows the immersive player and returns the {@link CompositorView}.
     *
     * @return The created compositor view.
     */
    public CompositorView show() {
        mPlayerCoordinator.show();
        showControlPanel();
        return mPlayerCoordinator.getCompositorView();
    }

    /** Disposes the coordinator and its components. */
    public void dispose() {
        mAutoHideManager.stopTimer();
        mControlCoordinator.dispose();
        mFormatCoordinator.dispose();
        mPlayerCoordinator.dispose();
    }

    /**
     * Updates the video layout based on stereo mode and projection type.
     *
     * @param stereoMode The stereo mode to use.
     * @param projectionType The projection type to use.
     */
    public void updateVideoLayout(
            @ImmersiveStereoMode int stereoMode, @ImmersiveProjectionType int projectionType) {
        mStereoMode = stereoMode;
        mProjectionType = projectionType;

        mPlayerCoordinator.updateVideoLayout(
                mapStereoMode(stereoMode), mapProjectionType(projectionType));
        mPlayerCoordinator.updatePose(
                mPoseManager.getPlayerPanelTranslation(projectionType),
                mPoseManager.getPlayerPanelRotation(projectionType));
        updateControlPanel();
    }

    /**
     * Updates the seek bar with the current media position.
     *
     * @param durationMs The total duration of the media in milliseconds.
     * @param positionMs The current position of the media in milliseconds.
     * @param playbackRate The current playback rate of the media.
     */
    public void updateMediaPosition(long durationMs, long positionMs, double playbackRate) {
        mControlCoordinator.updateMediaPosition(durationMs, positionMs, playbackRate);
    }

    /**
     * Updates the playback state.
     *
     * @param isPlaying True if playing, false otherwise.
     */
    public void updatePlaybackState(boolean isPlaying) {
        mControlCoordinator.updatePlaybackState(isPlaying);
    }

    /**
     * Updates the player size.
     *
     * @param width The width in pixels.
     * @param height The height in pixels.
     */
    public void updatePlayerSize(int width, int height) {
        mPlayerCoordinator.updatePlayerSize(width, height);
    }

    // =========================================================================
    // Delegate Implementations
    // =========================================================================

    // ImmersiveVideoPlayerCoordinator.Delegate

    @Override
    public void onPlayerPanelClicked() {
        toggleControlPanel();
    }

    @Override
    public void onPlayerPanelPoseChanged(float[] translation, float[] rotation) {
        mPoseManager.onPlayerPanelPoseChanged(translation, rotation, mProjectionType);
    }

    @Override
    public void onPlayerPanelResized(float width, float height) {
        updateControlPanel();
    }

    @Override
    public float getLayoutHeight() {
        return mPlayerCoordinator.getLayoutHeight();
    }

    // ImmersiveVideoControlCoordinator.Delegate

    @Override
    public void onControlPanelMoveChanged(boolean isMoving) {
        mAutoHideManager.onControlPanelMoveChanged(isMoving);
    }

    @Override
    public void onControlPanelPoseChanged(float[] translation, float[] rotation) {
        mPoseManager.onControlPanelPoseChanged(translation, rotation, mProjectionType);
    }

    @Override
    public void onControlPanelHoverChanged(boolean hovered) {
        mAutoHideManager.onControlPanelHoverChanged(hovered);
    }

    @Override
    public void togglePlayPause(boolean isPlaying) {
        mPlaybackDelegate.togglePlayPause(isPlaying);
    }

    @Override
    public void seekTo(long positionMs) {
        mPlaybackDelegate.seekTo(positionMs);
    }

    @Override
    public void onFormatClicked() {
        if (mFormatCoordinator.isShowing()) {
            hideFormatSelectionPanel();
        } else {
            showFormatSelectionPanel();
        }
    }

    @Override
    public void onExitImmersivePlayback() {
        mPlaybackDelegate.onExitImmersivePlayback();
    }

    // ImmersiveVideoFormatCoordinator.Delegate

    @Override
    public void onFormatSelected(int stereoMode, int projectionType) {
        updateVideoLayout(stereoMode, projectionType);
        hideFormatSelectionPanel();
    }

    @Override
    public void onFormatPanelHoverChanged(boolean hovered) {
        mAutoHideManager.onFormatPanelHoverChanged(hovered);
    }

    // =========================================================================
    // Private Helpers - Panel Management
    // =========================================================================

    private void toggleControlPanel() {
        if (mControlCoordinator.isShowing()) {
            hideControlPanel();
        } else {
            showControlPanel();
        }
    }

    private void showControlPanel() {
        mControlCoordinator.show(assumeNonNull(mPlayerCoordinator.getHolder()));
        // TODO(crbug.com/515422620): The player panel should be interactable all the time and
        // should toggle the visibility of the control panel.
        mPlayerCoordinator.setInteractable(false);
        updateControlPanel();
        mAutoHideManager.startTimer();
    }

    private void hideControlPanel() {
        hideFormatSelectionPanel();
        mControlCoordinator.dismiss();
        mPlayerCoordinator.setInteractable(true);
        mAutoHideManager.stopTimer();
    }

    private void updateControlPanel() {
        boolean isQuad = mProjectionType == ImmersiveProjectionType.QUAD;
        mControlCoordinator.setMovable(!isQuad);
        if (mControlCoordinator.isShowing()) {
            mControlCoordinator.updatePose(
                    mPoseManager.getControlPanelTranslation(mProjectionType),
                    mPoseManager.getControlPanelRotation(mProjectionType));
        }
    }

    private void showFormatSelectionPanel() {
        mFormatCoordinator.show(
                assumeNonNull(mControlCoordinator.getHolder()),
                mControlCoordinator.getSize(),
                mStereoMode,
                mProjectionType);
        mControlCoordinator.setFormatButtonSelected(true);
    }

    private void hideFormatSelectionPanel() {
        mFormatCoordinator.dismiss();
        mControlCoordinator.setFormatButtonSelected(false);
    }

    // =========================================================================
    // Factory & Testing Helpers
    // =========================================================================

    @VisibleForTesting
    protected ImmersiveVideoPlayerCoordinator createPlayerCoordinator(
            Activity activity,
            WindowAndroid windowAndroid,
            XrSceneCoreSessionManager sessionManager) {
        return new ImmersiveVideoPlayerCoordinator(activity, windowAndroid, sessionManager, this);
    }

    public ImmersiveVideoFormatCoordinator getFormatCoordinatorForTesting() {
        return mFormatCoordinator;
    }

    public ImmersiveVideoControlCoordinator getControlCoordinatorForTesting() {
        return mControlCoordinator;
    }
}
