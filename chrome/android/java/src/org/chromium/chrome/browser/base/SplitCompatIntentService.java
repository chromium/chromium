// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.base;

import android.app.IntentService;
import android.content.Context;
import android.content.Intent;
import android.os.IBinder;

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
        context = SplitCompatUtils.createChromeContext(context);
        mImpl = (Impl) SplitCompatUtils.newInstance(context, mServiceClassName);
        mImpl.setService(this);
        super.attachBaseContext(context);
    }

    private IBinder superOnBind(Intent intent) {
        return super.onBind(intent);
    }

    @Override
    protected void onHandleIntent(Intent intent) {
        mImpl.onHandleIntent(intent);
    }

    /**
     * Holds the implementation of service logic. Will be called by {@link
     * SplitCompatIntentService}.
     */
    public abstract static class Impl {
        private SplitCompatIntentService mService;

        private void setService(SplitCompatIntentService service) {
            mService = service;
        }

        protected final IntentService getService() {
            return mService;
        }

        public IBinder onBind(Intent intent) {
            return mService.superOnBind(intent);
        }

        protected abstract void onHandleIntent(Intent intent);
    }
}
