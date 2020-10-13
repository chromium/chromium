// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.base;

import android.app.Application;
import android.content.Context;
import android.content.Intent;
import android.content.res.Configuration;
import android.os.Bundle;

import androidx.annotation.CallSuper;

import org.chromium.base.ContextUtils;

/**
 * Application base class which will call through to the given {@link Impl}. Application classes
 * which extend this class should also extend {@link Impl}, and call {@link #setImpl(Impl)} before
 * calling {@link attachBaseContext(Context)}.
 */
public class SplitCompatApplication extends Application {
    private Impl mImpl;

    /**
     * Holds the implementation of application logic. Will be called by {@link
     * SplitCompatApplication}.
     */
    public static class Impl {
        private SplitCompatApplication mApplication;

        private final void setApplication(SplitCompatApplication application) {
            mApplication = application;
        }

        protected final SplitCompatApplication getApplication() {
            return mApplication;
        }

        @CallSuper
        public void attachBaseContext(Context context) {
            mApplication.superAttachBaseContext(context);
        }

        @CallSuper
        public void startActivity(Intent intent, Bundle options) {
            mApplication.superStartActivity(intent, options);
        }

        public void onCreate() {}

        public void onTrimMemory(int level) {}
        public void onConfigurationChanged(Configuration newConfig) {}
    }

    public final void setImpl(Impl impl) {
        assert mImpl == null;
        mImpl = impl;
        mImpl.setApplication(this);
    }

    /**
     * This exposes the super method so it can be called inside the Impl class code instead of just
     * at the start.
     */
    private void superAttachBaseContext(Context context) {
        super.attachBaseContext(context);
    }

    /**
     * This exposes the super method so it can be called inside the Impl class code instead of just
     * at the start.
     */
    private void superStartActivity(Intent intent, Bundle options) {
        super.startActivity(intent, options);
    }

    @Override
    protected void attachBaseContext(Context context) {
        mImpl.attachBaseContext(context);
    }

    @Override
    public void onCreate() {
        super.onCreate();
        mImpl.onCreate();
    }

    @Override
    public void onTrimMemory(int level) {
        super.onTrimMemory(level);
        mImpl.onTrimMemory(level);
    }

    /** Forward all startActivity() calls to the two argument version. */
    @Override
    public void startActivity(Intent intent) {
        startActivity(intent, null);
    }

    @Override
    public void startActivity(Intent intent, Bundle options) {
        mImpl.startActivity(intent, options);
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        mImpl.onConfigurationChanged(newConfig);
    }

    public static boolean isBrowserProcess() {
        return !ContextUtils.getProcessName().contains(":");
    }
}
