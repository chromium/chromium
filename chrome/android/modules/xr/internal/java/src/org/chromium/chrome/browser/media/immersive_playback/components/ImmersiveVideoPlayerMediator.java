// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.immersive_playback.components;

import org.chromium.base.lifetime.DestroyChecker;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.xr.scenecore.XrPose;
import org.chromium.ui.xr.scenecore.XrSurfaceEntityShape;
import org.chromium.ui.xr.scenecore.XrSurfaceEntityStereoMode;

/** Mediator for the video player surface in immersive video playback. */
@NullMarked
public class ImmersiveVideoPlayerMediator implements Destroyable {
    private static final String TAG = "ImmersiveVideoPlayer";

    private final PropertyModel mModel;
    private final DestroyChecker mDestroyChecker = new DestroyChecker();

    /**
     * Creates a new {@link ImmersiveVideoPlayerMediator}.
     *
     * @param model The {@link PropertyModel} to update.
     */
    public ImmersiveVideoPlayerMediator(PropertyModel model) {
        mModel = model;
    }

    /** Destroys the mediator. */
    @Override
    public void destroy() {
        if (mDestroyChecker.isDestroyed()) return;
        mDestroyChecker.destroy();
    }

    /**
     * Updates the video layout in the model.
     *
     * @param stereoMode The {@link XrSurfaceEntityStereoMode}.
     * @param shape The {@link XrSurfaceEntityShape}.
     */
    public void updateVideoLayout(
            @XrSurfaceEntityStereoMode int stereoMode, @XrSurfaceEntityShape int shape) {
        if (mDestroyChecker.isDestroyed()) return;
        mModel.set(ImmersiveVideoPlayerProperties.STEREO_MODE, stereoMode);
        mModel.set(ImmersiveVideoPlayerProperties.SHAPE, shape);
    }

    /**
     * Updates the player panel pose.
     *
     * @param pose The pose of the player panel.
     */
    public void updatePose(XrPose pose) {
        if (mDestroyChecker.isDestroyed()) return;
        mModel.set(ImmersiveVideoPlayerProperties.POSE, pose);
    }

    /** Updates the player size and aspect ratio. */
    public void updatePlayerSize(int width, int height) {
        if (mDestroyChecker.isDestroyed()) return;
        mModel.set(ImmersiveVideoPlayerProperties.PIXEL_WIDTH, width);
        mModel.set(ImmersiveVideoPlayerProperties.PIXEL_HEIGHT, height);
    }
}
