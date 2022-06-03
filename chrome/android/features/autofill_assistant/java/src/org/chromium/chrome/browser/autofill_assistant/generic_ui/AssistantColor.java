// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.generic_ui;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Color;

import androidx.annotation.ColorInt;
import androidx.annotation.Nullable;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/** Contains methods to convert color identifiers to 32-bit Java color integers. */
@JNINamespace("autofill_assistant")
public abstract class AssistantColor {
    public abstract @Nullable @ColorInt Integer getColor(Context context);

    @CalledByNative
    public static boolean isValidColorString(String color) {
        if (color.isEmpty()) {
            return false;
        }
        try {
            Color.parseColor(color);
            return true;
        } catch (IllegalArgumentException e) {
            return false;
        }
    }

    @CalledByNative
    public static boolean isValidResourceIdentifier(Context context, String resourceIdentifier) {
        int colorId = context.getResources().getIdentifier(
                resourceIdentifier, "color", context.getPackageName());
        if (colorId == 0) {
            return false;
        }
        try {
            ApiCompatibilityUtils.getColor(context.getResources(), colorId);
            return true;
        } catch (Resources.NotFoundException e) {
            return false;
        }
    }

    @Nullable
    @ColorInt
    @CalledByNative
    public static Integer createFromString(String color) {
        if (color.isEmpty()) {
            return null;
        }
        try {
            return Color.parseColor(color);
        } catch (IllegalArgumentException e) {
            return null;
        }
    }

    @Nullable
    @ColorInt
    @CalledByNative
    public static Integer createFromResource(Context context, String resourceIdentifier) {
        int colorId = context.getResources().getIdentifier(
                resourceIdentifier, "color", context.getPackageName());
        if (colorId == 0) {
            return null;
        }
        try {
            return ApiCompatibilityUtils.getColor(context.getResources(), colorId);
        } catch (Resources.NotFoundException e) {
            return null;
        }
    }
}
