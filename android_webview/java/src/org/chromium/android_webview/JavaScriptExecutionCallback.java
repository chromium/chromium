// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.chromium.android_webview.common.Lifetime;
import org.chromium.build.annotations.NullMarked;
import org.chromium.js_injection.mojom.JavaScriptExecutionError;

@Lifetime.Temporary
@NullMarked
public interface JavaScriptExecutionCallback {
    default void onSuccess(String result) {}

    default void onError(@JavaScriptExecutionError.EnumType int errorCode) {}
}
