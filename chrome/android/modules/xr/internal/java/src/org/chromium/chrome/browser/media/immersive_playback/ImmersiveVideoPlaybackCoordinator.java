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
import org.chromium.ui.xr.scenecore.XrEntityHolder;
import org.chromium.ui.xr.scenecore.XrFloatSize3d;
import org.chromium.ui.xr.scenecore.XrPose;
import org.chromium.ui.xr.scenecore.XrSceneCoreSessionManager;
import org.chromium.ui.xr.scenecore.XrSurfaceEntityShape;
import org.chromium.ui.xr.scenecore.XrSurfaceEntityStereoMode;
import org.chromium.ui.xr.scenecore.XrVector3;

/** Coordinator for the XR immersive video player. */
@NullMarked
public class ImmersiveVideoPlaybackCoordinator
        implements ImmersiveVideoControlCoordinator.Delegate,
                ImmersiveVideoFormatCoordinator.Delegate,
                ImmersiveVideoPlayerCoordinator.Delegate,
                ImmersiveVideoPoseManager.Delegate {
    private final XrEntityHolder mActivitySpaceEntity;
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
        mActivitySpaceEntity = xrSessionManager.getActivitySpaceEntity();
        mPlayerCoordinator = createPlayerCoordinator(activity, windowAndroid, xrSessionManager);
        mPoseManager = new ImmersiveVideoPoseManager(this);
        mPoseManager.updateStrategy(mapProjectionType(mProjectionType));
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
        mPlayerCoordinator.setInteractable(true);
        showControlPanel();
        return mPlayerCoordinator.getCompositorView();
    }

    /** Disposes the coordinator and its components. */
    public void dispose() {
        mAutoHideManager.stopTimer();
        mFormatCoordinator.dispose();
        mControlCoordinator.dispose();
        mPlayerCoordinator.dispose();
    }

    /**
     * Sets the initial immersive video options for the playback session.
     *
     * @param stereoMode The stereo mode to use.
     * @param projectionType The projection type to use.
     * @param isRecommended True if this format is recommended by native video metadata.
     */
    public void setImmersiveVideoOptions(
            @ImmersiveStereoMode int stereoMode,
            @ImmersiveProjectionType int projectionType,
            boolean isRecommended) {
        if (isRecommended) {
            mFormatCoordinator.setRecommendedFormat(stereoMode, projectionType);
        }
        updateVideoLayout(stereoMode, projectionType);
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
    public void onPlayerPanelPoseChanged(XrPose pose) {
        mPoseManager.onPlayerPanelPoseChanged(pose);
        updatePose();
    }

    @Override
    public void onPlayerPanelDragStart(XrVector3 origin, XrVector3 direction) {
        mPoseManager.onPlayerPanelDragStart(origin, direction);
    }

    @Override
    public void onPlayerPanelDragUpdate(XrVector3 origin, XrVector3 direction) {
        mPoseManager.onPlayerPanelDragUpdate(origin, direction);
        updatePose();
    }

    @Override
    public void onPlayerPanelDragEnd(XrVector3 origin, XrVector3 direction) {
        mPoseManager.onPlayerPanelDragEnd(origin, direction);
        updatePose();
    }

    private void updatePose() {
        mPlayerCoordinator.updatePose(mPoseManager.getPlayerPanelPose());
        updateControlPanel();
    }

    @Override
    public void onPlayerPanelResized(XrFloatSize3d size) {
        updateControlPanel();
    }

    @Override
    public float getLayoutHeight() {
        return mPlayerCoordinator.getLayoutHeight();
    }

    @Override
    public float getCurveRadius() {
        return mPlayerCoordinator.getCurveRadius();
    }

    // ImmersiveVideoControlCoordinator.Delegate

    @Override
    public void onControlPanelMoveChanged(boolean isMoving) {
        mAutoHideManager.onControlPanelMoveChanged(isMoving);
    }

    @Override
    public void onControlPanelPoseChanged(XrPose pose) {
        mPoseManager.onControlPanelPoseChanged(pose);
        updatePose();
    }

    @Override
    public void onControlPanelHoverChanged(boolean hovered) {
        mAutoHideManager.onControlPanelHoverChanged(hovered);
    }

    @Override
    public void onControlPanelAccessibilityFocusChanged(boolean focused) {
        mAutoHideManager.onControlPanelAccessibilityFocusChanged(focused);
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

    @Override
    public void onFormatPanelAccessibilityFocusChanged(boolean focused) {
        mAutoHideManager.onFormatPanelAccessibilityFocusChanged(focused);
    }

    // =========================================================================
    // Private Helpers - Panel Management
    // =========================================================================

    private void updateVideoLayout(
            @ImmersiveStereoMode int stereoMode, @ImmersiveProjectionType int projectionType) {
        mStereoMode = stereoMode;
        @XrSurfaceEntityShape int shape = mapProjectionType(projectionType);
        @XrSurfaceEntityStereoMode int mode = mapStereoMode(stereoMode);

        if (mProjectionType != projectionType) {
            mProjectionType = projectionType;
            mPoseManager.updateStrategy(shape);
        }
        mPlayerCoordinator.updateVideoLayout(mode, shape);
        updatePose();
    }

    private void toggleControlPanel() {
        if (mControlCoordinator.isShowing()) {
            hideControlPanel();
        } else {
            showControlPanel();
        }
    }

    private void showControlPanel() {
        mControlCoordinator.show(getControlPanelParent());
        updateControlPanel();
        mAutoHideManager.startTimer();
    }

    private void hideControlPanel() {
        hideFormatSelectionPanel();
        mControlCoordinator.dismiss();
        mPlayerCoordinator.requestFocusForAccessibility();
        mAutoHideManager.stopTimer();
    }

    private void updateControlPanel() {
        mControlCoordinator.setMovable(shouldControlPanelBeMovable());
        mControlCoordinator.setParent(getControlPanelParent());
        mControlCoordinator.updatePose(mPoseManager.getControlPanelPose());
    }

    private boolean shouldControlPanelBeMovable() {
        return mProjectionType == ImmersiveProjectionType.HEMISPHERE;
    }

    private XrEntityHolder getControlPanelParent() {
        boolean isQuad = mProjectionType == ImmersiveProjectionType.QUAD;
        return isQuad ? assumeNonNull(mPlayerCoordinator.getHolder()) : mActivitySpaceEntity;
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
