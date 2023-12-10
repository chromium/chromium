// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.base;

import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.os.IBinder;

import org.chromium.base.BundleUtils;

/**
 * Service base class which will call through to the given {@link Impl}. This class must be present
 * in the base module, while the Impl can be in the chrome module.
 */
public class SplitCompatService extends Service {
    private String mServiceClassName;
    private Impl mImpl;

    public SplitCompatService(String serviceClassName) {
        mServiceClassName = serviceClassName;
    }

    @Override
    protected void attachBaseContext(Context context) {
        context = SplitCompatApplication.createChromeContext(context);
        mImpl = (Impl) BundleUtils.newInstance(context, mServiceClassName);
        mImpl.setService(this);
        super.attachBaseContext(context);
    }

    @Override
    public void onCreate() {
        super.onCreate();
        mImpl.onCreate();
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        return mImpl.onStartCommand(intent, flags, startId);
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        mImpl.onDestroy();
    }

    @Override
    public void onTaskRemoved(Intent rootIntent) {
        super.onTaskRemoved(rootIntent);
        mImpl.onTaskRemoved(rootIntent);
    }

    @Override
    public void onLowMemory() {
        super.onLowMemory();
        mImpl.onLowMemory();
    }

    @Override
    public boolean onUnbind(Intent intent) {
        return mImpl.onUnbind(intent);
    }

    @Override
    public IBinder onBind(Intent intent) {
        return mImpl.onBind(intent);
    }

    private int superOnStartCommand(Intent intent, int flags, int startId) {
        return super.onStartCommand(intent, flags, startId);
    }

    private boolean superOnUnbind(Intent intent) {
        return super.onUnbind(intent);
    }

    public void attachBaseContextForTesting(Context context, Impl impl) {
        mImpl = impl;
        super.attachBaseContext(context);
    }

    /** Holds the implementation of service logic. Will be called by {@link SplitCompatService}. */
    public abstract static class Impl {
        private SplitCompatService mService;

        protected final void setService(SplitCompatService service) {
            mService = service;
        }

        protected final Service getService() {
            return mService;
        }

        public void onCreate() {}

        public int onStartCommand(Intent intent, int flags, int startId) {
            return mService.superOnStartCommand(intent, flags, startId);
        }

        public void onDestroy() {}

        public void onTaskRemoved(Intent rootIntent) {}

        public void onLowMemory() {}

        public boolean onUnbind(Intent intent) {
            return mService.superOnUnbind(intent);
        }

        public abstract IBinder onBind(Intent intent);
    }
}
