// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.chromium.build.annotations.NullMarked;

/** Controller responsible for managing WebView startup lifecycle and tasks. */
@NullMarked
public class StartupController {
    /** Delegate interface for callbacks needed during WebView global startup. */
    public interface Delegate {
        /** Wait until it's possible to access Android resources defined in the Chromium APK. */
        void waitForJavaResourcesSetup();

        /** Returns whether to use native sandboxed services. */
        boolean shouldForceNativeSandboxedServices();

        // TODO(abhijithnair): Rethink whether `getDrawFnFunctionTable` and `getDrawSWFunctionTable`
        // are the right interface. See
        // https://chromium-review.git.corp.google.com/c/chromium/src/+/8257352/comment/d9c4282e_3fa74a88/
        /** Returns the function table pointer for hardware-accelerated drawing. */
        long getDrawFnFunctionTable();

        /** Returns the function table pointer for software drawing. */
        long getDrawSWFunctionTable();
    }

    @SuppressWarnings("UnusedVariable")
    private final Delegate mDelegate;

    public StartupController(Delegate delegate) {
        mDelegate = delegate;
    }
}
