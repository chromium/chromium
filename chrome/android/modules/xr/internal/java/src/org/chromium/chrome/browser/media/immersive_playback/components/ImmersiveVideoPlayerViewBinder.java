// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.immersive_playback.components;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.xr.scenecore.XrCurvedSurfaceEntityHolder;
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
            boolean isQuad = shape == XrSurfaceEntityShape.QUAD;
            view.setSurfaceShape(shape);

            view.getResizableComponent()
                    .setResizable(/* resizable= */ isQuad, /* maintainAspectRatio= */ true);
            view.getMovableComponent().setMovable(/* movable= */ isQuad, /* scaleInZ= */ false);

            if (isQuad && view.getSurfaceShape() == XrSurfaceEntityShape.QUAD) {
                Float aspectRatio = model.get(ImmersiveVideoPlayerProperties.DEFAULT_ASPECT_RATIO);
                Float width = model.get(ImmersiveVideoPlayerProperties.DEFAULT_SPATIAL_WIDTH);
                if (width != null && aspectRatio != null && width > 0 && aspectRatio > 0) {
                    float height = width / aspectRatio;
                    view.setEntitySize(width, height);
                }
            } else if (view instanceof XrCurvedSurfaceEntityHolder) {
                Float radius = model.get(ImmersiveVideoPlayerProperties.DEFAULT_CURVE_RADIUS);
                if (radius != null && radius > 0) {
                    ((XrCurvedSurfaceEntityHolder) view).setEntityRadius(radius);
                }
            }
        } else if (propertyKey == ImmersiveVideoPlayerProperties.POSE_TRANSLATION
                || propertyKey == ImmersiveVideoPlayerProperties.POSE_ROTATION) {
            float[] translation = model.get(ImmersiveVideoPlayerProperties.POSE_TRANSLATION);
            float[] rotation = model.get(ImmersiveVideoPlayerProperties.POSE_ROTATION);
            if (translation != null && rotation != null) {
                view.setEntityPose(translation, rotation, XrSpace.ACTIVITY);
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
                    Float width = model.get(ImmersiveVideoPlayerProperties.DEFAULT_SPATIAL_WIDTH);
                    if (width != null && width > 0) {
                        float height = width / aspectRatio;
                        view.setEntitySize(width, height);
                    }
                }
            }
        } else if (propertyKey == ImmersiveVideoPlayerProperties.DEFAULT_MIN_WIDTH) {
            Float minWidth = model.get(ImmersiveVideoPlayerProperties.DEFAULT_MIN_WIDTH);
            Float aspectRatio = model.get(ImmersiveVideoPlayerProperties.DEFAULT_ASPECT_RATIO);
            if (minWidth != null && aspectRatio != null && minWidth > 0 && aspectRatio > 0) {
                float minHeight = minWidth / aspectRatio;
                view.getResizableComponent().setMinSize(minWidth, minHeight, 0f);
            }
        } else if (propertyKey == ImmersiveVideoPlayerProperties.DEFAULT_MAX_WIDTH) {
            Float maxWidth = model.get(ImmersiveVideoPlayerProperties.DEFAULT_MAX_WIDTH);
            Float aspectRatio = model.get(ImmersiveVideoPlayerProperties.DEFAULT_ASPECT_RATIO);
            if (maxWidth != null && aspectRatio != null && maxWidth > 0 && aspectRatio > 0) {
                float maxHeight = maxWidth / aspectRatio;
                view.getResizableComponent().setMaxSize(maxWidth, maxHeight, 0f);
            }
        }
    }
}
