// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.content.Context;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.net.test.EmbeddedTestServerImpl;

/**
 * Java bindings for running a net::test_server::EmbeddedTestServer.
 *
 * This should not be used directly. Use {@link EmbeddedTestServer} instead.
 */
@JNINamespace("android_webview::test")
public class AwEmbeddedTestServerImpl extends EmbeddedTestServerImpl {
    /** Create an uninitialized EmbeddedTestServer. */
    public AwEmbeddedTestServerImpl(Context context) {
        super(context);
    }

    /** Add the default handlers and serve files from the provided directory relative to the
     *  external storage directory.
     *
     *  @param directoryPath The path of the directory from which files should be served, relative
     *      to the external storage directory.
     */
    @Override
    public void addDefaultHandlers(final String directoryPath) {
        super.addDefaultHandlers(directoryPath);
        long[] handlers = AwEmbeddedTestServerImplJni.get().getHandlers();
        for (long handler : handlers) {
            super.registerRequestHandler(handler);
        }
    }

    @NativeMethods
    interface Natives {
        long[] getHandlers();
    }
}
