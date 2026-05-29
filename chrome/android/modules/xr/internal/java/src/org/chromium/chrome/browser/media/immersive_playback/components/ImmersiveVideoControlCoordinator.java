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
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.xr.scenecore.XrEntityHolder;
import org.chromium.ui.xr.scenecore.XrMovableComponent;
import org.chromium.ui.xr.scenecore.XrPanelEntityHolder;
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
        void onControlPanelPoseChanged(float[] translation, float[] rotation);
    }

    private final PropertyModel mModel =
            new PropertyModel.Builder(ImmersiveVideoControlProperties.ALL_KEYS)
                    .with(ImmersiveVideoControlProperties.DEFAULT_SPATIAL_WIDTH, 0.7f)
                    .with(ImmersiveVideoControlProperties.DEFAULT_SPATIAL_HEIGHT, 0.08f)
                    .with(ImmersiveVideoControlProperties.DURATION_MS, 0L)
                    .with(ImmersiveVideoControlProperties.POSITION_MS, 0L)
                    .with(ImmersiveVideoControlProperties.PLAYBACK_RATE, 1.0)
                    .with(ImmersiveVideoControlProperties.IS_PLAYING, false)
                    .with(ImmersiveVideoControlProperties.FORMAT_BUTTON_SELECTED, false)
                    .with(ImmersiveVideoControlProperties.IS_MOVABLE, false)
                    .build();

    private final Activity mActivity;
    private final XrSceneCoreSessionManager mSessionManager;
    private final Delegate mVideoControlDelegate;
    private final XrMovableComponent.OnMoveListener mOnMoveListener =
            new XrMovableComponent.OnMoveListener() {
                @Override
                public void onMoveStart(float[] translation, float[] rotation, float scale) {
                    mVideoControlDelegate.onControlPanelMoveChanged(true);
                }

                @Override
                public void onMoveUpdate(float[] translation, float[] rotation, float scale) {}

                @Override
                public void onMoveEnd(float[] translation, float[] rotation, float scale) {
                    mVideoControlDelegate.onControlPanelMoveChanged(false);
                    mVideoControlDelegate.onControlPanelPoseChanged(translation, rotation);
                }
            };

    private @Nullable ImmersiveVideoControlView mView;
    private @Nullable ImmersiveVideoControlMediator mMediator;
    private @Nullable XrPanelEntityHolder<?> mHolder;

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
    }

    private void ensureInitialized() {
        if (mHolder != null) return;

        mMediator = new ImmersiveVideoControlMediator(mModel, mVideoControlDelegate);
        mView = createView(mActivity, mMediator);
        mHolder = mSessionManager.createPanelEntity(mView, "MediaControlPanel");
        mHolder.getMovableComponent().addMoveListener(mOnMoveListener);

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
        ensureInitialized();

        if (mHolder != null) {
            mHolder.setParent(parent);
            mHolder.setEntityEnabled(true);
        }
        if (mView != null) {
            mView.setVisibility(View.VISIBLE);
            mView.setHoverListener(mVideoControlDelegate::onControlPanelHoverChanged);
        }
    }

    /** Dismisses the control panel. */
    public void dismiss() {
        if (mHolder != null && !mHolder.isDisposed()) {
            mHolder.setEntityEnabled(false);
            mHolder.setParent(null);
        }

        if (mView != null) {
            mView.setVisibility(View.GONE);
            mView.setHoverListener(null);
        }
    }

    /** Disposes the control panel. */
    public void dispose() {
        dismiss();
        if (mHolder != null) {
            mHolder.dispose();
            mHolder = null;
        }
    }

    /** Returns true if the control panel is currently showing, false otherwise. */
    public boolean isShowing() {
        return mHolder != null && mHolder.getParent() != null;
    }

    /** Returns the {@link XrPanelEntityHolder} for the control panel. */
    public @Nullable XrPanelEntityHolder<?> getHolder() {
        return mHolder;
    }

    /** Returns the size of the control panel in spatial units. */
    public SizeF getSize() {
        if (mHolder != null) {
            return mHolder.getEntitySize();
        }
        return new SizeF(
                mModel.get(ImmersiveVideoControlProperties.DEFAULT_SPATIAL_WIDTH),
                mModel.get(ImmersiveVideoControlProperties.DEFAULT_SPATIAL_HEIGHT));
    }

    /**
     * Updates the pose of the control panel relative to its parent.
     *
     * @param translation The translation from the parent {@link XrSpace}.
     * @param rotation The rotation from the parent {@link XrSpace}.
     */
    public void updatePose(float[] translation, float[] rotation) {
        if (mMediator != null) {
            mMediator.updatePose(translation, rotation);
        }
    }

    /**
     * Sets whether the control panel is movable by the user.
     *
     * @param isMovable True if movable, false otherwise.
     */
    public void setMovable(boolean isMovable) {
        if (mMediator != null) {
            mMediator.setMovable(isMovable);
        }
    }

    /**
     * Updates the media position displayed in the control panel.
     *
     * @param durationMs The total duration in milliseconds.
     * @param positionMs The current position in milliseconds.
     * @param playbackRate The current playback rate.
     */
    public void updateMediaPosition(long durationMs, long positionMs, double playbackRate) {
        if (mMediator != null) {
            mMediator.updateMediaPosition(durationMs, positionMs, playbackRate);
        }
    }

    /**
     * Updates the playback state displayed in the control panel.
     *
     * @param isPlaying True if playing, false otherwise.
     */
    public void updatePlaybackState(boolean isPlaying) {
        if (mMediator != null) {
            mMediator.updatePlaybackState(isPlaying);
        }
    }

    /**
     * Sets the selected state of the format button.
     *
     * @param selected True if selected, false otherwise.
     */
    public void setFormatButtonSelected(boolean selected) {
        if (mMediator != null) {
            mMediator.setFormatButtonSelected(selected);
        }
    }

    public ImmersiveVideoControlView getControlPanelForTesting() {
        return assumeNonNull(mView);
    }
}
