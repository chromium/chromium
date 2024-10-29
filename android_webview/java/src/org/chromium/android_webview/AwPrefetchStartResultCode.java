// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

@IntDef({AwPrefetchStartResultCode.SUCCESS, AwPrefetchStartResultCode.FAILURE})
@Retention(RetentionPolicy.SOURCE)
public @interface AwPrefetchStartResultCode {
    int SUCCESS = 0;
    int FAILURE = 1;
}
