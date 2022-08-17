// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package androidx.javascriptengine;

/**
 * Exception thrown when evaluation is terminated due to {@link JavaScriptIsolate} being
 * closed. This can occur when the {@link JavaScriptIsolate#close()} is called.
 */
public final class IsolateTerminatedException extends JavaScriptException {
    public IsolateTerminatedException() {
        super();
    }
}
