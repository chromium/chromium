// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.immersive_playback.components;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.xr.scenecore.XrPixelDensity;
import org.chromium.ui.xr.scenecore.XrPose;
import org.chromium.ui.xr.scenecore.XrSpace;

/** View binder for the immersive video control panel. */
@NullMarked
public class ImmersiveVideoControlViewBinder {
    /**
     * Binds the {@link PropertyModel} to the view for a specific property key.
     *
     * @param model The {@link PropertyModel} to update.
     * @param view The {@link ImmersiveVideoControlSpatialView}.
     * @param propertyKey The {@link PropertyKey} to bind.
     */
    public static void bind(
            PropertyModel model, ImmersiveVideoControlSpatialView view, PropertyKey propertyKey) {
        if (propertyKey == ImmersiveVideoControlProperties.POSITION_TEXT) {
            view.androidView.setPositionText(
                    model.get(ImmersiveVideoControlProperties.POSITION_TEXT));
        } else if (propertyKey == ImmersiveVideoControlProperties.DURATION_TEXT) {
            view.androidView.setDurationText(
                    model.get(ImmersiveVideoControlProperties.DURATION_TEXT));
        } else if (propertyKey == ImmersiveVideoControlProperties.PROGRESS) {
            view.androidView.setProgress(model.get(ImmersiveVideoControlProperties.PROGRESS));
        } else if (propertyKey == ImmersiveVideoControlProperties.MAX_PROGRESS) {
            view.androidView.setMaxProgress(
                    model.get(ImmersiveVideoControlProperties.MAX_PROGRESS));
        } else if (propertyKey == ImmersiveVideoControlProperties.IS_PLAYING) {
            view.androidView.setPlaybackState(
                    model.get(ImmersiveVideoControlProperties.IS_PLAYING));
        } else if (propertyKey == ImmersiveVideoControlProperties.FORMAT_BUTTON_SELECTED) {
            view.androidView.setFormatButtonSelected(
                    model.get(ImmersiveVideoControlProperties.FORMAT_BUTTON_SELECTED));
        } else if (propertyKey == ImmersiveVideoControlProperties.DEFAULT_WIDTH_DP
                || propertyKey == ImmersiveVideoControlProperties.DEFAULT_HEIGHT_DP) {
            int widthDp = model.get(ImmersiveVideoControlProperties.DEFAULT_WIDTH_DP);
            int heightDp = model.get(ImmersiveVideoControlProperties.DEFAULT_HEIGHT_DP);
            XrPixelDensity pixelDensity =
                    model.get(ImmersiveVideoControlProperties.DEFAULT_PIXEL_DENSITY);
            if (widthDp > 0 && heightDp > 0 && pixelDensity != null) {
                float widthMeters = pixelDensity.convertDpToMeters(widthDp);
                float heightMeters = pixelDensity.convertDpToMeters(heightDp);
                view.spatialEntityHolder.setEntitySize(widthMeters, heightMeters);
                view.spatialEntityHolder.setEntityCornerRadius(heightMeters / 2f);
            }
        } else if (propertyKey == ImmersiveVideoControlProperties.IS_MOVABLE) {
            view.spatialEntityHolder
                    .getMovableComponent()
                    .setMovable(model.get(ImmersiveVideoControlProperties.IS_MOVABLE), false);
        } else if (propertyKey == ImmersiveVideoControlProperties.POSE) {
            XrPose pose = model.get(ImmersiveVideoControlProperties.POSE);
            if (pose != null) {
                view.spatialEntityHolder.setEntityPose(pose, XrSpace.PARENT);
            }
        }
    }
}
