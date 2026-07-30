// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.extensions.api.messaging;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.IBinder;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Manages a single ServiceConnection to an Android app. */
@NullMarked
public class NativeMessagingConnection implements ServiceConnection {
    private static final String TAG = "NMConnection";
    public static final String ACTION_NATIVE_MESSAGING =
            "org.chromium.chrome.browser.extensions.messaging.action.NATIVE_MESSAGING";

    public interface Observer {
        void onUnbound(String packageName);
    }

    private final String mPackageName;
    private boolean mIsBound;

    private @Nullable IBrowserNativeMessageService mService;
    private @Nullable Observer mObserver;

    public NativeMessagingConnection(String packageName, Observer observer) {
        mPackageName = packageName;
        mObserver = observer;

        Context context = ContextUtils.getApplicationContext();
        Intent intent = new Intent(ACTION_NATIVE_MESSAGING);
        intent.setPackage(mPackageName);

        mIsBound = context.bindService(intent, this, Context.BIND_AUTO_CREATE);

        if (!mIsBound) {
            Log.e(TAG, "Failed to bind to service for package: " + mPackageName);
        }
    }

    public boolean isBound() {
        return mIsBound;
    }

    @Nullable IBrowserNativeMessageService getServiceForTesting() {
        return mService;
    }

    public void unbind() {
        if (mIsBound) {
            try {
                ContextUtils.getApplicationContext().unbindService(this);
            } catch (IllegalArgumentException e) {
                Log.w(TAG, "Service was not registered during unbind", e);
            }
            mIsBound = false;
        }

        mService = null;
        if (mObserver != null) {
            mObserver.onUnbound(mPackageName);
            mObserver = null;
        }
    }

    @Override
    public void onServiceConnected(ComponentName name, IBinder service) {
        mService = IBrowserNativeMessageService.Stub.asInterface(service);
    }

    @Override
    public void onServiceDisconnected(ComponentName name) {
        // We unbind here because the app's service has likely crashed and not
        // unbinding and expecting the service to auto-startup again causes a
        // mismatch in state parity where:
        // - the extension might have all the state/session records
        // - unless the app proactively saves messaging "sessions" as they
        //   happen it will not retain any state.
        // Therefore it's cleaner to just unbind.
        Log.i(TAG, "Service disconnected for package: " + name.getPackageName());
        unbind();
    }

    @Override
    public void onBindingDied(ComponentName name) {
        Log.w(TAG, "Binding died for package: " + name.getPackageName());
        unbind();
    }

    @Override
    public void onNullBinding(ComponentName name) {
        Log.w(TAG, "Null binding for package: " + name.getPackageName());
        unbind();
    }
}
