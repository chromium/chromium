// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.base;

import android.app.IntentService;
import android.content.Context;
import android.content.Intent;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * IntentService base class which will call through to the given {@link Impl}. This class must be
 * present in the base module, while the Impl can be in the chrome module.
 */
@NullMarked
public class SplitCompatIntentService extends IntentService {
    private final String mServiceClassName;
    private Impl mImpl;

    public SplitCompatIntentService(String serviceClassName, String name) {
        super(name);
        mServiceClassName = serviceClassName;
    }

    @Override
    protected void attachBaseContext(Context baseContext) {
        mImpl =
                (Impl)
                        SplitCompatUtils.loadClassAndAdjustContextChrome(
                                baseContext, mServiceClassName);
        mImpl.setService(this);
        super.attachBaseContext(baseContext);
    }

    @Override
    protected void onHandleIntent(@Nullable Intent intent) {
        mImpl.onHandleIntent(intent);
    }

    public void attachBaseContextForTesting(Context context, Impl impl) {
        mImpl = impl;
        super.attachBaseContext(context);
    }

    /**
     * Holds the implementation of service logic. Will be called by {@link
     * SplitCompatIntentService}.
     */
    public abstract static class Impl {
        private @Nullable SplitCompatIntentService mService;

        protected final void setService(SplitCompatIntentService service) {
            mService = service;
            onServiceSet();
        }

        protected final @Nullable SplitCompatIntentService getService() {
            return mService;
        }

        protected void onServiceSet() {}

        protected abstract void onHandleIntent(@Nullable Intent intent);
    }
}
