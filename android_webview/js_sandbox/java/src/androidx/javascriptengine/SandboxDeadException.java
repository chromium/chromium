// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package androidx.javascriptengine;

/**
 * Exception thrown when evaluation is terminated due the {@link JavaScriptSandbox} being dead.
 * This can happen when {@link JavaScriptSandbox#close()} is called or when the sandbox process
 * is killed by the framework.
 * <p>
 * This is different from {@link IsolateTerminatedException} which occurs when the
 * {@link JavaScriptIsolate} within a {@link JavaScriptSandbox} is terminated.
 * <p>
 * This exception will continue to be thrown for all future evaluation requests on unclosed
 * isolates.
 */
public final class SandboxDeadException extends JavaScriptException {
    public SandboxDeadException() {
        super();
    }
}
