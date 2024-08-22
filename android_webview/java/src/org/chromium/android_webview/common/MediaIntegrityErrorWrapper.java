// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.common;

/**
 * Type safe wrapper to capture error codes returned by the media integrity API.
 *
 * <p>This class allows us to pass {@code int} values annotated with {@link MediaIntegrityErrorCode}
 * as the error value of an {@link ValueOrErrorCallback} without losing the enum annotation
 * information.
 */
public class MediaIntegrityErrorWrapper {

    public final @MediaIntegrityErrorCode int value;

    public MediaIntegrityErrorWrapper(@MediaIntegrityErrorCode int value) {
        this.value = value;
    }
}
