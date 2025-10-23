// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.process_launcher;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.ContextWrapper;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.Build;
import android.os.Handler;
import android.os.Process;
import android.os.UserHandle;

import androidx.annotation.RequiresApi;

import org.chromium.base.AconfigFlaggedApiDelegate;
import org.chromium.base.BindingRequestQueue;
import org.chromium.base.ContextUtils;
import org.chromium.build.BuildConfig;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.lang.reflect.Method;
import java.util.concurrent.Executor;

/** Class of static helper methods to call Context.bindService variants. */
@NullMarked
public final class BindService {
    private static @Nullable Method sBindServiceAsUserMethod;
    private static @Nullable BinderCallCounter sBinderCallCounter;

    public static final class BinderCallCounter {
        public int mBindServiceCount;
        public int mRebindServiceCount;
        public int mUnbindServiceCount;
        public int mUpdateServiceGroupCount;
    }

    static boolean supportVariableConnections() {
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q
                && !BuildConfig.IS_INCREMENTAL_INSTALL;
    }

    // Note that handler is not guaranteed to be used, and client still need to correctly handle
    // callbacks on the UI thread.
    static boolean doBindService(
            Context context,
            Intent intent,
            ServiceConnection connection,
            int flags,
            Handler handler,
            Executor executor,
            @Nullable String instanceName) {
        if (sBinderCallCounter != null) {
            sBinderCallCounter.mBindServiceCount++;
        }
        if (supportVariableConnections() && instanceName != null) {
            return context.bindIsolatedService(intent, flags, instanceName, executor, connection);
        }

        if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.N) {
            return bindServiceByCall(context, intent, connection, flags);
        }

        try {
            return bindServiceByReflection(context, intent, connection, flags, handler);
        } catch (ReflectiveOperationException reflectionException) {
            try {
                return bindServiceByCall(context, intent, connection, flags);
            } catch (RuntimeException runtimeException) {
                // Include the reflectionException in crash reports.
                throw new RuntimeException(runtimeException.getMessage(), reflectionException);
            }
        }
    }

    @SuppressWarnings("NewApi")
    static void doRebindService(Context context, ServiceConnection connection, int flags) {
        if (sBinderCallCounter != null) {
            sBinderCallCounter.mRebindServiceCount++;
        }
        Context.BindServiceFlags bindServiceFlags = Context.BindServiceFlags.of(flags);
        if (context == ContextUtils.getApplicationContext()
                && ScopedServiceBindingBatch.shouldBatchUpdate()) {
            BindingRequestQueue queue = ScopedServiceBindingBatch.getBindingRequestQueue();
            // This should never be null because shouldBatchUpdate() checks that the feature is
            // enabled.
            assert queue != null;
            queue.rebind(connection, bindServiceFlags);
            return;
        }
        final AconfigFlaggedApiDelegate delegate = AconfigFlaggedApiDelegate.getInstance();
        if (delegate != null) {
            delegate.rebindService(context, connection, bindServiceFlags);
        }
    }

    static void doUnbindService(Context context, ServiceConnection connection) {
        if (sBinderCallCounter != null) {
            sBinderCallCounter.mUnbindServiceCount++;
        }
        if (context == ContextUtils.getApplicationContext()
                && ScopedServiceBindingBatch.shouldBatchUpdate()) {
            BindingRequestQueue queue = ScopedServiceBindingBatch.getBindingRequestQueue();
            // This should never be null because shouldBatchUpdate() checks that the feature is
            // enabled.
            assert queue != null;
            queue.unbind(connection);
            return;
        }
        context.unbindService(connection);
    }

    static void doUpdateServiceGroup(
            Context context, ServiceConnection connection, int group, int importanceInGroup) {
        if (sBinderCallCounter != null) {
            sBinderCallCounter.mUpdateServiceGroupCount++;
        }
        context.updateServiceGroup(connection, group, importanceInGroup);
    }

    /**
     * Enables counting of service binding Binder calls.
     *
     * <p>Note that counter is not thread-safe. setEnableCounting(), doBindService(),
     * doUnbindService(), doUpdateServiceGroup(), and getAndResetBinderCallCounter() should be
     * called on the same thread.
     *
     * @param enabled Whether to enable counting of binder calls.
     */
    public static void setEnableCounting(boolean enabled) {
        if (enabled) {
            sBinderCallCounter = new BinderCallCounter();
        } else {
            sBinderCallCounter = null;
        }
    }

    /**
     * Returns the number of bindService calls and resets the counter.
     *
     * @return The number of bindService calls.
     */
    public static @Nullable BinderCallCounter getAndResetBinderCallCounter() {
        BinderCallCounter counter = sBinderCallCounter;
        if (counter != null) {
            sBinderCallCounter = new BinderCallCounter();
        }
        return counter;
    }

    private static boolean bindServiceByCall(
            Context context, Intent intent, ServiceConnection connection, int flags) {
        return context.bindService(intent, connection, flags);
    }

    @RequiresApi(Build.VERSION_CODES.N)
    @SuppressLint("DiscouragedPrivateApi")
    private static boolean bindServiceByReflection(
            Context context,
            Intent intent,
            ServiceConnection connection,
            int flags,
            Handler handler)
            throws ReflectiveOperationException {
        if (sBindServiceAsUserMethod == null) {
            sBindServiceAsUserMethod =
                    Context.class.getDeclaredMethod(
                            "bindServiceAsUser",
                            Intent.class,
                            ServiceConnection.class,
                            int.class,
                            Handler.class,
                            UserHandle.class);
        }
        // No need for null checks or worry about infinite looping here. Otherwise a regular calls
        // into the ContextWrapper would lead to problems as well.
        while (context instanceof ContextWrapper) {
            context = ((ContextWrapper) context).getBaseContext();
        }
        return (Boolean)
                sBindServiceAsUserMethod.invoke(
                        context, intent, connection, flags, handler, Process.myUserHandle());
    }

    private BindService() {}
}
