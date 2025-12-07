// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.common;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/**
 * Utility to log locations where NonRecoverableError is returned to investigate why so many errors
 * are recorded.
 */
@NullMarked
public final class MediaIntegrityNonRecoverableErrorLogger {

    @IntDef({
        MISMATCHED_ORIGINS,
        GET_PROVIDER_SECURITY_EXCEPTION,
        AOSP_BUILD,
        MAPPED_SERVICE_RESPONSE,
        VALUE_COUNT
    })
    @Target(ElementType.TYPE_USE)
    @Retention(RetentionPolicy.SOURCE)
    @interface Reason {}

    public static final int MISMATCHED_ORIGINS = 0;
    public static final int GET_PROVIDER_SECURITY_EXCEPTION = 1;
    public static final int AOSP_BUILD = 2;
    public static final int MAPPED_SERVICE_RESPONSE = 3;

    private static final int VALUE_COUNT = 4;

    public static void log(@Reason int location) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.WebView.MediaIntegrity.NonRecoverableErrorReason", location, VALUE_COUNT);
    }
}
