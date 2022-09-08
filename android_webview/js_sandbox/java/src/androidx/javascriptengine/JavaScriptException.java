// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package androidx.javascriptengine;

import androidx.annotation.NonNull;

/**
 * Super class for all exceptions resolved by
 * {@link JavaScriptIsolate#evaluateJavaScriptAsync(String)}.
 */
public class JavaScriptException extends Exception {
    public JavaScriptException(@NonNull String error) {
        super(error);
    }

    public JavaScriptException() {
        super();
    }
}
