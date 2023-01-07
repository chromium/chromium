// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.base;

import android.content.Context;
import android.os.PersistableBundle;

import org.chromium.base.BundleUtils;
import org.chromium.components.minidump_uploader.MinidumpUploadJob;
import org.chromium.components.minidump_uploader.MinidumpUploadJobService;

/**
 * MinidumpUploadJobService base class which will call through to the given {@link Impl}. This class
 * must be present in the base module, while the Impl can be in the chrome module.
 */
public class SplitCompatMinidumpUploadJobService extends MinidumpUploadJobService {
    private String mServiceClassName;
    private Impl mImpl;

    public SplitCompatMinidumpUploadJobService(String serviceClassName) {
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
    protected MinidumpUploadJob createMinidumpUploadJob(PersistableBundle permissions) {
        return mImpl.createMinidumpUploadJob(permissions);
    }

    /**
     * Holds the implementation of service logic. Will be called by {@link
     * SplitCompatMinidumpUploadJobService}.
     */
    public abstract static class Impl {
        private SplitCompatMinidumpUploadJobService mService;

        protected final void setService(SplitCompatMinidumpUploadJobService service) {
            mService = service;
        }

        protected final MinidumpUploadJobService getService() {
            return mService;
        }

        protected abstract MinidumpUploadJob createMinidumpUploadJob(PersistableBundle extras);
    }
}
