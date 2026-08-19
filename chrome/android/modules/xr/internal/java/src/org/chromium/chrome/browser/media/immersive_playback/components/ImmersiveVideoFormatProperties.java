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

/** Properties for the immersive playback format selection panel. */
@NullMarked
public class ImmersiveVideoFormatProperties {
    public static final ReadableObjectPropertyKey<XrPixelDensity> DEFAULT_PIXEL_DENSITY =
            new ReadableObjectPropertyKey<>();
    public static final ReadableIntPropertyKey DEFAULT_CORNER_RADIUS_DP =
            new ReadableIntPropertyKey();
    public static final ReadableIntPropertyKey DEFAULT_WIDTH_DP = new ReadableIntPropertyKey();
    public static final WritableIntPropertyKey HEIGHT_DP = new WritableIntPropertyKey();
    public static final WritableObjectPropertyKey<Float> PARENT_HEIGHT =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<Float> PARENT_WIDTH =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<Integer> RECOMMENDED_PROJECTION_TYPE =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<Integer> RECOMMENDED_STEREO_MODE =
            new WritableObjectPropertyKey<>();
    public static final WritableIntPropertyKey SELECTED_PROJECTION_TYPE =
            new WritableIntPropertyKey();
    public static final WritableIntPropertyKey SELECTED_STEREO_MODE = new WritableIntPropertyKey();

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                DEFAULT_PIXEL_DENSITY,
                DEFAULT_CORNER_RADIUS_DP,
                DEFAULT_WIDTH_DP,
                HEIGHT_DP,
                PARENT_HEIGHT,
                PARENT_WIDTH,
                RECOMMENDED_PROJECTION_TYPE,
                RECOMMENDED_STEREO_MODE,
                SELECTED_PROJECTION_TYPE,
                SELECTED_STEREO_MODE,
            };
}
