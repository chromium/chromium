// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.immersive_playback.components;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;

import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.thinwebview.CompositorView;
import org.chromium.components.thinwebview.CompositorViewFactory;
import org.chromium.components.thinwebview.ThinWebViewConstraints;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.xr.scenecore.XrMovableComponent;
import org.chromium.ui.xr.scenecore.XrResizableComponent;
import org.chromium.ui.xr.scenecore.XrSceneCoreSessionManager;
import org.chromium.ui.xr.scenecore.XrSurfaceEntityHolder;
import org.chromium.ui.xr.scenecore.XrSurfaceEntityShape;
import org.chromium.ui.xr.scenecore.XrSurfaceEntityStereoMode;
import org.chromium.ui.xr.scenecore.XrSurfaceEntityView;

/** Coordinator for the video player panel. */
@NullMarked
public class ImmersiveVideoPlayerCoordinator {
    /** Delegate for player panel interactions and pose changes. */
    public interface Delegate {
        void onPlayerPanelClicked();

        void onPlayerPanelPoseChanged(float[] translation, float[] rotation);

        void onPlayerPanelResized(float width, float height);
    }

    private final PropertyModel mModel =
            new PropertyModel.Builder(ImmersiveVideoPlayerProperties.ALL_KEYS)
                    .with(ImmersiveVideoPlayerProperties.DEFAULT_SPATIAL_WIDTH, 1f)
                    .with(ImmersiveVideoPlayerProperties.DEFAULT_MIN_WIDTH, 1f)
                    .with(ImmersiveVideoPlayerProperties.DEFAULT_MAX_WIDTH, 3f)
                    .with(ImmersiveVideoPlayerProperties.DEFAULT_CURVE_RADIUS, 5f)
                    .with(ImmersiveVideoPlayerProperties.DEFAULT_ASPECT_RATIO, 16f / 9f)
                    .with(
                            ImmersiveVideoPlayerProperties.POSE_TRANSLATION,
                            new float[] {0f, 0f, 0.5f})
                    .with(
                            ImmersiveVideoPlayerProperties.POSE_ROTATION,
                            new float[] {0f, 0f, 0f, 1f})
                    .with(
                            ImmersiveVideoPlayerProperties.STEREO_MODE,
                            XrSurfaceEntityStereoMode.MONO)
                    .with(ImmersiveVideoPlayerProperties.SHAPE, XrSurfaceEntityShape.QUAD)
                    .build();

    private final Activity mActivity;
    private final WindowAndroid mWindowAndroid;
    private final XrSceneCoreSessionManager mSessionManager;
    private final Delegate mDelegate;
    private final XrMovableComponent.OnMoveListener mOnMoveListener =
            new XrMovableComponent.OnMoveListener() {
                @Override
                public void onMoveStart(float[] translation, float[] rotation, float scale) {}

                @Override
                public void onMoveUpdate(float[] translation, float[] rotation, float scale) {}

                @Override
                public void onMoveEnd(float[] translation, float[] rotation, float scale) {
                    mDelegate.onPlayerPanelPoseChanged(translation, rotation);
                }
            };

    private @Nullable CompositorView mCompositorView;
    private @Nullable ImmersiveVideoPlayerMediator mMediator;
    private @Nullable XrSurfaceEntityHolder mHolder;

    /**
     * Creates a new {@link ImmersiveVideoPlayerCoordinator}.
     *
     * @param activity The {@link Activity} context.
     * @param windowAndroid The {@link WindowAndroid}.
     * @param sessionManager The {@link XrSceneCoreSessionManager}.
     * @param delegate The {@link Delegate}.
     */
    public ImmersiveVideoPlayerCoordinator(
            Activity activity,
            WindowAndroid windowAndroid,
            XrSceneCoreSessionManager sessionManager,
            Delegate delegate) {
        mActivity = activity;
        mWindowAndroid = windowAndroid;
        mSessionManager = sessionManager;
        mDelegate = delegate;
    }

