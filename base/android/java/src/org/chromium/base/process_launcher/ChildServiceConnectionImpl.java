// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.process_launcher;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.Handler;
import android.os.IBinder;

import org.chromium.base.Log;
import org.chromium.base.TraceEvent;

import java.util.concurrent.Executor;

/** Implementation of ChildServiceConnection that does connect to a service. */
/* package */ class ChildServiceConnectionImpl
        implements ChildServiceConnection, ServiceConnection {
    private static final String TAG = "ChildServiceConn";

    private final Context mContext;
    private final Intent mBindIntent;
    private final int mBindFlags;
    private final Handler mHandler;
    private final Executor mExecutor;
    private ChildServiceConnectionDelegate mDelegate;
    private final String mInstanceName;
    private boolean mBound;

    /* package */ ChildServiceConnectionImpl(
            Context context,
            Intent bindIntent,
            int bindFlags,
            Handler handler,
            Executor executor,
            ChildServiceConnectionDelegate delegate,
            String instanceName) {
        mContext = context;
        mBindIntent = bindIntent;
        mBindFlags = bindFlags;
        mHandler = handler;
        mExecutor = executor;
        mDelegate = delegate;
        mInstanceName = instanceName;
    }

    @Override
    public boolean bindServiceConnection() {
        try {
            TraceEvent.begin("ChildServiceConnectionImpl.bindServiceConnection");
            mBound =
                    BindService.doBindService(
                            mContext,
                            mBindIntent,
                            this,
                            mBindFlags,
                            mHandler,
                            mExecutor,
                            mInstanceName);
        } finally {
            TraceEvent.end("ChildServiceConnectionImpl.bindServiceConnection");
        }
        return mBound;
    }

    @Override
    public void unbindServiceConnection() {
        if (mBound) {
            mContext.unbindService(this);
            mBound = false;
        }
    }

    @Override
    public boolean isBound() {
        return mBound;
    }

    @Override
    public void updateGroupImportance(int group, int importanceInGroup) {
        // ChildProcessConnection checks there is a real connection to the service before calling
        // this, and this `isBound` check should in theory be unnecessary. However this is still
        // tripped on some devices where another service connection bound successfully but this
        // service connection failed in `bindServiceConnection`. Such a case is not expected OS
        // behavior and is not handled. However, avoid crashing in `updateServiceGroup` by doing
        // this check here.
        if (!isBound()) {
            return;
        }
        if (BindService.supportVariableConnections()) {
            try {
                mContext.updateServiceGroup(this, group, importanceInGroup);
            } catch (IllegalArgumentException e) {
                // There is an unavoidable race here binding might be removed for example due to a
                // crash, which has not been processed on the launcher thread.
                // Ignore these. See crbug.com/1026626 and crbug.com/1026626 for context.
                return;
            }
            BindService.doBindService(
                    mContext, mBindIntent, this, mBindFlags, mHandler, mExecutor, mInstanceName);
        }
    }

    @Override
    public void retire() {
        mDelegate = null;
        unbindServiceConnection();
    }

    @Override
    public void onServiceConnected(ComponentName className, final IBinder service) {
        if (mDelegate == null) {
            Log.w(TAG, "onServiceConnected after timeout " + className);
            return;
        }
        mDelegate.onServiceConnected(service);
    }

    // Called on the main thread to notify that the child service did not disconnect gracefully.
    @Override
    public void onServiceDisconnected(ComponentName className) {
        if (mDelegate != null) mDelegate.onServiceDisconnected();
    }
}
