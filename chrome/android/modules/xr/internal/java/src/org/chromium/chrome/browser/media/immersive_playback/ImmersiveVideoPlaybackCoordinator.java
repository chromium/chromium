// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.immersive_playback;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.os.Build;
import android.util.Rational;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.DeviceInfo;
import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.xr.scenecore.XrModule;
import org.chromium.components.thinwebview.CompositorView;
import org.chromium.components.thinwebview.CompositorViewFactory;
import org.chromium.components.thinwebview.ThinWebViewConstraints;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.xr.scenecore.XrCurvedSurfaceEntityHolder;
import org.chromium.ui.xr.scenecore.XrMovableComponent;
import org.chromium.ui.xr.scenecore.XrMovableComponent.OnMoveListener;
import org.chromium.ui.xr.scenecore.XrPanelEntityHolder;
import org.chromium.ui.xr.scenecore.XrQuadSurfaceEntityHolder;
import org.chromium.ui.xr.scenecore.XrResizableComponent;
import org.chromium.ui.xr.scenecore.XrResizableComponent.OnResizeListener;
import org.chromium.ui.xr.scenecore.XrResizableEntityHolder;
import org.chromium.ui.xr.scenecore.XrSceneCoreSessionManager;
import org.chromium.ui.xr.scenecore.XrSurfaceEntityHolder;
import org.chromium.ui.xr.scenecore.XrSurfaceEntityShape;
import org.chromium.ui.xr.scenecore.XrSurfaceEntityStereoMode;
import org.chromium.ui.xr.scenecore.XrSurfaceEntityView;

/** A coordinator for XR specific Picture-in-Picture functionality. */
@NullMarked
public class ImmersiveVideoPlaybackCoordinator {
    private static final float PLAYER_PANEL_MIN_WIDTH_METERS = 0.5f;
    private static final float PLAYER_PANEL_MAX_WIDTH_METERS = 3.0f;
    private static final float PLAYER_PANEL_INITIAL_WIDTH_METERS = 2.0f;
    private static final float PLAYER_PANEL_INITIAL_CURVE_RADIUS_METERS = 10.0f;
    private static final float[] PLAYER_PANEL_INITIAL_TRANSLATION = {0.0f, 0.0f, -1.75f};
    private static final float[] PLAYER_PANEL_INITIAL_ROTATION = {0.0f, 0.0f, 0.0f, 1.0f};

    private static final float[] MEDIA_CONTROL_PANEL_INITIAL_TRANSLATION = {0.0f, -1f, -1.75f};
    private static final float[] MEDIA_CONTROL_PANEL_INITIAL_ROTATION = {0.0f, 0.0f, 0.0f, 1.0f};
    private static final float MEDIA_CONTROL_PANEL_WIDTH_METERS = 1.0f;
    private static final float MEDIA_CONTROL_PANEL_HEIGHT_METERS = 0.25f;
    private static final float MEDIA_CONTROL_PANEL_VERTICAL_SPACING_METERS = 0.25f;
    private static final float MEDIA_CONTROL_PANEL_Z_OFFSET_METERS = 0.01f;

    private final Activity mActivity;
    private final WindowAndroid mWindowAndroid;
    private final LazyOneshotSupplier<XrSceneCoreSessionManager> mXrSceneCoreSessionManagerSupplier;
    private final ImmersiveVideoControlPanel mControlPanel;

    private @Nullable CompositorView mCompositorView;
    private @Nullable XrSurfaceEntityHolder mSurfaceHolder;
    private @Nullable XrPanelEntityHolder mControlPanelHolder;
    private @Nullable OnMoveListener mPlayerPanelMoveListener;
    private @Nullable OnResizeListener mPlayerPanelResizeListener;
    private Rational mAspectRatio = new Rational(16, 9);

    /**
     * @param activity The activity that will contain the XR compositor view.
     * @param windowAndroid The window android for the activity.
     * @param videoControlDelegate The delegate for media controls.
     */
    public ImmersiveVideoPlaybackCoordinator(
            Activity activity,
            WindowAndroid windowAndroid,
            ImmersiveVideoControlDelegate videoControlDelegate) {
        mActivity = activity;
        mWindowAndroid = windowAndroid;
        mXrSceneCoreSessionManagerSupplier =
                LazyOneshotSupplier.fromSupplier(this::createXrSceneCoreSessionManager);
        mControlPanel = new ImmersiveVideoControlPanel(activity, videoControlDelegate);
    }

    @VisibleForTesting
    protected XrSceneCoreSessionManager createXrSceneCoreSessionManager() {
        assert DeviceInfo.isXr();
        assert Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE;
        assert XrModule.isInstalled() : "XR module must be installed on XR devices.";
        return assumeNonNull(XrModule.getImpl().getXrSceneCoreSessionManager(mActivity));
    }

