// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.common;

import androidx.annotation.IntDef;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/**
 * Defines permission levels for the Media Integrity API.
 *
 * This enum is logged in Android.WebView.MediaIntegrity.ApiStatus
 * histogram and its values correspond to
 * AndroidWebViewMediaIntegrityApiStatus in enums.xml
 *
 * Please do not delete, reorder or reuse these values.
 */
@Target(ElementType.TYPE_USE)
@Retention(RetentionPolicy.SOURCE)
@IntDef({
    MediaIntegrityApiStatus.DISABLED,
    MediaIntegrityApiStatus.ENABLED_WITHOUT_APP_IDENTITY,
    MediaIntegrityApiStatus.ENABLED,
    MediaIntegrityApiStatus.COUNT
})
public @interface MediaIntegrityApiStatus {
    int DISABLED = 0;
    int ENABLED_WITHOUT_APP_IDENTITY = 1;
    int ENABLED = 2;
    int COUNT = 3;
}
