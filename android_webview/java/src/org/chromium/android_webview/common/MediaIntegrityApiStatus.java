// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.common;

import androidx.annotation.IntDef;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

@Target(ElementType.TYPE_USE)
@Retention(RetentionPolicy.SOURCE)
@IntDef({
    MediaIntegrityApiStatus.DISABLED,
    MediaIntegrityApiStatus.ENABLED_WITHOUT_APP_IDENTITY,
    MediaIntegrityApiStatus.ENABLED
})
public @interface MediaIntegrityApiStatus {
    int DISABLED = 0;
    int ENABLED_WITHOUT_APP_IDENTITY = 1;
    int ENABLED = 2;
}
