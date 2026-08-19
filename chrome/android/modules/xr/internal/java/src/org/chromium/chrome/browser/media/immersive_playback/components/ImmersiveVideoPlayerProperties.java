// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.immersive_playback.components;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;
import org.chromium.ui.xr.scenecore.XrPixelDensity;
import org.chromium.ui.xr.scenecore.XrPose;

/** Properties for the immersive video player panel. */
@NullMarked
public class ImmersiveVideoPlayerProperties {
    public static final ReadableObjectPropertyKey<XrPixelDensity> DEFAULT_PIXEL_DENSITY =
            new ReadableObjectPropertyKey<>();
    public static final ReadableObjectPropertyKey<Float> DEFAULT_ASPECT_RATIO =
            new ReadableObjectPropertyKey<>();
    public static final ReadableIntPropertyKey DEFAULT_CURVE_RADIUS_DP =
            new ReadableIntPropertyKey();
    public static final ReadableIntPropertyKey DEFAULT_FEATHER_RADIUS_DP =
            new ReadableIntPropertyKey();
    public static final ReadableIntPropertyKey DEFAULT_MAX_WIDTH_DP = new ReadableIntPropertyKey();
    public static final ReadableIntPropertyKey DEFAULT_MIN_WIDTH_DP = new ReadableIntPropertyKey();
    public static final ReadableIntPropertyKey DEFAULT_WIDTH_DP = new ReadableIntPropertyKey();
    public static final WritableIntPropertyKey PIXEL_HEIGHT = new WritableIntPropertyKey();
    public static final WritableIntPropertyKey PIXEL_WIDTH = new WritableIntPropertyKey();
    public static final WritableObjectPropertyKey<XrPose> POSE = new WritableObjectPropertyKey<>();
    public static final WritableIntPropertyKey SHAPE = new WritableIntPropertyKey();
    public static final WritableIntPropertyKey STEREO_MODE = new WritableIntPropertyKey();

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                DEFAULT_PIXEL_DENSITY,
                DEFAULT_ASPECT_RATIO,
                DEFAULT_CURVE_RADIUS_DP,
                DEFAULT_FEATHER_RADIUS_DP,
                DEFAULT_MAX_WIDTH_DP,
                DEFAULT_MIN_WIDTH_DP,
                DEFAULT_WIDTH_DP,
                PIXEL_HEIGHT,
                PIXEL_WIDTH,
                POSE,
                SHAPE,
                STEREO_MODE,
            };
}
