// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.immersive_playback.components;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.xr.scenecore.XrSurfaceEntityShape;
import org.chromium.ui.xr.scenecore.XrSurfaceEntityStereoMode;

/** Mediator for the video player surface in immersive video playback. */
@NullMarked
public class ImmersiveVideoPlayerMediator {
    private static final String TAG = "ImmersiveVideoPlayer";

    private final PropertyModel mModel;

    /**
     * Creates a new {@link ImmersiveVideoPlayerMediator}.
     *
     * @param model The {@link PropertyModel} to update.
     */
    public ImmersiveVideoPlayerMediator(PropertyModel model) {
        mModel = model;
    }

    /**
     * Updates the video layout in the model.
     *
     * @param stereoMode The {@link XrSurfaceEntityStereoMode}.
     * @param shape The {@link XrSurfaceEntityShape}.
     */
    public void updateVideoLayout(
            @XrSurfaceEntityStereoMode int stereoMode, @XrSurfaceEntityShape int shape) {
        mModel.set(ImmersiveVideoPlayerProperties.STEREO_MODE, stereoMode);
        mModel.set(ImmersiveVideoPlayerProperties.SHAPE, shape);
    }

    /**
     * Updates the player panel pose.
     *
     * @param translation The translation of the player panel.
     * @param rotation The rotation of the player panel.
     */
    public void updatePose(float[] translation, float[] rotation) {
        mModel.set(ImmersiveVideoPlayerProperties.POSE_TRANSLATION, translation);
        mModel.set(ImmersiveVideoPlayerProperties.POSE_ROTATION, rotation);
    }

    /** Updates the player size and aspect ratio. */
    public void updatePlayerSize(int width, int height) {
        mModel.set(ImmersiveVideoPlayerProperties.PIXEL_WIDTH, width);
        mModel.set(ImmersiveVideoPlayerProperties.PIXEL_HEIGHT, height);
    }
}
