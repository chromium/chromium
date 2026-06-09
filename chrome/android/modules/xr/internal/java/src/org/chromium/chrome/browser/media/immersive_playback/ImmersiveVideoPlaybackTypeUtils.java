// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.immersive_playback;

import org.chromium.build.annotations.NullMarked;
import org.chromium.content_public.browser.ImmersiveProjectionType;
import org.chromium.content_public.browser.ImmersiveStereoMode;
import org.chromium.ui.xr.scenecore.XrSurfaceEntityShape;
import org.chromium.ui.xr.scenecore.XrSurfaceEntityStereoMode;

/** Utility methods for mapping projection types and stereo modes in immersive playback. */
@NullMarked
public class ImmersiveVideoPlaybackTypeUtils {
    private ImmersiveVideoPlaybackTypeUtils() {}

    /** Maps an {@link ImmersiveProjectionType} to an {@link XrSurfaceEntityShape}. */
    public static @XrSurfaceEntityShape int mapProjectionType(
            @ImmersiveProjectionType int projectionType) {
        switch (projectionType) {
            case ImmersiveProjectionType.QUAD:
                return XrSurfaceEntityShape.QUAD;
            case ImmersiveProjectionType.SPHERE:
                return XrSurfaceEntityShape.SPHERE;
            case ImmersiveProjectionType.HEMISPHERE:
                return XrSurfaceEntityShape.HEMISPHERE;
            case ImmersiveProjectionType.CUSTOM:
            default:
                return XrSurfaceEntityShape.QUAD;
        }
    }

    /** Maps an {@link ImmersiveStereoMode} to an {@link XrSurfaceEntityStereoMode}. */
    public static @XrSurfaceEntityStereoMode int mapStereoMode(
            @ImmersiveStereoMode int stereoMode) {
        switch (stereoMode) {
            case ImmersiveStereoMode.MONO:
                return XrSurfaceEntityStereoMode.MONO;
            case ImmersiveStereoMode.MULTIVIEW_LEFT_PRIMARY:
                return XrSurfaceEntityStereoMode.MULTIVIEW_LEFT_PRIMARY;
            case ImmersiveStereoMode.MULTIVIEW_RIGHT_PRIMARY:
                return XrSurfaceEntityStereoMode.MULTIVIEW_RIGHT_PRIMARY;
            case ImmersiveStereoMode.SIDE_BY_SIDE:
                return XrSurfaceEntityStereoMode.SIDE_BY_SIDE;
            case ImmersiveStereoMode.TOP_BOTTOM:
                return XrSurfaceEntityStereoMode.TOP_BOTTOM;
            default:
                return XrSurfaceEntityStereoMode.MONO;
        }
    }
}
