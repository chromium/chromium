// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.immersive_playback.components;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Properties for the immersive playback format selection panel. */
@NullMarked
public class ImmersiveVideoFormatProperties {
    public static final ReadableObjectPropertyKey<Float> DEFAULT_CORNER_RADIUS =
            new ReadableObjectPropertyKey<>();
    public static final ReadableObjectPropertyKey<Float> DEFAULT_SPATIAL_HEIGHT =
            new ReadableObjectPropertyKey<>();
    public static final ReadableObjectPropertyKey<Float> DEFAULT_SPATIAL_WIDTH =
            new ReadableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<Float> PARENT_HEIGHT =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<Float> PARENT_WIDTH =
            new WritableObjectPropertyKey<>();
    public static final WritableIntPropertyKey SELECTED_PROJECTION_TYPE =
            new WritableIntPropertyKey();
    public static final WritableIntPropertyKey SELECTED_STEREO_MODE = new WritableIntPropertyKey();

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                DEFAULT_CORNER_RADIUS,
                DEFAULT_SPATIAL_HEIGHT,
                DEFAULT_SPATIAL_WIDTH,
                PARENT_HEIGHT,
                PARENT_WIDTH,
                SELECTED_PROJECTION_TYPE,
                SELECTED_STEREO_MODE,
            };
}
