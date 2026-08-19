// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.immersive_playback.components;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.util.SizeF;
import android.view.View;

import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.media.immersive_playback.ImmersiveVideoPlaybackDelegate;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.xr.scenecore.XrEntityHolder;
import org.chromium.ui.xr.scenecore.XrMovableComponent;
import org.chromium.ui.xr.scenecore.XrPanelEntityHolder;
import org.chromium.ui.xr.scenecore.XrPixelDensity;
import org.chromium.ui.xr.scenecore.XrPose;
import org.chromium.ui.xr.scenecore.XrSceneCoreSessionManager;
import org.chromium.ui.xr.scenecore.XrSpace;

/** Coordinator for the media control panel. Owns the model, view, mediator, and spatial holder. */
@NullMarked
public class ImmersiveVideoControlCoordinator {
    /** Delegate for controlling media playback in XR. */
    public interface Delegate extends ImmersiveVideoPlaybackDelegate {
        /** Called when the format button is clicked. */
        void onFormatClicked();

        /** Called when hover state of the control panel changes. */
        void onControlPanelHoverChanged(boolean hovered);

        /** Called when movement of the control panel starts or ends. */
        void onControlPanelMoveChanged(boolean isMoving);

        /** Called when the pose of the control panel changes during movement. */
        void onControlPanelPoseChanged(XrPose pose);

        /** Called when accessibility focus state of the control panel changes. */
        void onControlPanelAccessibilityFocusChanged(boolean focused);
    }

    private final PropertyModel mModel;
    private final XrPixelDensity mPixelDensity;
    private final Activity mActivity;
    private final XrSceneCoreSessionManager mSessionManager;
    private final Delegate mVideoControlDelegate;
    private final ImmersiveVideoControlMediator mMediator;
    private final XrMovableComponent.OnMoveListener mOnMoveListener =
            new XrMovableComponent.OnMoveListener() {
                @Override
                public void onMoveStart(XrPose pose, float scale) {
                    mVideoControlDelegate.onControlPanelMoveChanged(true);
                }

                @Override
                public void onMoveUpdate(XrPose pose, float scale) {
                    mVideoControlDelegate.onControlPanelPoseChanged(pose);
                }

                @Override
                public void onMoveEnd(XrPose pose, float scale) {
                    mVideoControlDelegate.onControlPanelMoveChanged(false);
                    mVideoControlDelegate.onControlPanelPoseChanged(pose);
                }
            };

    private @Nullable ImmersiveVideoControlView mView;
    private @Nullable XrPanelEntityHolder<?> mHolder;
    private @Nullable PropertyModelChangeProcessor<
                    PropertyModel, ImmersiveVideoControlSpatialView, PropertyKey>
            mModelChangeProcessor;
    private boolean mIsShowing;
    private boolean mIsDisposed;

    /**
     * Creates a new {@link ImmersiveVideoControlCoordinator}.
     *
     * @param activity The {@link Activity} context.
     * @param sessionManager The {@link XrSceneCoreSessionManager}.
     * @param videoControlDelegate The {@link Delegate} for handling user interactions.
     */
    public ImmersiveVideoControlCoordinator(
            Activity activity,
            XrSceneCoreSessionManager sessionManager,
            Delegate videoControlDelegate) {
        mActivity = activity;
        mSessionManager = sessionManager;
        mVideoControlDelegate = videoControlDelegate;
        mPixelDensity = sessionManager.getPixelDensity();

        mModel =
                new PropertyModel.Builder(ImmersiveVideoControlProperties.ALL_KEYS)
                        .with(ImmersiveVideoControlProperties.DEFAULT_PIXEL_DENSITY, mPixelDensity)
                        .with(ImmersiveVideoControlProperties.DEFAULT_WIDTH_DP, 700)
                        .with(ImmersiveVideoControlProperties.DEFAULT_HEIGHT_DP, 80)
                        .with(ImmersiveVideoControlProperties.DURATION_MS, 0L)
                        .with(ImmersiveVideoControlProperties.POSITION_MS, 0L)
                        .with(ImmersiveVideoControlProperties.PLAYBACK_RATE, 1.0)
                        .with(ImmersiveVideoControlProperties.IS_PLAYING, false)
                        .with(ImmersiveVideoControlProperties.FORMAT_BUTTON_SELECTED, false)
                        .with(ImmersiveVideoControlProperties.IS_MOVABLE, false)
                        .build();
        mMediator = new ImmersiveVideoControlMediator(mModel, mVideoControlDelegate);
    }

