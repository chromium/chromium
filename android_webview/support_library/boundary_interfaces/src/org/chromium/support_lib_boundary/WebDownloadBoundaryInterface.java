// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_boundary;

import android.net.Uri;

import androidx.annotation.IntDef;

import org.jspecify.annotations.NullMarked;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;
import java.util.concurrent.Executor;
import java.util.function.Consumer;

/** Boundary interface for WebDownload. */
@NullMarked
public interface WebDownloadBoundaryInterface {
    /** Errors reported by {@link #saveTo}, matching androidx.webkit.DownloadException. */
    @Target(ElementType.TYPE_USE)
    @IntDef({
        ErrorCode.UNKNOWN,
        ErrorCode.CANCELLED,
        ErrorCode.NETWORK,
        ErrorCode.FILE,
        ErrorCode.INSUFFICIENT_SPACE,
        ErrorCode.SERVER,
        ErrorCode.SERVER_ACCESS_DENIED,
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface ErrorCode {
        int UNKNOWN = 0;
        int CANCELLED = 1;
        int NETWORK = 2;
        int FILE = 3;
        int INSUFFICIENT_SPACE = 4;
        int SERVER = 5;
        int SERVER_ACCESS_DENIED = 6;
    }

    String getUrl();

    String getMimeType();

    long getContentLength();

    String getSuggestedFilename();

    /** Calls exactly one of the callbacks, on {@code executor}. */
    void saveTo(
            Uri destination,
            Executor executor,
            Consumer<Long> resultCallback,
            Consumer<@ErrorCode Integer> errorCallback);

    void cancel();
}
