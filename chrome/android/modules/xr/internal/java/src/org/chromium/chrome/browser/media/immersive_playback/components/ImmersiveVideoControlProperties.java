// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.immersive_playback.components;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Properties for the immersive video control panel. */
@NullMarked
public class ImmersiveVideoControlProperties {
    public static final ReadableObjectPropertyKey<Float> DEFAULT_SPATIAL_HEIGHT =
            new ReadableObjectPropertyKey<>();
    public static final ReadableObjectPropertyKey<Float> DEFAULT_SPATIAL_WIDTH =
            new ReadableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<Long> DURATION_MS =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<String> DURATION_TEXT =
            new WritableObjectPropertyKey<>();
    public static final WritableBooleanPropertyKey FORMAT_BUTTON_SELECTED =
            new WritableBooleanPropertyKey();
    public static final WritableBooleanPropertyKey IS_MOVABLE = new WritableBooleanPropertyKey();
    public static final WritableBooleanPropertyKey IS_PLAYING = new WritableBooleanPropertyKey();
    public static final WritableObjectPropertyKey<Integer> MAX_PROGRESS =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<Double> PLAYBACK_RATE =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<float[]> POSE_ROTATION =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<float[]> POSE_TRANSLATION =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<Long> POSITION_MS =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<String> POSITION_TEXT =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<Integer> PROGRESS =
            new WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                DEFAULT_SPATIAL_HEIGHT,
                DEFAULT_SPATIAL_WIDTH,
                DURATION_MS,
                DURATION_TEXT,
                FORMAT_BUTTON_SELECTED,
                IS_MOVABLE,
                IS_PLAYING,
                MAX_PROGRESS,
                PLAYBACK_RATE,
                POSE_ROTATION,
                POSE_TRANSLATION,
                POSITION_MS,
                POSITION_TEXT,
                PROGRESS,
            };
}
