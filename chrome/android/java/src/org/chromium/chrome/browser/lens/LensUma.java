// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.lens;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Static utility methods to support UMA logging for Lens entry points.
 */
public class LensUma {
    // Note: these values must match the ContextMenuLensSupportStatus enum in enums.xml.
    // Only add new values at the end, right before NUM_ENTRIES.
    @IntDef({LensSupportStatus.LENS_SEARCH_SUPPORTED, LensSupportStatus.NON_GOOGLE_SEARCH_ENGINE,
            LensSupportStatus.ACTIVITY_NOT_ACCESSIBLE, LensSupportStatus.OUT_OF_DATE,
            LensSupportStatus.SEARCH_BY_IMAGE_UNAVAILABLE, LensSupportStatus.LEGACY_OS,
            LensSupportStatus.INVALID_PACKAGE, LensSupportStatus.LENS_SHOP_SUPPORTED,
            LensSupportStatus.LENS_SHOP_AND_SEARCH_SUPPORTED,
            LensSupportStatus.CAMERA_NOT_AVAILABLE, LensSupportStatus.LOW_END_DEVICE,
            LensSupportStatus.LENS_CAMERA_ASSISTED_SEARCH_SUPPORTED,
            LensSupportStatus.LENS_TRANSLATE_SUPPORTED})
    @Retention(RetentionPolicy.SOURCE)
    public static @interface LensSupportStatus {
        int LENS_SEARCH_SUPPORTED = 0;
        int NON_GOOGLE_SEARCH_ENGINE = 1;
        int ACTIVITY_NOT_ACCESSIBLE = 2;
        int OUT_OF_DATE = 3;
        int SEARCH_BY_IMAGE_UNAVAILABLE = 4;
        int LEGACY_OS = 5;
        int INVALID_PACKAGE = 6;
        int LENS_SHOP_SUPPORTED = 7;
        int LENS_SHOP_AND_SEARCH_SUPPORTED = 8;
        int CAMERA_NOT_AVAILABLE = 9;
        int LOW_END_DEVICE = 10;
        int LENS_CAMERA_ASSISTED_SEARCH_SUPPORTED = 11;
        int LENS_TRANSLATE_SUPPORTED = 12;
        int NUM_ENTRIES = 13;
    }

    public static void recordLensSupportStatus(
            String histogramName, @LensSupportStatus int reason) {
        RecordHistogram.recordEnumeratedHistogram(
                histogramName, reason, LensSupportStatus.NUM_ENTRIES);
    }
}
