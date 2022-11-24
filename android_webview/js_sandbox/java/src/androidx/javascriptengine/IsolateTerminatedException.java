// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package androidx.javascriptengine;

/**
 * Exception thrown when evaluation is terminated due to the {@link JavaScriptIsolate} being closed
 * or crashing.
 *
 * Calling {@link JavaScriptIsolate#close()} will cause this exception to be thrown for all
 * previously requested but pending evaluations.
 * <p>
 * If the individual isolate has crashed, for example, due to exceeding a memory limit, this
 * exception will also be thrown for all pending and future evaluations (until
 * {@link JavaScriptIsolate#close()} is called).
 * <p>
 * Note that if the sandbox as a whole has crashed or been closed, {@link SandboxDeadException} will
 * be thrown instead.
 * <p>
 * Note that this exception will not be thrown if the isolate has been explicitly closed before a
 * call to {@link JavaScriptIsolate#evaluateJavaScriptAsync(String)}, which will instead immediately
 * throw an IllegalStateException (and not asynchronously via a future). This applies even if the
 * isolate was closed following a crash.
 */
public final class IsolateTerminatedException extends JavaScriptException {
    public IsolateTerminatedException() {
        super();
    }
}
