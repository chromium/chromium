// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.overlay;

import android.graphics.Color;
import android.graphics.RectF;
import android.support.annotation.Nullable;

import androidx.annotation.ColorInt;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.browser.autofill_assistant.AssistantDimension;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;

/**
 * State for the header of the Autofill Assistant.
 */
@JNINamespace("autofill_assistant")
public class AssistantOverlayModel extends PropertyModel {
    public static final WritableIntPropertyKey STATE = new WritableIntPropertyKey();

    public static final WritableObjectPropertyKey<List<RectF>> TOUCHABLE_AREA =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<List<RectF>> RESTRICTED_AREA =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<RectF> VISUAL_VIEWPORT =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<AssistantOverlayDelegate> DELEGATE =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<Integer> BACKGROUND_COLOR =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<Integer> HIGHLIGHT_BORDER_COLOR =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<Integer> TAP_TRACKING_COUNT =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<Long> TAP_TRACKING_DURATION_MS =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<AssistantOverlayImage> OVERLAY_IMAGE =
            new WritableObjectPropertyKey<>();

    public AssistantOverlayModel() {
        super(STATE, TOUCHABLE_AREA, RESTRICTED_AREA, VISUAL_VIEWPORT, DELEGATE, BACKGROUND_COLOR,
                HIGHLIGHT_BORDER_COLOR, TAP_TRACKING_COUNT, TAP_TRACKING_DURATION_MS,
                OVERLAY_IMAGE);
    }

    @CalledByNative
    private void setState(@AssistantOverlayState int state) {
        set(STATE, state);
    }

    @CalledByNative
    private void setVisualViewport(float left, float top, float right, float bottom) {
        set(VISUAL_VIEWPORT, new RectF(left, top, right, bottom));
    }

    @CalledByNative
    private void setTouchableArea(float[] coords) {
        set(TOUCHABLE_AREA, toRectangles(coords));
    }

    private static List<RectF> toRectangles(float[] coords) {
        List<RectF> boxes = new ArrayList<>();
        for (int i = 0; i < coords.length; i += 4) {
            boxes.add(new RectF(/* left= */ coords[i], /* top= */ coords[i + 1],
                    /* right= */ coords[i + 2], /* bottom= */ coords[i + 3]));
        }
        return boxes;
    }

    @CalledByNative
    private void setRestrictedArea(float[] coords) {
        set(RESTRICTED_AREA, toRectangles(coords));
    }

    @CalledByNative
    private void setDelegate(AssistantOverlayDelegate delegate) {
        set(DELEGATE, delegate);
    }

    @CalledByNative
    private void setBackgroundColor(@ColorInt int color) {
        set(BACKGROUND_COLOR, color);
    }

    @CalledByNative
    private void clearBackgroundColor() {
        set(BACKGROUND_COLOR, null);
    }

    @CalledByNative
    private void setHighlightBorderColor(@ColorInt int color) {
        set(HIGHLIGHT_BORDER_COLOR, color);
    }

    @CalledByNative
    private void clearHighlightBorderColor() {
        set(HIGHLIGHT_BORDER_COLOR, null);
    }

    @CalledByNative
    private void setOverlayImage(String imageUrl, @Nullable AssistantDimension imageSize,
            @Nullable AssistantDimension imageTopMargin,
            @Nullable AssistantDimension imageBottomMargin, String text, @ColorInt int textColor,
            @Nullable AssistantDimension textSize) {
        set(OVERLAY_IMAGE,
                new AssistantOverlayImage(imageUrl, imageSize, imageTopMargin, imageBottomMargin,
                        text, textColor, textSize));
    }

    @CalledByNative
    private void clearOverlayImage() {
        set(OVERLAY_IMAGE, null);
    }

    /**
     * Parses {@code colorString} and returns the corresponding color integer. This is only safe to
     * call for valid strings, which should be checked with {@code isValidColorString} before
     * calling this method!
     * @return the 32-bit integer representation of {@code colorString} or an unspecified fallback
     * value if {@code colorString} is not a valid color string.
     */
    @CalledByNative
    private static @ColorInt int parseColorString(String colorString) {
        if (!isValidColorString(colorString)) {
            return Color.BLACK;
        }
        return Color.parseColor(colorString);
    }

    /**
     * Returns whether {@code colorString} is a valid string representation of a color. Supported
     * color formats are #RRGGBB and #AARRGGBB.
     */
    @CalledByNative
    private static boolean isValidColorString(String colorString) {
        if (colorString.isEmpty()) {
            return false;
        }
        try {
            Color.parseColor(colorString);
            return true;
        } catch (IllegalArgumentException e) {
            return false;
        }
    }

    @CalledByNative
    private void setTapTracking(int count, long durationMs) {
        set(TAP_TRACKING_COUNT, count);
        set(TAP_TRACKING_DURATION_MS, durationMs);
    }
}
