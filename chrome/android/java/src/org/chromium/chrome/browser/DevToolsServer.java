// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.pm.PackageManager;

import androidx.annotation.IntDef;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Controller for Remote Web Debugging (Developer Tools).
 */
public class DevToolsServer {
    private static final String DEBUG_PERMISSION_SIFFIX = ".permission.DEBUG";

    private long mNativeDevToolsServer;

    // Defines what processes may access to the socket.
    @IntDef({Security.DEFAULT, Security.ALLOW_DEBUG_PERMISSION})
    @Retention(RetentionPolicy.SOURCE)
    public @interface Security {
        // Use content::CanUserConnectToDevTools to authorize access to the socket.
        int DEFAULT = 0;
        // In addition to default authorization allows access to an app with android permission
        // named chromeAppPackageName + DEBUG_PERMISSION_SIFFIX.
        int ALLOW_DEBUG_PERMISSION = 1;
    }

    public DevToolsServer(String socketNamePrefix) {
        mNativeDevToolsServer =
                DevToolsServerJni.get().initRemoteDebugging(DevToolsServer.this, socketNamePrefix);
    }

    public void destroy() {
        DevToolsServerJni.get().destroyRemoteDebugging(DevToolsServer.this, mNativeDevToolsServer);
        mNativeDevToolsServer = 0;
    }

    public boolean isRemoteDebuggingEnabled() {
        return DevToolsServerJni.get().isRemoteDebuggingEnabled(
                DevToolsServer.this, mNativeDevToolsServer);
    }

    public void setRemoteDebuggingEnabled(boolean enabled, @Security int security) {
        boolean allowDebugPermission = security == Security.ALLOW_DEBUG_PERMISSION;
        DevToolsServerJni.get().setRemoteDebuggingEnabled(
                DevToolsServer.this, mNativeDevToolsServer, enabled, allowDebugPermission);
    }

    public void setRemoteDebuggingEnabled(boolean enabled) {
        setRemoteDebuggingEnabled(enabled, Security.DEFAULT);
    }

    @CalledByNative
    private static boolean checkDebugPermission(int pid, int uid) {
        String debugPermissionName =
                ContextUtils.getApplicationContext().getPackageName() + DEBUG_PERMISSION_SIFFIX;
        return ApiCompatibilityUtils.checkPermission(
                       ContextUtils.getApplicationContext(), debugPermissionName, pid, uid)
                == PackageManager.PERMISSION_GRANTED;
    }

    @NativeMethods
    interface Natives {
        long initRemoteDebugging(DevToolsServer caller, String socketNamePrefix);
        void destroyRemoteDebugging(DevToolsServer caller, long devToolsServer);
        boolean isRemoteDebuggingEnabled(DevToolsServer caller, long devToolsServer);
        void setRemoteDebuggingEnabled(DevToolsServer caller, long devToolsServer, boolean enabled,
                boolean allowDebugPermission);
    }
}