    /**
     * Creates an XR compositor view and adds it to the activity.
     *
     * @param layout The video layout to use.
     * @return The created compositor view.
     */
    public CompositorView createXrCompositorView(
            @XrSurfaceEntityStereoMode int stereoMode, @XrSurfaceEntityShape int shape) {
        assumeNonNull(mXrSceneCoreSessionManagerSupplier.get())
                .getMainPanelEntity()
                .setEntityEnabled(false);

        mControlPanelHolder =
                mXrSceneCoreSessionManagerSupplier
                        .get()
                        .createPanelEntity(mControlPanel, "MediaControlPanel");

        mPlayerPanelMoveListener =
                new XrMovableComponent.OnMoveListener() {
                    @Override
                    public void onMoveUpdate(float[] translation, float[] rotation, float scale) {
                        if (mSurfaceHolder != null
                                && mSurfaceHolder.getSurfaceShape() == XrSurfaceEntityShape.QUAD) {
                            float height =
                                    ((XrResizableEntityHolder) mSurfaceHolder)
                                            .getEntitySize()
                                            .getHeight();
                            updateMediaControlPanelPose(translation, rotation, height);
                        }
                    }
                };

        mPlayerPanelResizeListener =
                new XrResizableComponent.OnResizeListener() {
                    @Override
                    public void onResizeUpdate(float width, float height, float depth) {
                        if (mSurfaceHolder != null) {
                            float[] translation = mSurfaceHolder.getEntityTranslation();
                            float[] rotation = mSurfaceHolder.getEntityRotation();
                            updateMediaControlPanelPose(translation, rotation, height);
                        }
                    }
                };

        mCompositorView = createCompositorView(shape);

        if (mCompositorView.getView() instanceof XrSurfaceEntityView view) {
            mSurfaceHolder = view.getHolder();
            updateVideoLayout(stereoMode, shape);
        }

        return mCompositorView;
    }

    @VisibleForTesting
    protected CompositorView createCompositorView(int shape) {
        return CompositorViewFactory.create(
                mActivity,
                mWindowAndroid,
                new ThinWebViewConstraints(),
                assumeNonNull(mXrSceneCoreSessionManagerSupplier.get()),
                shape);
    }

    /**
     * Updates the video layout.
     *
     * @param layout The video layout to use.
     */
    public void updateVideoLayout(
            @XrSurfaceEntityStereoMode int stereoMode, @XrSurfaceEntityShape int shape) {
        assumeNonNull(mSurfaceHolder);
        updateShapeIfNeeded(shape);
        updateStereoModeIfNeeded(stereoMode);
    }

    private void updateStereoModeIfNeeded(@XrSurfaceEntityStereoMode int stereoMode) {
        XrSurfaceEntityHolder surfaceHolder = assumeNonNull(mSurfaceHolder);
        if (stereoMode != surfaceHolder.getSurfaceStereoMode()) {
            surfaceHolder.setSurfaceStereoMode(stereoMode);
        }
    }

    private void updateShapeIfNeeded(@XrSurfaceEntityShape int shape) {
        XrSurfaceEntityHolder surfaceHolder = assumeNonNull(mSurfaceHolder);

        if (surfaceHolder instanceof XrQuadSurfaceEntityHolder) {
            resetQuadSurface((XrQuadSurfaceEntityHolder) surfaceHolder);
        }

        surfaceHolder.setSurfaceShape(shape);
        surfaceHolder.setEntityPose(
                PLAYER_PANEL_INITIAL_TRANSLATION, PLAYER_PANEL_INITIAL_ROTATION);

        switch (shape) {
            case XrSurfaceEntityShape.QUAD:
                configureMediaControlPanel(false);
                if (surfaceHolder instanceof XrQuadSurfaceEntityHolder) {
                    configureQuadSurface((XrQuadSurfaceEntityHolder) surfaceHolder);
                }
                break;
            case XrSurfaceEntityShape.SPHERE:
            case XrSurfaceEntityShape.HEMISPHERE:
                configureMediaControlPanel(true);
                if (surfaceHolder instanceof XrCurvedSurfaceEntityHolder) {
                    configureCurvedSurface((XrCurvedSurfaceEntityHolder) surfaceHolder);
                }
                break;
        }
    }

    private void configureMediaControlPanel(boolean isMovable) {
        if (mControlPanelHolder == null) {
            return;
        }

        mControlPanelHolder.setEntitySize(
                MEDIA_CONTROL_PANEL_WIDTH_METERS, MEDIA_CONTROL_PANEL_HEIGHT_METERS);
        mControlPanelHolder.getMovableComponent().setMovable(isMovable, false);
    }

