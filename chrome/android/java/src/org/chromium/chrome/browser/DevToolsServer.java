// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;


import android.content.pm.PackageManager;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ContextUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.components.prefs.PrefChangeRegistrar;
import org.chromium.components.prefs.PrefService;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Controller for Remote Web Debugging (Developer Tools). */
@NullMarked
public class DevToolsServer {
    private static final String DEBUG_PERMISSION_SUFFIX = ".permission.DEBUG";

    private final PrefService mPrefService;
    private final PrefChangeRegistrar mPrefChangeRegistrar;
    private long mNativeDevToolsServer;
    private boolean mRemoteDebuggingEnabled;
    private @Security int mSecurity = Security.DEFAULT;

    // Defines what processes may access to the socket.
    @IntDef({Security.DEFAULT, Security.ALLOW_DEBUG_PERMISSION})
    @Retention(RetentionPolicy.SOURCE)
    public @interface Security {
        // Use content::CanUserConnectToDevTools to authorize access to the socket.
        int DEFAULT = 0;
        // In addition to default authorization allows access to an app with android permission
        // named chromeAppPackageName + DEBUG_PERMISSION_SUFFIX.
        int ALLOW_DEBUG_PERMISSION = 1;
    }

    public DevToolsServer(String socketNamePrefix, PrefService prefService) {
        this(socketNamePrefix, prefService, new PrefChangeRegistrar(prefService));
    }

    @VisibleForTesting
    DevToolsServer(
            String socketNamePrefix,
            PrefService prefService,
            PrefChangeRegistrar prefChangeRegistrar) {
        mNativeDevToolsServer = DevToolsServerJni.get().initRemoteDebugging(socketNamePrefix);
        mPrefService = prefService;
        mPrefChangeRegistrar = prefChangeRegistrar;
        mPrefChangeRegistrar.addObserver(
                Pref.DEV_TOOLS_REMOTE_DEBUGGING_ALLOWED, this::updateRemoteDebugging);
    }

    public void destroy() {
        mPrefChangeRegistrar.destroy();
        if (mNativeDevToolsServer != 0) {
            DevToolsServerJni.get().destroyRemoteDebugging(mNativeDevToolsServer);
            mNativeDevToolsServer = 0;
        }
    }

    public void setRemoteDebuggingEnabled(boolean enabled, @Security int security) {
        mRemoteDebuggingEnabled = enabled;
        mSecurity = security;
        updateRemoteDebugging();
    }

    @VisibleForTesting
    void updateRemoteDebugging() {
        if (mNativeDevToolsServer == 0) return;
        boolean enabled =
                mRemoteDebuggingEnabled
                        && mPrefService.getBoolean(Pref.DEV_TOOLS_REMOTE_DEBUGGING_ALLOWED);
        boolean allowDebugPermission = mSecurity == Security.ALLOW_DEBUG_PERMISSION;
        DevToolsServerJni.get()
                .setRemoteDebuggingEnabled(mNativeDevToolsServer, enabled, allowDebugPermission);
    }

    @CalledByNative
    private static boolean checkDebugPermission(int pid, int uid) {
        String debugPermissionName =
                ContextUtils.getApplicationContext().getPackageName() + DEBUG_PERMISSION_SUFFIX;
        return ApiCompatibilityUtils.checkPermission(
                        ContextUtils.getApplicationContext(), debugPermissionName, pid, uid)
                == PackageManager.PERMISSION_GRANTED;
    }

    @NativeMethods
    interface Natives {
        long initRemoteDebugging(@JniType("std::string") String socketNamePrefix);

        void destroyRemoteDebugging(long devToolsServer);

        void setRemoteDebuggingEnabled(
                long devToolsServer, boolean enabled, boolean allowDebugPermission);
    }
}
