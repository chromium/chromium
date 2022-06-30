// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.js_sandbox.client;

import androidx.annotation.NonNull;

/** Super class for all exceptions thrown during evaluation. */
public class JsException extends Exception {
    public JsException(@NonNull String error) {
        super(error);
    }

    public JsException() {
        super();
    }
}
