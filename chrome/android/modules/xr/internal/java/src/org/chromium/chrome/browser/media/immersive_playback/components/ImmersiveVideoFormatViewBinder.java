// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.immersive_playback.components;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.media.immersive_playback.ImmersiveVideoFormatRadioGroup;
import org.chromium.chrome.browser.media.immersive_playback.ImmersiveVideoFormatRadioGroup.FormatOption;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.xr.scenecore.XrPixelDensity;
import org.chromium.ui.xr.scenecore.XrPose;
import org.chromium.ui.xr.scenecore.XrSpace;
import org.chromium.ui.xr.scenecore.XrVector3;

/** View binder for the immersive playback format selection panel. */
@NullMarked
public class ImmersiveVideoFormatViewBinder {

    /**
     * Binds the model to the view for a specific property key.
     *
     * @param model The property model.
     * @param view The spatial view wrapper.
     * @param propertyKey The property key to bind.
     */
    public static void bind(
            PropertyModel model, ImmersiveVideoFormatSpatialView view, PropertyKey propertyKey) {
        if (propertyKey == ImmersiveVideoFormatProperties.SELECTED_STEREO_MODE
                || propertyKey == ImmersiveVideoFormatProperties.SELECTED_PROJECTION_TYPE) {
            Integer stereoMode = model.get(ImmersiveVideoFormatProperties.SELECTED_STEREO_MODE);
            Integer projectionType =
                    model.get(ImmersiveVideoFormatProperties.SELECTED_PROJECTION_TYPE);

            if (stereoMode != null && projectionType != null) {
                ImmersiveVideoFormatRadioGroup radioGroup = view.androidView.getRadioGroup();
                FormatOption selected = radioGroup.getSelectedOption();
                if (selected == null
                        || selected.stereoMode != stereoMode
                        || selected.projectionType != projectionType) {
                    radioGroup.checkOption(stereoMode, projectionType);
                }
            }
        } else if (propertyKey == ImmersiveVideoFormatProperties.DEFAULT_WIDTH_DP
                || propertyKey == ImmersiveVideoFormatProperties.HEIGHT_DP) {
            int widthDp = model.get(ImmersiveVideoFormatProperties.DEFAULT_WIDTH_DP);
            int heightDp = model.get(ImmersiveVideoFormatProperties.HEIGHT_DP);
            XrPixelDensity pixelDensity =
                    model.get(ImmersiveVideoFormatProperties.DEFAULT_PIXEL_DENSITY);
            if (widthDp > 0 && heightDp > 0 && pixelDensity != null) {
                float widthMeters = pixelDensity.convertDpToMeters(widthDp);
                float heightMeters = pixelDensity.convertDpToMeters(heightDp);
                view.spatialEntityHolder.setEntitySize(widthMeters, heightMeters);
                updatePose(model, view);
            }
        } else if (propertyKey == ImmersiveVideoFormatProperties.PARENT_WIDTH
                || propertyKey == ImmersiveVideoFormatProperties.PARENT_HEIGHT) {
            updatePose(model, view);
        } else if (propertyKey == ImmersiveVideoFormatProperties.DEFAULT_CORNER_RADIUS_DP) {
            int radiusDp = model.get(ImmersiveVideoFormatProperties.DEFAULT_CORNER_RADIUS_DP);
            XrPixelDensity pixelDensity =
                    model.get(ImmersiveVideoFormatProperties.DEFAULT_PIXEL_DENSITY);
            if (radiusDp > 0 && pixelDensity != null) {
                view.spatialEntityHolder.setEntityCornerRadius(
                        pixelDensity.convertDpToMeters(radiusDp));
            }
        } else if (propertyKey == ImmersiveVideoFormatProperties.RECOMMENDED_STEREO_MODE
                || propertyKey == ImmersiveVideoFormatProperties.RECOMMENDED_PROJECTION_TYPE) {
            Integer stereoMode = model.get(ImmersiveVideoFormatProperties.RECOMMENDED_STEREO_MODE);
            Integer projectionType =
                    model.get(ImmersiveVideoFormatProperties.RECOMMENDED_PROJECTION_TYPE);
            if (stereoMode != null && projectionType != null) {
                ImmersiveVideoFormatRadioGroup radioGroup = view.androidView.getRadioGroup();
                radioGroup.setRecommendedOption(stereoMode, projectionType);
                radioGroup.checkOption(stereoMode, projectionType);
            }
        }
    }

    /**
     * Updates the pose of the format selection panel relative to its parent entity.
     *
     * <p>Positions the panel directly above and aligned with the right edge of the parent entity
     * (e.g., the media control panel).
     */
    private static void updatePose(PropertyModel model, ImmersiveVideoFormatSpatialView view) {
        int widthDp = model.get(ImmersiveVideoFormatProperties.DEFAULT_WIDTH_DP);
        int heightDp = model.get(ImmersiveVideoFormatProperties.HEIGHT_DP);
        Float parentWidth = model.get(ImmersiveVideoFormatProperties.PARENT_WIDTH);
        Float parentHeight = model.get(ImmersiveVideoFormatProperties.PARENT_HEIGHT);
        XrPixelDensity pixelDensity =
                model.get(ImmersiveVideoFormatProperties.DEFAULT_PIXEL_DENSITY);

        if (widthDp > 0
                && heightDp > 0
                && parentWidth != null
                && parentHeight != null
                && parentWidth > 0f
                && parentHeight > 0f
                && pixelDensity != null) {
            float widthMeters = pixelDensity.convertDpToMeters(widthDp);
            float heightMeters = pixelDensity.convertDpToMeters(heightDp);
            XrPose pose =
                    XrPose.create(
                            XrVector3.create(
                                    parentWidth / 2 - widthMeters / 2,
                                    parentHeight / 2 + heightMeters / 2,
                                    0f));
            view.spatialEntityHolder.setEntityPose(pose, XrSpace.PARENT);
        }
    }
}
