// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package androidx.javascriptengine;

import androidx.annotation.IntDef;
import androidx.annotation.RestrictTo;

import org.chromium.android_webview.js_sandbox.common.IJsSandboxIsolateCallback;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** @hide */
@IntDef(value = {ExecutionErrorTypes.JS_EVALUATION_ERROR})
@RestrictTo(RestrictTo.Scope.LIBRARY)
@Retention(RetentionPolicy.SOURCE)
public @interface ExecutionErrorTypes {
    int JS_EVALUATION_ERROR = IJsSandboxIsolateCallback.JS_EVALUATION_ERROR;
}
