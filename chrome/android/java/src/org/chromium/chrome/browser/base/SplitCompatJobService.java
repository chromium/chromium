// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.base;

import android.app.job.JobParameters;
import android.app.job.JobService;
import android.content.Context;

import org.chromium.base.BundleUtils;

/**
 * JobService base class which will call through to the given {@link Impl}. This class must be
 * present in the base module, while the Impl can be in the chrome module.
 */
public class SplitCompatJobService extends JobService {
    private String mServiceClassName;
    private String mSplitName;
    private Impl mImpl;

    public SplitCompatJobService(String serviceClassName) {
        mServiceClassName = serviceClassName;
    }

    public SplitCompatJobService(String serviceClassName, String splitName) {
        mServiceClassName = serviceClassName;
        mSplitName = splitName;
    }

    @Override
    protected void attachBaseContext(Context context) {
        // Make sure specified split is installed, otherwise fall back to chrome split.
        if (mSplitName != null && BundleUtils.isIsolatedSplitInstalled(mSplitName)) {
            context = BundleUtils.createIsolatedSplitContext(context, mSplitName);
        } else {
            context = SplitCompatApplication.createChromeContext(context);
        }
        mImpl = (Impl) BundleUtils.newInstance(context, mServiceClassName);
        mImpl.setService(this);
        super.attachBaseContext(context);
    }

    @Override
    public boolean onStartJob(JobParameters params) {
        return mImpl.onStartJob(params);
    }

    @Override
    public boolean onStopJob(JobParameters params) {
        return mImpl.onStopJob(params);
    }

    /**
     * Holds the implementation of service logic. Will be called by {@link SplitCompatJobService}.
     */
    public abstract static class Impl {
        private SplitCompatJobService mService;

        private void setService(SplitCompatJobService service) {
            mService = service;
        }

        protected final JobService getService() {
            return mService;
        }

        public abstract boolean onStartJob(JobParameters params);

        public abstract boolean onStopJob(JobParameters params);
    }
}
