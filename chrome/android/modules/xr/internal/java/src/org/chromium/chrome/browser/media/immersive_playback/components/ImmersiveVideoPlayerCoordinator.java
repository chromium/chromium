// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.immersive_playback.components;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.os.Bundle;
import android.view.View;
import android.view.accessibility.AccessibilityEvent;
import android.view.accessibility.AccessibilityNodeInfo;

import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.thinwebview.CompositorView;
import org.chromium.components.thinwebview.CompositorViewFactory;
import org.chromium.components.thinwebview.ThinWebViewConstraints;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.xr.scenecore.XrCurvedSurfaceEntityHolder;
import org.chromium.ui.xr.scenecore.XrFloatSize3d;
import org.chromium.ui.xr.scenecore.XrInteractableComponent;
import org.chromium.ui.xr.scenecore.XrMovableComponent;
import org.chromium.ui.xr.scenecore.XrPose;
import org.chromium.ui.xr.scenecore.XrResizableComponent;
import org.chromium.ui.xr.scenecore.XrSceneCoreSessionManager;
import org.chromium.ui.xr.scenecore.XrSurfaceEntityHolder;
import org.chromium.ui.xr.scenecore.XrSurfaceEntityShape;
import org.chromium.ui.xr.scenecore.XrSurfaceEntityStereoMode;
import org.chromium.ui.xr.scenecore.XrSurfaceEntityView;
import org.chromium.ui.xr.scenecore.XrVector3;

/** Coordinator for the video player panel. */
@NullMarked
public class ImmersiveVideoPlayerCoordinator {
    /** Delegate for player panel interactions and pose changes. */
    public interface Delegate {
        void onPlayerPanelClicked();

        void onPlayerPanelPoseChanged(XrPose pose);

        void onPlayerPanelResized(XrFloatSize3d size);

        void onPlayerPanelDragStart(XrVector3 origin, XrVector3 direction);

        void onPlayerPanelDragUpdate(XrVector3 origin, XrVector3 direction);

        void onPlayerPanelDragEnd(XrVector3 origin, XrVector3 direction);
    }

    private final PropertyModel mModel =
            new PropertyModel.Builder(ImmersiveVideoPlayerProperties.ALL_KEYS)
                    .with(ImmersiveVideoPlayerProperties.DEFAULT_SPATIAL_WIDTH, 1f)
                    .with(ImmersiveVideoPlayerProperties.DEFAULT_MIN_WIDTH, 1f)
                    .with(ImmersiveVideoPlayerProperties.DEFAULT_MAX_WIDTH, 3f)
                    .with(ImmersiveVideoPlayerProperties.DEFAULT_CURVE_RADIUS, 5f)
                    .with(ImmersiveVideoPlayerProperties.DEFAULT_ASPECT_RATIO, 16f / 9f)
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
                public void onMoveStart(XrPose pose, float scale) {}

                @Override
                public void onMoveUpdate(XrPose pose, float scale) {
                    mDelegate.onPlayerPanelPoseChanged(pose);
                }

                @Override
                public void onMoveEnd(XrPose pose, float scale) {
                    mDelegate.onPlayerPanelPoseChanged(pose);
                }
            };

    private final XrResizableComponent.OnResizeListener mOnResizeListener =
            new XrResizableComponent.OnResizeListener() {
                @Override
                public void onResizeUpdate(XrFloatSize3d size) {
                    mDelegate.onPlayerPanelResized(size);
                }

                @Override
                public void onResizeEnd(XrFloatSize3d size) {
                    mDelegate.onPlayerPanelResized(size);
                }
            };

    private final XrInteractableComponent.OnDragListener mOnDragListener =
            new XrInteractableComponent.OnDragListener() {
                @Override
                public void onDragStart(XrVector3 origin, XrVector3 direction) {
                    mDelegate.onPlayerPanelDragStart(origin, direction);
                }

                @Override
                public void onDragUpdate(XrVector3 origin, XrVector3 direction) {
                    mDelegate.onPlayerPanelDragUpdate(origin, direction);
                }

                @Override
                public void onDragEnd(XrVector3 origin, XrVector3 direction) {
                    mDelegate.onPlayerPanelDragEnd(origin, direction);
                }
            };

    private final View.AccessibilityDelegate mAccessibilityDelegate =
            new View.AccessibilityDelegate() {
                @Override
                public void onInitializeAccessibilityNodeInfo(
                        View host, AccessibilityNodeInfo info) {
                    super.onInitializeAccessibilityNodeInfo(host, info);
                    info.addAction(AccessibilityNodeInfo.AccessibilityAction.ACTION_CLICK);
                    info.setClickable(true);
                }

                @Override
                public boolean performAccessibilityAction(
                        View host, int action, @Nullable Bundle args) {
                    if (action == AccessibilityNodeInfo.ACTION_CLICK) {
                        mDelegate.onPlayerPanelClicked();
                        return true;
                    }
                    return super.performAccessibilityAction(host, action, args);
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
        View playerView = mCompositorView.getView();
        if (playerView != null) {
            playerView.setFocusable(true);
            playerView.setFocusableInTouchMode(true);
            playerView.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_YES);
            playerView.setContentDescription(
                    mActivity.getString(org.chromium.chrome.R.string.accessibility_video_player));
            playerView.setBackgroundColor(android.graphics.Color.TRANSPARENT);
            playerView.setAccessibilityDelegate(mAccessibilityDelegate);
        }
        mHolder =
                (playerView instanceof XrSurfaceEntityView)
                        ? ((XrSurfaceEntityView) playerView).getHolder()
                        : null;

        if (mHolder != null) {
            mHolder.getInteractableComponent().addOnClickListener(mDelegate::onPlayerPanelClicked);
            mHolder.getInteractableComponent().addOnDragListener(mOnDragListener);
            mHolder.getMovableComponent().addMoveListener(mOnMoveListener);
            mHolder.getResizableComponent().addResizeListener(mOnResizeListener);
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
        View playerView = mCompositorView != null ? mCompositorView.getView() : null;
        if (playerView != null) {
            playerView.requestFocus();
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

    /** Updates the pose. */
    public void updatePose(XrPose pose) {
        if (mMediator != null) {
            mMediator.updatePose(pose);
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

    /** Requests accessibility focus on the player view. */
    @SuppressLint("AccessibilityFocus")
    public void requestFocusForAccessibility() {
        View playerView = mCompositorView != null ? mCompositorView.getView() : null;
        if (playerView != null) {
            playerView.post(
                    () -> {
                        playerView.requestFocus();
                        playerView.performAccessibilityAction(
                                android.view.accessibility.AccessibilityNodeInfo
                                        .ACTION_ACCESSIBILITY_FOCUS,
                                null);
                        playerView.sendAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_FOCUSED);
                    });
        }
    }

    /** Returns the layout height of the video surface. */
    public float getLayoutHeight() {
        if (mHolder != null && mHolder.getSurfaceShape() == XrSurfaceEntityShape.QUAD) {
            return mHolder.getEntitySize().getHeight();
        }
        return getDefaultLayoutHeight();
    }

    /** Returns the curve radius. */
    public float getCurveRadius() {
        if (mHolder != null && mHolder instanceof XrCurvedSurfaceEntityHolder) {
            return ((XrCurvedSurfaceEntityHolder) mHolder).getEntityRadius();
        }
        Float defaultRadius = mModel.get(ImmersiveVideoPlayerProperties.DEFAULT_CURVE_RADIUS);
        return defaultRadius != null ? defaultRadius : 0f;
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
