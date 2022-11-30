// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.base;

import android.content.Context;
import android.content.Intent;
import android.widget.RemoteViewsService;

import org.chromium.base.BundleUtils;

/**
 * RemoteViewsService base class which will call through to the given {@link Impl}. This class must
 * be present in the base module, while the Impl can be in the chrome module.
 */
public class SplitCompatRemoteViewsService extends RemoteViewsService {
    private String mServiceClassName;
    private Impl mImpl;

    public SplitCompatRemoteViewsService(String serviceClassName) {
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
    public RemoteViewsFactory onGetViewFactory(Intent intent) {
        return mImpl.onGetViewFactory(intent);
    }

    /**
     * Holds the implementation of service logic. Will be called by {@link
     * SplitCompatRemoteViewsService}.
     */
    public abstract static class Impl {
        private SplitCompatRemoteViewsService mService;

        protected final void setService(SplitCompatRemoteViewsService service) {
            mService = service;
        }

        protected final SplitCompatRemoteViewsService getService() {
            return mService;
        }

        public abstract RemoteViewsFactory onGetViewFactory(Intent intent);
    }
}
