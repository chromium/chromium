// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

/**
 * Controller for Remote Web Debugging (Developer Tools).
 */
@JNINamespace("android_webview")
public class AwDevToolsServer {

    private long mNativeDevToolsServer;

    public AwDevToolsServer() {
        mNativeDevToolsServer =
                AwDevToolsServerJni.get().initRemoteDebugging(AwDevToolsServer.this);
    }

    public void destroy() {
        AwDevToolsServerJni.get().destroyRemoteDebugging(
                AwDevToolsServer.this, mNativeDevToolsServer);
        mNativeDevToolsServer = 0;
    }

    public void setRemoteDebuggingEnabled(boolean enabled) {
        AwDevToolsServerJni.get().setRemoteDebuggingEnabled(
                AwDevToolsServer.this, mNativeDevToolsServer, enabled);
    }

    @NativeMethods
    interface Natives {
        long initRemoteDebugging(AwDevToolsServer caller);
        void destroyRemoteDebugging(AwDevToolsServer caller, long devToolsServer);
        void setRemoteDebuggingEnabled(
                AwDevToolsServer caller, long devToolsServer, boolean enabled);
    }
}
