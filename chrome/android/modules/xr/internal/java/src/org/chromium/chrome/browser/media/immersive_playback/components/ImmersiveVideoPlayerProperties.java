// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.immersive_playback.components;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Properties for the immersive video player panel. */
@NullMarked
public class ImmersiveVideoPlayerProperties {
    public static final ReadableObjectPropertyKey<Float> DEFAULT_ASPECT_RATIO =
            new ReadableObjectPropertyKey<>();
    public static final ReadableObjectPropertyKey<Float> DEFAULT_CURVE_RADIUS =
            new ReadableObjectPropertyKey<>();
    public static final ReadableObjectPropertyKey<Float> DEFAULT_MAX_WIDTH =
            new ReadableObjectPropertyKey<>();
    public static final ReadableObjectPropertyKey<Float> DEFAULT_MIN_WIDTH =
            new ReadableObjectPropertyKey<>();
    public static final ReadableObjectPropertyKey<Float> DEFAULT_SPATIAL_WIDTH =
            new ReadableObjectPropertyKey<>();
    public static final WritableIntPropertyKey PIXEL_HEIGHT = new WritableIntPropertyKey();
    public static final WritableIntPropertyKey PIXEL_WIDTH = new WritableIntPropertyKey();
    public static final WritableObjectPropertyKey<float[]> POSE_ROTATION =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<float[]> POSE_TRANSLATION =
            new WritableObjectPropertyKey<>();
    public static final WritableIntPropertyKey SHAPE = new WritableIntPropertyKey();
    public static final WritableIntPropertyKey STEREO_MODE = new WritableIntPropertyKey();

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                DEFAULT_ASPECT_RATIO,
                DEFAULT_CURVE_RADIUS,
                DEFAULT_MAX_WIDTH,
                DEFAULT_MIN_WIDTH,
                DEFAULT_SPATIAL_WIDTH,
                PIXEL_HEIGHT,
                PIXEL_WIDTH,
                POSE_ROTATION,
                POSE_TRANSLATION,
                SHAPE,
                STEREO_MODE,
            };
}
