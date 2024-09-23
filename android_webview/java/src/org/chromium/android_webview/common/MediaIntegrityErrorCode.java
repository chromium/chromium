// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.common;

import androidx.annotation.IntDef;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/** Error codes for Media Integrity Blink Renderer Extension. */
@Target(ElementType.TYPE_USE)
@Retention(RetentionPolicy.SOURCE)
@IntDef({
    MediaIntegrityErrorCode.INTERNAL_ERROR,
    MediaIntegrityErrorCode.NON_RECOVERABLE_ERROR,
    MediaIntegrityErrorCode.API_DISABLED_BY_APPLICATION,
    MediaIntegrityErrorCode.INVALID_ARGUMENT,
    MediaIntegrityErrorCode.TOKEN_PROVIDER_INVALID,
})
public @interface MediaIntegrityErrorCode {
    int INTERNAL_ERROR = 1;
    int NON_RECOVERABLE_ERROR = 2;
    int API_DISABLED_BY_APPLICATION = 3;
    int INVALID_ARGUMENT = 4;
    int TOKEN_PROVIDER_INVALID = 5;
}
