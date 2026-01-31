// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.bar_component;

import android.content.res.Resources;
import android.graphics.Outline;
import android.graphics.Path;
import android.os.Build;
import android.view.View;
import android.view.ViewOutlineProvider;

import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.keyboard_accessory.R;
import org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryStyle.NotchPosition;

/**
 * A {@link ViewOutlineProvider} that creates a rounded outline with a triangular notch at the top
 * or bottom.
 */
@NullMarked
public class NotchedKeyboardAccessoryOutlineProvider extends ViewOutlineProvider {
    private final @NotchPosition int mNotchPosition;
    private int mNotchOffsetX;

    public NotchedKeyboardAccessoryOutlineProvider(@NotchPosition int notchPosition) {
        mNotchPosition = notchPosition;
    }

    public void setNotchOffsetX(int notchOffsetX) {
        mNotchOffsetX = notchOffsetX;
    }

    @Override
    public void getOutline(View view, Outline outline) {
        int width = view.getWidth();
        int height = view.getHeight();
        Resources res = view.getResources();

        // Fallback for the old devices that don't support complex paths.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
            float cornerRadius =
                    res.getDimension(R.dimen.keyboard_accessory_corner_radius_redesign);
            outline.setRoundRect(0, 0, width, height, (int) cornerRadius);
            return;
        }

        // For shape with the notch, calculate the path.
        Path path = createNotchPath(res, mNotchPosition, width, height, mNotchOffsetX);
        outline.setPath(path);
    }

    /**
     * Shared logic to generate the clip path. Used by {@link #getOutline} for hardware clipping and
     * exposed for tests to simulate software clipping.
     *
     * @param res Resources to fetch dimensions.
     * @param notchPosition The position of the notch.
     * @param width The width of the view.
     * @param height The height of the view.
     * @param notchOffsetX The offset of the notch from its default position.
     * @return The calculated Path object.
     */
    @VisibleForTesting
    public static Path createNotchPath(
            Resources res,
            @NotchPosition int notchPosition,
            int width,
            int height,
            int notchOffsetX) {
        Path path = new Path();

        float notchHeight = res.getDimension(R.dimen.keyboard_accessory_notch_height);
        float cornerRadius = res.getDimension(R.dimen.keyboard_accessory_corner_radius_redesign);
        float notchX = res.getDimension(R.dimen.keyboard_accessory_notch_position) + notchOffsetX;
        float tipWidth = res.getDimension(R.dimen.keyboard_accessory_notch_tip_width);
        float baseWidth = res.getDimension(R.dimen.keyboard_accessory_notch_base_width);
        float notchRounding = res.getDimension(R.dimen.keyboard_accessory_notch_tip_rounding);

        switch (notchPosition) {
            case NotchPosition.HIDDEN:
                path.addRoundRect(
                        0, 0, width, height, cornerRadius, cornerRadius, Path.Direction.CW);
                break;
            case NotchPosition.TOP:
                path.moveTo(notchX - (baseWidth / 2f), notchHeight);
                path.lineTo(notchX - (tipWidth / 2f), 0);
                path.lineTo(notchX + (tipWidth / 2f), 0);
                path.lineTo(notchX + (baseWidth / 2f), notchHeight);
                path.addRoundRect(
                        0,
                        notchHeight,
                        (float) width,
                        (float) height,
                        cornerRadius,
                        cornerRadius,
                        Path.Direction.CW);
                break;

            case NotchPosition.BOTTOM:
                float rectBottom = height - notchHeight;
                path.addRoundRect(
                        0, 0, width, rectBottom, cornerRadius, cornerRadius, Path.Direction.CW);

                path.moveTo(notchX - (baseWidth / 2f), rectBottom);
                path.lineTo(notchX - (tipWidth / 2f) - notchRounding, height - notchRounding);
                path.quadTo(
                        notchX - (tipWidth / 2f),
                        height,
                        notchX + (tipWidth / 2f) - notchRounding,
                        height);
                path.lineTo(notchX + (tipWidth / 2f) - notchRounding, height);
                path.quadTo(
                        notchX + (tipWidth / 2f),
                        height,
                        notchX + (tipWidth / 2f) + notchRounding,
                        height - notchRounding);
                path.lineTo(notchX + (baseWidth / 2f), rectBottom);
                break;
            default:
                assert false : "Every position should be handled";
                break;
        }
        return path;
    }
}
