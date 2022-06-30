// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.js_sandbox.client;

import androidx.annotation.NonNull;

/** Wrapper for the exception thrown by the JS evaluation engine. */
public class EvaluationFailedException extends JsException {
    public EvaluationFailedException(@NonNull String error) {
        super(error);
    }
}
