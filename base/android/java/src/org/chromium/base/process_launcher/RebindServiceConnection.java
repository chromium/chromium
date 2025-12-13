// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.process_launcher;

import android.content.ComponentName;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.Handler;
import android.os.IBinder;

import org.chromium.base.ContextUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.concurrent.Executor;

/**
 * RebindServiceConnection is to update a bound process in the LRU of the ProcessList of
 * OomAdjuster.
 *
 * <p>`Context.updateServiceGroup()` requires application to rebind the process to apply the change.
 * Also rebinding a process moves the process to the more recent position in the ProcessList even
 * without `Context.updateServiceGroup()`.
 *
 * <p>This RebindServiceConnection is a workaround for 2 issues of rebinding ServiceConnection.
 *
 * <p>* Poorly differentiated concepts of a binding and a connection.
 *
 * <p>In the public API, a `ServiceConnection` uniquely identifies a binding. However, AMS has an
 * additional `ConnectionRecord` concept, where for a given `ServiceConnection`, each
 * `Context.bindService()` call creates is a new `ConnectionRecord`. This means that repeatedly
 * re-binding with a given `ServiceConnection` will cause `ConnectionRecord` objects to accumulate
 * in AMS, and there is a hard limit how many `ConnectionRecord`s a process can have. Unfortunately,
 * the only way to free `ConnectionRecord` objects is to unbind the entire connection. This means
 * it's necessary to use a dedicated `ServiceConnection` to trigger re-binding for
 * `Context.updateServiceGroup()`, as the unbindings necessary to free `ConnectionRecord`s would
 * interfere with the lifecycle management of a normal binding.
 *
 * <p>* Accumulation of un-GC'ed Binder proxies
 *
 * <p>`LoadedApk$ServiceDispatcher$InnerConnection` is created per new `Context` and
 * `ServiceConnection` combination on `Context.bindService()` in the Android SDK. The inner
 * connection represents a binder proxy and is GC-ed when GC happened on AMS. Though Binder Proxy is
 * GC-ed eventually, if there are too many calls of pairs of `Context.bindService()` and
 * `Context.unbindService()` in a short term even with the same `ServiceConnection`, Chrome can be
 * killed due to too many Binder Proxy remaining.
 *
 * <p>To prevent the first issue, `ChildProcessConnection` uses this short living service connection
 * `RebindServiceConnection` and `RebindServiceConnection.rebind()` unbinds itself just after
 * binding it if maxDeferredConnections is zero.
 *
 * <p>If the second issue is problematic, RebindServiceConnection supports deferring unbinding so
 * that it can be reused for multiple processes without increasing the Binder Proxy by setting
 * maxDeferredConnections as non-zero. The `ConnectionRecord`s in AMS are cleared when the number of
 * deferred connections reaches to maxDeferredConnections.
 *
 * <p>When `ChildProcessConnection` wants to kill the process (e.g. `unbind()`), keeping the
 * deferred connections prevents the process from being freed by AMS. `ChildProcessConnection` have
 * to call `RebindServiceConnection.unbind()` to clear all deferred connections when
 * `ChildProcessConnection` unbinds the waived binding to avoid the leak.
 */
@NullMarked
/* package */ final class RebindServiceConnection implements ServiceConnection {
    private final int mMaxDeferredConnections;
    private int mDeferredConnections;
    private final Handler mHandler;
    private final Executor mExecutor;

    RebindServiceConnection(int maxDeferredConnections) {
        mMaxDeferredConnections = maxDeferredConnections;
        mHandler = new Handler();
        mExecutor =
                (Runnable runnable) -> {
                    // We don't mind if the callback is executed on any thread because
                    // RebindServiceConnection does nothing on its callbacks.
                    runnable.run();
                };
    }

    void rebind(Intent bindIntent, int bindFlags, @Nullable String instanceName) {
        BindService.doBindService(
                ContextUtils.getApplicationContext(),
                bindIntent,
                this,
                bindFlags,
                mHandler,
                mExecutor,
                instanceName);
        if (mDeferredConnections >= mMaxDeferredConnections) {
            BindService.doUnbindService(ContextUtils.getApplicationContext(), this);
            mDeferredConnections = 0;
        } else {
            mDeferredConnections += 1;
        }
    }

    void unbind() {
        if (mDeferredConnections > 0) {
            BindService.doUnbindService(ContextUtils.getApplicationContext(), this);
            mDeferredConnections = 0;
        }
    }

    @Override
    public void onServiceConnected(ComponentName name, IBinder service) {}

    @Override
    public void onServiceDisconnected(ComponentName name) {}
}
