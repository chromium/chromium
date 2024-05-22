// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.android_webview.common.AwSwitches;
import org.chromium.android_webview.common.Lifetime;
import org.chromium.base.CommandLine;

/** Controller for Remote Web Debugging (Developer Tools). */
@Lifetime.Singleton
@JNINamespace("android_webview")
public class AwDevToolsServer {

    public void setRemoteDebuggingEnabled(boolean enabled) {
        if (CommandLine.getInstance().hasSwitch(AwSwitches.NET_LOG)) {
            if (enabled) {
                AwNetLogsConnection.startConnectNetLogService();
            } else {
                AwNetLogsConnection.stopNetLogService();
            }
        }
        AwDevToolsServerJni.get().setRemoteDebuggingEnabled(AwDevToolsServer.this, enabled);
    }

    @NativeMethods
    interface Natives {
        void setRemoteDebuggingEnabled(AwDevToolsServer caller, boolean enabled);
    }
}
