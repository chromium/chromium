// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.js_sandbox.common;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

@IntDef({IJsSandboxIsolateCallback.JS_EVALUATION_ERROR})
@Retention(RetentionPolicy.SOURCE)
public @interface ExecutionErrorTypes {}