    private void ensureInitialized() {
        if (mHolder != null) return;

        mView = createView(mActivity, mMediator);
        mHolder = mSessionManager.createPanelEntity(mView, "MediaControlPanel");
        mHolder.getMovableComponent().addMoveListener(mOnMoveListener);

        mModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mModel,
                        new ImmersiveVideoControlSpatialView(mView, mHolder),
                        ImmersiveVideoControlViewBinder::bind);
    }

    @VisibleForTesting
    ImmersiveVideoControlView createView(
            Activity activity, ImmersiveVideoControlView.UserInteractionListener listener) {
        return new ImmersiveVideoControlView(activity, listener);
    }

    /**
     * Shows the control panel by attaching it to the given parent {@link XrEntityHolder}.
     *
     * @param parent The parent entity to attach to.
     */
    public void show(XrEntityHolder<?> parent) {
        if (mIsDisposed) return;

        ensureInitialized();
        mIsShowing = true;
        setParent(parent);

        if (mHolder != null) {
            mHolder.setEntityEnabled(true);
        }
        if (mView != null) {
            mView.setVisibility(View.VISIBLE);
            mView.setHoverListener(mVideoControlDelegate::onControlPanelHoverChanged);
            mView.setAccessibilityFocusListener(
                    mVideoControlDelegate::onControlPanelAccessibilityFocusChanged);
        }
        mMediator.setVisible(true);
    }

    /**
     * Sets the parent entity for the control panel.
     *
     * @param parent The parent entity to attach to.
     */
    public void setParent(XrEntityHolder<?> parent) {
        if (mIsShowing && mHolder != null) {
            mHolder.setParent(parent);
        }
    }

    /** Dismisses the control panel. */
    public void dismiss() {
        if (mIsDisposed || !mIsShowing) return;

        mIsShowing = false;
        mMediator.setVisible(false);
        if (mView != null) {
            mView.setHoverListener(null);
            mView.setAccessibilityFocusListener(null);
            mView.setVisibility(View.GONE);
        }
        if (mHolder != null && !mHolder.isDisposed()) {
            mHolder.setEntityEnabled(false);
            mHolder.setParent(null);
        }
    }

    /** Disposes the control panel. */
    public void dispose() {
        if (mIsDisposed) return;

        mIsDisposed = true;
        mIsShowing = false;
        mMediator.destroy();
        if (mModelChangeProcessor != null) {
            mModelChangeProcessor.destroy();
            mModelChangeProcessor = null;
        }
        if (mView != null) {
            mView.setHoverListener(null);
            mView.setAccessibilityFocusListener(null);
            mView.setVisibility(View.GONE);
        }
        if (mHolder != null && !mHolder.isDisposed()) {
            mHolder.setEntityEnabled(false);
            mHolder.setParent(null);
            mHolder.getMovableComponent().removeMoveListener(mOnMoveListener);
            mHolder.dispose();
        }
        mHolder = null;
        mView = null;
    }

    /** Returns true if the control panel is currently showing, false otherwise. */
    public boolean isShowing() {
        return mIsShowing;
    }

    /** Returns the {@link XrPanelEntityHolder} for the control panel. */
    public @Nullable XrPanelEntityHolder<?> getHolder() {
        return mHolder;
    }

    /** Returns the size of the control panel in meters. */
    public SizeF getSize() {
        if (mHolder != null) {
            return mHolder.getEntitySize();
        }
        int widthDp = mModel.get(ImmersiveVideoControlProperties.DEFAULT_WIDTH_DP);
        int heightDp = mModel.get(ImmersiveVideoControlProperties.DEFAULT_HEIGHT_DP);
        return new SizeF(
                mPixelDensity.convertDpToMeters(widthDp),
                mPixelDensity.convertDpToMeters(heightDp));
    }

    /**
     * Updates the pose of the control panel relative to its parent.
     *
     * @param translation The translation from the parent {@link XrSpace}.
     * @param rotation The rotation from the parent {@link XrSpace}.
     */
    public void updatePose(XrPose pose) {
        mMediator.updatePose(pose);
    }

    /**
     * Sets whether the control panel is movable by the user.
     *
     * @param isMovable True if movable, false otherwise.
     */
    public void setMovable(boolean isMovable) {
        mMediator.setMovable(isMovable);
    }

    /**
     * Updates the media position displayed in the control panel.
     *
     * @param durationMs The total duration in milliseconds.
     * @param positionMs The current position in milliseconds.
     * @param playbackRate The current playback rate.
     */
    public void updateMediaPosition(long durationMs, long positionMs, double playbackRate) {
        mMediator.updateMediaPosition(durationMs, positionMs, playbackRate);
    }

    /**
     * Updates the playback state displayed in the control panel.
     *
     * @param isPlaying True if playing, false otherwise.
     */
    public void updatePlaybackState(boolean isPlaying) {
        mMediator.updatePlaybackState(isPlaying);
    }

    /**
     * Sets the selected state of the format button.
     *
     * @param selected True if selected, false otherwise.
     */
    public void setFormatButtonSelected(boolean selected) {
        mMediator.setFormatButtonSelected(selected);
    }

    /** Requests accessibility focus on the format button. */
    public void requestFormatButtonAccessibilityFocus() {
        if (mView != null) {
            mView.requestFormatButtonAccessibilityFocus();
        }
    }

    /** Cancels any pending accessibility focus requests. */
    public void cancelPendingAccessibilityFocusRequests() {
        if (mView != null) {
            mView.cancelPendingAccessibilityFocusRequests();
        }
    }

    /**
     * Sets the importance of the control panel for accessibility.
     *
     * @param mode The accessibility importance mode.
     */
    public void setImportantForAccessibility(int mode) {
        if (mView != null) {
            mView.setImportantForAccessibility(mode);
        }
    }

    public ImmersiveVideoControlView getControlPanelForTesting() {
        return assumeNonNull(mView);
    }

    PropertyModel getModelForTesting() {
        return mModel;
    }
}
