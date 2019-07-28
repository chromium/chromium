// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.process_launcher;

import android.annotation.TargetApi;
import android.content.Context;
import android.content.ContextWrapper;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.Build;
import android.os.Handler;
import android.os.Process;
import android.os.UserHandle;

import org.chromium.base.BuildInfo;
import org.chromium.base.StrictModeContext;

import java.lang.reflect.Method;
import java.util.concurrent.Executor;

/**
 * Class of static helper methods to call Context.bindService variants.
 */
final class BindService {
    private static final Method sDoBindServiceQMethod;
    private static final Method sUpdateServiceGroupQMethod;
    static {
        Method bindMethod = null;
        Method updateServiceGroupMethod = null;
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            if (BuildInfo.isAtLeastQ()) {
                Class<?> clazz =
                        Class.forName("org.chromium.base.process_launcher.BindServiceInternal");
                bindMethod = clazz.getDeclaredMethod("doBindServiceQ", Context.class, Intent.class,
                        ServiceConnection.class, int.class, Executor.class, String.class);
                updateServiceGroupMethod = clazz.getDeclaredMethod("updateServiceGroupQ",
                        Context.class, ServiceConnection.class, int.class, int.class);
            }
        } catch (Exception e) {
            // Ignore exceptions.
        } finally {
            sDoBindServiceQMethod = bindMethod;
            sUpdateServiceGroupQMethod = updateServiceGroupMethod;
        }
    }

    private static Method sBindServiceAsUserMethod;

    static boolean supportVariableConnections() {
        return sDoBindServiceQMethod != null && sUpdateServiceGroupQMethod != null;
    }

    // Note that handler is not guaranteed to be used, and client still need to correctly handle
    // callbacks on the UI thread.
    static boolean doBindService(Context context, Intent intent, ServiceConnection connection,
            int flags, Handler handler, Executor executor, String instanceName) {
        if (supportVariableConnections()) {
            try {
                return (boolean) sDoBindServiceQMethod.invoke(
                        null, context, intent, connection, flags, executor, instanceName);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.N) {
            return bindServiceByCall(context, intent, connection, flags);
        }

        try {
            return bindServiceByReflection(context, intent, connection, flags, handler);
        } catch (ReflectiveOperationException e) {
            return bindServiceByCall(context, intent, connection, flags);
        }
    }

    static void updateServiceGroup(
            Context context, ServiceConnection connection, int group, int importance) {
        try {
            sUpdateServiceGroupQMethod.invoke(null, context, connection, group, importance);
        } catch (Exception e) {
            // Ignore reflection errors.
        }
    }

    private static boolean bindServiceByCall(
            Context context, Intent intent, ServiceConnection connection, int flags) {
        return context.bindService(intent, connection, flags);
    }

    @TargetApi(Build.VERSION_CODES.N)
    private static boolean bindServiceByReflection(Context context, Intent intent,
            ServiceConnection connection, int flags, Handler handler)
            throws ReflectiveOperationException {
        if (sBindServiceAsUserMethod == null) {
            sBindServiceAsUserMethod =
                    Context.class.getDeclaredMethod("bindServiceAsUser", Intent.class,
                            ServiceConnection.class, int.class, Handler.class, UserHandle.class);
        }
        // No need for null checks or worry about infinite looping here. Otherwise a regular calls
        // into the ContextWrapper would lead to problems as well.
        while (context instanceof ContextWrapper) {
            context = ((ContextWrapper) context).getBaseContext();
        }
        return (Boolean) sBindServiceAsUserMethod.invoke(
                context, intent, connection, flags, handler, Process.myUserHandle());
    }

    private BindService() {}
}
