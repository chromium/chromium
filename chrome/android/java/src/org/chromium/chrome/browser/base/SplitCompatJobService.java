// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.base;

import static org.chromium.chrome.browser.base.SplitCompatApplication.CHROME_SPLIT_NAME;

import android.app.job.JobParameters;
import android.app.job.JobService;
import android.content.Context;

import org.chromium.base.BundleUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * JobService base class which will call through to the given {@link Impl}. This class must be
 * present in the base module, while the Impl can be in the chrome module.
 */
@NullMarked
public class SplitCompatJobService extends JobService {
    private final String mServiceClassName;
    private final String mSplitName;
    private Impl mImpl;

    public SplitCompatJobService(String serviceClassName) {
        this(serviceClassName, CHROME_SPLIT_NAME);
    }

    public SplitCompatJobService(String serviceClassName, String splitName) {
        mServiceClassName = serviceClassName;
        mSplitName = splitName;
    }

    @Override
    protected void attachBaseContext(Context baseContext) {
        String splitToLoad = CHROME_SPLIT_NAME;
        // Make sure specified split is installed, otherwise fall back to chrome split.
        if (BundleUtils.isIsolatedSplitInstalled(mSplitName)) {
            splitToLoad = mSplitName;
        }
        mImpl =
                (Impl)
                        SplitCompatUtils.loadClassAndAdjustContext(
                                baseContext, mServiceClassName, splitToLoad);
        mImpl.setService(this);
        super.attachBaseContext(baseContext);
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
        private @Nullable SplitCompatJobService mService;

        private void setService(SplitCompatJobService service) {
            mService = service;
        }

        protected final @Nullable JobService getService() {
            return mService;
        }

        public abstract boolean onStartJob(JobParameters params);

        public abstract boolean onStopJob(JobParameters params);
    }
}
