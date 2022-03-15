// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.js.common;

/**
 * Used to communicate the result of the JavaScript evaluation from the
 * sandbox to the embedding app.
 */
oneway interface IJsSandboxContextCallback {
    void reportResult(in String result);
    void reportError(in String error);
}
