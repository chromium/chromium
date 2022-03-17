// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.js.common;
import org.chromium.android_webview.js.common.IJsSandboxContext;

/**
 * Used by the embedding app to execute JavaScript in a sandboxed environment.
 */
interface IJsSandboxService {
    IJsSandboxContext createContext() = 0;
}
