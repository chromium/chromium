// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.base;

import android.content.Context;
import android.content.Intent;
import android.widget.RemoteViewsService;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * RemoteViewsService base class which will call through to the given {@link Impl}. This class must
 * be present in the base module, while the Impl can be in the chrome module.
 */
@NullMarked
public class SplitCompatRemoteViewsService extends RemoteViewsService {
    private final String mServiceClassName;
    private Impl mImpl;

    public SplitCompatRemoteViewsService(String serviceClassName) {
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
    public @Nullable RemoteViewsFactory onGetViewFactory(Intent intent) {
        return mImpl.onGetViewFactory(intent);
    }

    /**
     * Holds the implementation of service logic. Will be called by {@link
     * SplitCompatRemoteViewsService}.
     */
    public abstract static class Impl {
        private @Nullable SplitCompatRemoteViewsService mService;

        protected final void setService(SplitCompatRemoteViewsService service) {
            mService = service;
        }

        protected final @Nullable SplitCompatRemoteViewsService getService() {
            return mService;
        }

        public abstract @Nullable RemoteViewsFactory onGetViewFactory(Intent intent);
    }
}
