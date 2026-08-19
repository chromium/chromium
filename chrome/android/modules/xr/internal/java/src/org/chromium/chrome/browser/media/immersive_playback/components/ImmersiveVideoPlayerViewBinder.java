// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.immersive_playback.components;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.xr.scenecore.XrCurvedSurfaceEntityHolder;
import org.chromium.ui.xr.scenecore.XrFloatSize3d;
import org.chromium.ui.xr.scenecore.XrPixelDensity;
import org.chromium.ui.xr.scenecore.XrPose;
import org.chromium.ui.xr.scenecore.XrSpace;
import org.chromium.ui.xr.scenecore.XrSurfaceEntityHolder;
import org.chromium.ui.xr.scenecore.XrSurfaceEntityShape;

/** View binder for the immersive video player panel. */
@NullMarked
public class ImmersiveVideoPlayerViewBinder {
    /**
     * Binds the model to the view for a specific property key.
     *
     * @param model The {@link PropertyModel}.
     * @param view The {@link XrSurfaceEntityHolder}.
     * @param propertyKey The {@link PropertyKey} to bind.
     */
    public static void bind(
            PropertyModel model, @Nullable XrSurfaceEntityHolder view, PropertyKey propertyKey) {
        if (view == null) {
            return;
        }

        if (propertyKey == ImmersiveVideoPlayerProperties.STEREO_MODE) {
            view.setSurfaceStereoMode(model.get(ImmersiveVideoPlayerProperties.STEREO_MODE));
        } else if (propertyKey == ImmersiveVideoPlayerProperties.SHAPE) {
            int shape = model.get(ImmersiveVideoPlayerProperties.SHAPE);
            boolean resizable = shape == XrSurfaceEntityShape.QUAD;
            boolean movable = shape == XrSurfaceEntityShape.QUAD;
            view.setSurfaceShape(shape);
            view.removeEdgeFeathering();
            view.getResizableComponent().setResizable(resizable, /* maintainAspectRatio= */ true);
            view.getMovableComponent().setMovable(movable, /* scaleInZ= */ false);

            XrPixelDensity pixelDensity =
                    model.get(ImmersiveVideoPlayerProperties.DEFAULT_PIXEL_DENSITY);
            if (pixelDensity != null) {
                if (shape == XrSurfaceEntityShape.QUAD) {
                    Float aspectRatio =
                            model.get(ImmersiveVideoPlayerProperties.DEFAULT_ASPECT_RATIO);
                    int widthDp = model.get(ImmersiveVideoPlayerProperties.DEFAULT_WIDTH_DP);
                    if (widthDp > 0 && aspectRatio != null && aspectRatio > 0) {
                        float widthMeters = pixelDensity.convertDpToMeters(widthDp);
                        float heightMeters = widthMeters / aspectRatio;
                        view.setEntitySize(widthMeters, heightMeters);
                    }
                } else if (shape == XrSurfaceEntityShape.HEMISPHERE) {
                    int featherRadiusDp =
                            model.get(ImmersiveVideoPlayerProperties.DEFAULT_FEATHER_RADIUS_DP);
                    float featherMeters =
                            featherRadiusDp > 0
                                    ? pixelDensity.convertDpToMeters(featherRadiusDp)
                                    : 0f;
                    view.setRectangleEdgeFeathering(featherMeters, featherMeters);
                }

                if (view instanceof XrCurvedSurfaceEntityHolder) {
                    int curveRadiusDp =
                            model.get(ImmersiveVideoPlayerProperties.DEFAULT_CURVE_RADIUS_DP);
                    if (curveRadiusDp > 0) {
                        ((XrCurvedSurfaceEntityHolder) view)
                                .setEntityRadius(pixelDensity.convertDpToMeters(curveRadiusDp));
                    }
                }
            }
        } else if (propertyKey == ImmersiveVideoPlayerProperties.POSE) {
            XrPose pose = model.get(ImmersiveVideoPlayerProperties.POSE);
            if (pose != null) {
                view.setEntityPose(pose, XrSpace.ACTIVITY);
            }
        } else if (propertyKey == ImmersiveVideoPlayerProperties.PIXEL_WIDTH
                || propertyKey == ImmersiveVideoPlayerProperties.PIXEL_HEIGHT) {
            Integer pixelWidth = model.get(ImmersiveVideoPlayerProperties.PIXEL_WIDTH);
            Integer pixelHeight = model.get(ImmersiveVideoPlayerProperties.PIXEL_HEIGHT);
            if (pixelWidth != null && pixelHeight != null && pixelWidth > 0 && pixelHeight > 0) {
                view.setSurfacePixelDimensions(pixelWidth, pixelHeight);

                // Update the spatial panel dimensions to match the aspect ratio of the video.
                float aspectRatio = (float) pixelWidth / pixelHeight;
                if (view.getSurfaceShape() == XrSurfaceEntityShape.QUAD) {
                    int widthDp = model.get(ImmersiveVideoPlayerProperties.DEFAULT_WIDTH_DP);
                    XrPixelDensity pixelDensity =
                            model.get(ImmersiveVideoPlayerProperties.DEFAULT_PIXEL_DENSITY);
                    if (widthDp > 0 && pixelDensity != null) {
                        float widthMeters = pixelDensity.convertDpToMeters(widthDp);
                        float heightMeters = widthMeters / aspectRatio;
                        view.setEntitySize(widthMeters, heightMeters);
                    }
                }
            }
        } else if (propertyKey == ImmersiveVideoPlayerProperties.DEFAULT_MIN_WIDTH_DP) {
            int minWidthDp = model.get(ImmersiveVideoPlayerProperties.DEFAULT_MIN_WIDTH_DP);
            Float aspectRatio = model.get(ImmersiveVideoPlayerProperties.DEFAULT_ASPECT_RATIO);
            XrPixelDensity pixelDensity =
                    model.get(ImmersiveVideoPlayerProperties.DEFAULT_PIXEL_DENSITY);
            if (minWidthDp > 0 && aspectRatio != null && aspectRatio > 0 && pixelDensity != null) {
                float minWidthMeters = pixelDensity.convertDpToMeters(minWidthDp);
                float minHeightMeters = minWidthMeters / aspectRatio;
                view.getResizableComponent()
                        .setMinSize(XrFloatSize3d.create(minWidthMeters, minHeightMeters, 0f));
            }
        } else if (propertyKey == ImmersiveVideoPlayerProperties.DEFAULT_MAX_WIDTH_DP) {
            int maxWidthDp = model.get(ImmersiveVideoPlayerProperties.DEFAULT_MAX_WIDTH_DP);
            Float aspectRatio = model.get(ImmersiveVideoPlayerProperties.DEFAULT_ASPECT_RATIO);
            XrPixelDensity pixelDensity =
                    model.get(ImmersiveVideoPlayerProperties.DEFAULT_PIXEL_DENSITY);
            if (maxWidthDp > 0 && aspectRatio != null && aspectRatio > 0 && pixelDensity != null) {
                float maxWidthMeters = pixelDensity.convertDpToMeters(maxWidthDp);
                float maxHeightMeters = maxWidthMeters / aspectRatio;
                view.getResizableComponent()
                        .setMaxSize(XrFloatSize3d.create(maxWidthMeters, maxHeightMeters, 0f));
            }
        }
    }
}
