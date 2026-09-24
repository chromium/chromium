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

import org.chromium.base.BaseFeatureList;
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
        if (ScopedServiceBindingBatch.shouldBatchUpdate()) {
            BindingRequestQueue queue = ScopedServiceBindingBatch.getBindingRequestQueue();
            // This should never be null because shouldBatchUpdate() checks that the feature is
            // enabled.
            assert queue != null;
            // Flush all enqueued unbind requests before binding a new service. The order of unbind
            // -> bind requests is important on the devices where process count limit is hit.
            // TODO(crbug.com/469633098): Skip flushing if there is no unbind request in the queue
            // (e.g. rebind requests only).
            queue.flush();
        }

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

    /**
     * This method should always be used when creating an instance of {@link
     * Context.BindServiceFlags}. This method removes the incompatible BIND_EXTERNAL_SERVICE flag if
     * present and replaces it with BIND_EXTERNAL_SERVICE_LONG. If you don't use this method when
     * creating BindServiceFlags, it could lead to an IllegalArgumentException on some devices.
     *
     * <p>Don't:
     *
     * <pre>
     *     int flags = ...;
     *     BindServiceFlags.of(flags);
     * </pre>
     *
     * Do:
     *
     * <pre>
     *     int flags = ...;
     *     BindServiceFlags.of(sanitizeFlagsForBindServiceFlags(flags));
     * </pre>
     *
     * This method can be cleaned up once Build.VERSION.SDK_INT >= U is always true.
     */
    public static long sanitizeFlagsForBindServiceFlags(int flags) {
        // crbug.com/482179609 BindServiceFlags is incompatible with BIND_EXTERNAL_SERVICE. We must
        // use BIND_EXTERNAL_SERVICE_LONG instead.
        long longFlags = flags;
        if ((longFlags & Context.BIND_EXTERNAL_SERVICE) != 0) {
            longFlags &= ~Context.BIND_EXTERNAL_SERVICE;
            longFlags |= Context.BIND_EXTERNAL_SERVICE_LONG;
        }
        return longFlags;
    }

    /**
     * Calls or enqueues a `Context.rebindService()` for {@code connection}.
     *
     * @param context The context used to bind the service.
     * @param connection The connection to rebind.
     * @param flags The flags to use for the rebind.
     * @param urgent Whether the updated flags must reach the system as soon as possible. A rebind
     *     which raises the priority of a process is latency sensitive: the process is already
     *     expected to produce visible content, so it must not keep its previous (lower) binding
     *     flags until the enclosing {@link ScopedServiceBindingBatch} is closed. See
     *     crbug.com/465607095.
     */
    @SuppressWarnings("NewApi")
    static void doRebindService(
            Context context, ServiceConnection connection, int flags, boolean urgent) {
        if (sBinderCallCounter != null) {
            sBinderCallCounter.mRebindServiceCount++;
        }
        Context.BindServiceFlags bindServiceFlags =
                Context.BindServiceFlags.of(sanitizeFlagsForBindServiceFlags(flags));
        if (context == ContextUtils.getApplicationContext()
                && ScopedServiceBindingBatch.shouldBatchUpdate()) {
            BindingRequestQueue queue = ScopedServiceBindingBatch.getBindingRequestQueue();
            // This should never be null because shouldBatchUpdate() checks that the feature is
            // enabled.
            assert queue != null;
            // Enqueue even when the request is urgent. The queue is keyed by ServiceConnection, so
            // sending the request directly instead would leave an already enqueued request for the
            // same connection in the queue, and that stale request would overwrite these flags when
            // the queue is flushed later.
            queue.rebind(connection, bindServiceFlags);
            if (urgent && BaseFeatureList.sRebindServiceBatchApiFlushOnUpgrade.getValue()) {
                queue.flush();
            }
            return;
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.CINNAMON_BUN) {
            context.rebindService(connection, bindServiceFlags);
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
