// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.js_sandbox.client;

/** Wrapper for the exception thrown by the JS evaluation engine. */
public class JsEvaluationException extends Exception {
    public JsEvaluationException(String error) {
        super(error);
    }
}
