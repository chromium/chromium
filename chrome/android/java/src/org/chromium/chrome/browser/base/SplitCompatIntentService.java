// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.base;

import android.app.IntentService;
import android.content.Context;
import android.content.Intent;

import org.chromium.base.BundleUtils;

/**
 * IntentService base class which will call through to the given {@link Impl}. This class must be
 * present in the base module, while the Impl can be in the chrome module.
 */
public class SplitCompatIntentService extends IntentService {
    private String mServiceClassName;
    private Impl mImpl;

    public SplitCompatIntentService(String serviceClassName, String name) {
        super(name);
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
    protected void onHandleIntent(Intent intent) {
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
        private SplitCompatIntentService mService;

        protected final void setService(SplitCompatIntentService service) {
            mService = service;
            onServiceSet();
        }

        protected final SplitCompatIntentService getService() {
            return mService;
        }

        protected void onServiceSet() {}

        protected abstract void onHandleIntent(Intent intent);
    }
}
