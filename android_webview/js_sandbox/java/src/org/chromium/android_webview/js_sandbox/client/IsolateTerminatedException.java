// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.js_sandbox.client;

/** Exception thrown when evaluation is terminated. */
public class IsolateTerminatedException extends JsException {
    public IsolateTerminatedException() {
        super();
    }
}
