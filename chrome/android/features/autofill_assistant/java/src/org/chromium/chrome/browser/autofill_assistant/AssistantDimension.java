// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.util.DisplayMetrics;
import android.util.TypedValue;

import org.chromium.base.annotations.CalledByNative;

/** Represents a size dimension, such as width or height. */
public abstract class AssistantDimension {
    /** Returns the size of this dimension in pixels. */
    public abstract int getSizeInPixels(DisplayMetrics displayMetrics);

    @CalledByNative
    public static AssistantDimension createFromDpi(int dpi) {
        return new AssistantDimensionDpi(dpi);
    }

    @CalledByNative
    public static AssistantDimension createFromWidthFactor(float factor) {
        return new AssistantDimensionWidthFactor(factor);
    }

    @CalledByNative
    public static AssistantDimension createFromHeightFactor(float factor) {
        return new AssistantDimensionHeightFactor(factor);
    }

    private static class AssistantDimensionDpi extends AssistantDimension {
        private final int mDpi;
        AssistantDimensionDpi(int dpi) {
            mDpi = dpi;
        }
        @Override
        public int getSizeInPixels(DisplayMetrics displayMetrics) {
            return Math.round(
                    TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, mDpi, displayMetrics));
        }
    }

    private static class AssistantDimensionWidthFactor extends AssistantDimension {
        private final double mFactor;
        AssistantDimensionWidthFactor(double factor) {
            mFactor = factor;
        }
        @Override
        public int getSizeInPixels(DisplayMetrics displayMetrics) {
            return (int) Math.round(displayMetrics.widthPixels * mFactor);
        }
    }

    private static class AssistantDimensionHeightFactor extends AssistantDimension {
        private final double mFactor;
        AssistantDimensionHeightFactor(double factor) {
            mFactor = factor;
        }
        @Override
        public int getSizeInPixels(DisplayMetrics displayMetrics) {
            return (int) Math.round(displayMetrics.heightPixels * mFactor);
        }
    }
}
