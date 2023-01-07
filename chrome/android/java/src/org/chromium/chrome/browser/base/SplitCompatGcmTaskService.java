// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.base;

import android.content.Context;

import com.google.android.gms.gcm.GcmTaskService;
import com.google.android.gms.gcm.TaskParams;

import org.chromium.base.BundleUtils;

/**
 * GcmTaskService base class which will call through to the given {@link Impl}. This class must be
 * present in the base module, while the Impl can be in the chrome module.
 */
public class SplitCompatGcmTaskService extends GcmTaskService {
    private String mServiceClassName;
    private Impl mImpl;

    public SplitCompatGcmTaskService(String serviceClassName) {
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
    public int onRunTask(TaskParams params) {
        return mImpl.onRunTask(params);
    }

    @Override
    public void onInitializeTasks() {
        mImpl.onInitializeTasks();
    }

    /**
     * Holds the implementation of service logic. Will be called by {@link
     * SplitCompatGcmTaskService}.
     */
    public abstract static class Impl {
        private SplitCompatGcmTaskService mService;

        protected final void setService(SplitCompatGcmTaskService service) {
            mService = service;
        }

        protected final GcmTaskService getService() {
            return mService;
        }

        public abstract int onRunTask(TaskParams params);

        public void onInitializeTasks() {}
    }
}