    private void configureQuadSurface(XrQuadSurfaceEntityHolder quadHolder) {
        float initialWidth = PLAYER_PANEL_INITIAL_WIDTH_METERS;
        float initialHeight = calculateHeight(initialWidth);
        float minWidth = PLAYER_PANEL_MIN_WIDTH_METERS;
        float minHeight = calculateHeight(minWidth);
        float maxWidth = PLAYER_PANEL_MAX_WIDTH_METERS;
        float maxHeight = calculateHeight(maxWidth);

        XrResizableComponent resizable = quadHolder.getResizableComponent();
        XrMovableComponent movable = quadHolder.getMovableComponent();

        quadHolder.setEntitySize(initialWidth, initialHeight);
        resizable.setMinSize(minWidth, minHeight);
        resizable.setMaxSize(maxWidth, maxHeight);
        resizable.setResizable(true, true);
        movable.setMovable(true, false);
        movable.addMoveListener(assumeNonNull(mPlayerPanelMoveListener));
        resizable.addResizeListener(assumeNonNull(mPlayerPanelResizeListener));

        updateMediaControlPanelPose(
                PLAYER_PANEL_INITIAL_TRANSLATION, PLAYER_PANEL_INITIAL_ROTATION, initialHeight);
    }

    private void resetQuadSurface(XrQuadSurfaceEntityHolder quadHolder) {
        XrResizableComponent resizable = quadHolder.getResizableComponent();
        XrMovableComponent movable = quadHolder.getMovableComponent();

        quadHolder.setEntitySize(1f, 1f);
        resizable.setMinSize(1f, 1f);
        resizable.setMaxSize(1f, 1f);
        resizable.setResizable(false, false);
        movable.setMovable(false, false);
        movable.removeMoveListener(assumeNonNull(mPlayerPanelMoveListener));
        resizable.removeResizeListener(assumeNonNull(mPlayerPanelResizeListener));
    }

    private void configureCurvedSurface(XrCurvedSurfaceEntityHolder curvedHolder) {
        curvedHolder.setEntityRadius(PLAYER_PANEL_INITIAL_CURVE_RADIUS_METERS);
        if (mControlPanelHolder != null) {
            mControlPanelHolder.setEntityPose(
                    MEDIA_CONTROL_PANEL_INITIAL_TRANSLATION, MEDIA_CONTROL_PANEL_INITIAL_ROTATION);
        }
    }

    private float calculateHeight(float width) {
        Rational aspect = mAspectRatio;
        if (aspect.floatValue() == 0) {
            return width * 9f / 16f;
        }
        return width / aspect.floatValue();
    }

    private void updateMediaControlPanelPose(float[] translation, float[] rotation, float height) {
        if (mControlPanelHolder != null) {
            float[] newTranslation = translation.clone();
            // Move the media control panel under the player panel.
            newTranslation[1] -= height / 2 + MEDIA_CONTROL_PANEL_VERTICAL_SPACING_METERS;
            // Keep the media control panel slightly above the player panel.
            newTranslation[2] += MEDIA_CONTROL_PANEL_Z_OFFSET_METERS;
            mControlPanelHolder.setEntityPose(newTranslation, rotation);
        }
    }

    /**
     * Updates the seek bar with the current media position.
     *
     * @param durationMs The total duration of the media in milliseconds.
     * @param positionMs The current position of the media in milliseconds.
     * @param playbackRate The current playback rate of the media.
     */
    public void updateMediaPosition(long durationMs, long positionMs, double playbackRate) {
        mControlPanel.updateMediaPosition(durationMs, positionMs, playbackRate);
    }

    /**
     * Updates the playback state of the control panel.
     *
     * @param isPlaying Whether the media is currently playing.
     */
    public void updatePlaybackState(boolean isPlaying) {
        mControlPanel.updatePlaybackState(isPlaying);
    }

    /**
     * Updates XR specific Picture-in-Picture parameters.
     *
     * @param width The current width.
     * @param height The current height.
     */
    public void updatePlayerSize(int width, int height) {
        mAspectRatio = new Rational(width, height);
        if (mSurfaceHolder != null) {
            mSurfaceHolder.setSurfacePixelDimensions(width, height);
        }
    }

    public ImmersiveVideoControlPanel getControlPanelForTesting() {
        return mControlPanel;
    }

    /** Cleans up the coordinator and the compositor view. */
    public void destroy() {
        if (mCompositorView != null) {
            View view = mCompositorView.getView();
            if (view.getParent() instanceof ViewGroup parent) {
                parent.removeView(view);
            }
            mCompositorView.destroy();
            mCompositorView = null;
        }
        if (mControlPanel.getParent() instanceof ViewGroup parent) {
            parent.removeView(mControlPanel);
        }
        if (mControlPanelHolder != null) {
            mControlPanelHolder.dispose();
            mControlPanelHolder = null;
        }
        // Listeners are cleaned up automatically during mCompositorView.destroy(), which
        // disposes the surface holder and its components.
        mPlayerPanelMoveListener = null;
        mPlayerPanelResizeListener = null;
        mSurfaceHolder = null;
    }
}