    private void ensureInitialized() {
        if (mCompositorView != null) return;

        mMediator = new ImmersiveVideoPlayerMediator(mModel);
        mCompositorView = createCompositorView(mActivity, mWindowAndroid, mSessionManager);
        mHolder =
                (mCompositorView.getView() instanceof XrSurfaceEntityView)
                        ? ((XrSurfaceEntityView) mCompositorView.getView()).getHolder()
                        : null;

        if (mHolder != null) {
            mHolder.getInteractableComponent().addOnClickListener(mDelegate::onPlayerPanelClicked);
            mHolder.getMovableComponent().addMoveListener(mOnMoveListener);
            mHolder.getResizableComponent()
                    .addResizeListener(
                            new XrResizableComponent.OnResizeListener() {
                                @Override
                                public void onResizeUpdate(float width, float height, float depth) {
                                    mDelegate.onPlayerPanelResized(width, height);
                                }

                                @Override
                                public void onResizeEnd(float width, float height, float depth) {
                                    mDelegate.onPlayerPanelResized(width, height);
                                }
                            });
            PropertyModelChangeProcessor.create(
                    mModel, mHolder, ImmersiveVideoPlayerViewBinder::bind);
        }
    }

    /** Shows the player panel. */
    public void show() {
        ensureInitialized();
        if (mHolder != null) {
            mSessionManager.getMainPanelEntity().setEntityEnabled(false);
            mHolder.setEntityEnabled(true);
        }
    }

    /** Disposes the player panel. */
    public void dispose() {
        if (mHolder != null) {
            mHolder.dispose();
            mHolder = null;
        }
    }

    /** Returns the {@link CompositorView}. */
    public CompositorView getCompositorView() {
        return assumeNonNull(mCompositorView);
    }

    /** Returns the {@link XrSurfaceEntityHolder}. */
    public @Nullable XrSurfaceEntityHolder getHolder() {
        return mHolder;
    }

    /**
     * Updates the video layout.
     *
     * @param stereoMode The stereo mode.
     * @param shape The shape.
     */
    public void updateVideoLayout(
            @XrSurfaceEntityStereoMode int stereoMode, @XrSurfaceEntityShape int shape) {
        if (mMediator != null) {
            mMediator.updateVideoLayout(stereoMode, shape);
        }
    }

    /**
     * Updates the pose.
     *
     * @param translation The translation.
     * @param rotation The rotation.
     */
    public void updatePose(float[] translation, float[] rotation) {
        if (mMediator != null) {
            mMediator.updatePose(translation, rotation);
        }
    }

    /**
     * Updates the player size.
     *
     * @param width The width in pixels.
     * @param height The height in pixels.
     */
    public void updatePlayerSize(int width, int height) {
        if (mMediator != null) {
            mMediator.updatePlayerSize(width, height);
        }
    }

    /** Sets whether the video player panel is interactable. */
    public void setInteractable(boolean interactable) {
        if (mHolder != null) {
            mHolder.getInteractableComponent().setInteractable(interactable);
        }
    }

    /** Returns the layout height of the video surface. */
    public float getLayoutHeight() {
        if (mHolder != null && mHolder.getSurfaceShape() == XrSurfaceEntityShape.QUAD) {
            return mHolder.getEntitySize().getHeight();
        }
        return getDefaultLayoutHeight();
    }

    private float getDefaultLayoutHeight() {
        Float defaultWidth = mModel.get(ImmersiveVideoPlayerProperties.DEFAULT_SPATIAL_WIDTH);
        Float defaultAspectRatio = mModel.get(ImmersiveVideoPlayerProperties.DEFAULT_ASPECT_RATIO);
        if (defaultWidth != null && defaultAspectRatio != null && defaultAspectRatio > 0) {
            return defaultWidth / defaultAspectRatio;
        }
        return 0f;
    }

    @VisibleForTesting
    protected CompositorView createCompositorView(
            Activity activity,
            WindowAndroid windowAndroid,
            XrSceneCoreSessionManager sessionManager) {
        return CompositorViewFactory.create(
                activity,
                windowAndroid,
                new ThinWebViewConstraints(),
                sessionManager,
                XrSurfaceEntityShape.QUAD);
    }
}
