// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import javax.annotation.concurrent.GuardedBy;

/**
 * Diagnostic data collected during WebView startup, including durations and stack traces of
 * initialization locations.
 */
@NullMarked
public class StartupDiagnostics {
    /** Callback interface invoked when WebView startup has completed. */
    @FunctionalInterface
    public interface Callback {
        void onSuccess(StartupDiagnostics result);
    }

    private final Object mLock = new Object();

    @GuardedBy("mLock")
    private @Nullable Long mTotalTimeUiThreadChromiumInitMillis;

    @GuardedBy("mLock")
    private @Nullable Long mMaxTimePerTaskUiThreadChromiumInitMillis;

    @GuardedBy("mLock")
    private @Nullable Long mStartTimeMillis;

    @GuardedBy("mLock")
    private int mStartupMode;

    @GuardedBy("mLock")
    private int mStartCallSite;

    @GuardedBy("mLock")
    private int mFinishCallSite;

    @GuardedBy("mLock")
    private @Nullable Throwable mSynchronousChromiumInitLocation;

    @GuardedBy("mLock")
    private @Nullable Throwable mProviderInitOnMainLooperLocation;

    @GuardedBy("mLock")
    private @Nullable Throwable mAsynchronousChromiumInitLocation;

    public @Nullable Long getTotalTimeUiThreadChromiumInitMillis() {
        synchronized (mLock) {
            return mTotalTimeUiThreadChromiumInitMillis;
        }
    }

    public @Nullable Long getMaxTimePerTaskUiThreadChromiumInitMillis() {
        synchronized (mLock) {
            return mMaxTimePerTaskUiThreadChromiumInitMillis;
        }
    }

    public @Nullable Long getStartTimeMillis() {
        synchronized (mLock) {
            return mStartTimeMillis;
        }
    }

    public int getStartupMode() {
        synchronized (mLock) {
            return mStartupMode;
        }
    }

    public int getStartCallSite() {
        synchronized (mLock) {
            return mStartCallSite;
        }
    }

    public int getFinishCallSite() {
        synchronized (mLock) {
            return mFinishCallSite;
        }
    }

    public @Nullable Throwable getSynchronousChromiumInitLocationOrNull() {
        synchronized (mLock) {
            return mSynchronousChromiumInitLocation;
        }
    }

    public @Nullable Throwable getProviderInitOnMainLooperLocationOrNull() {
        synchronized (mLock) {
            return mProviderInitOnMainLooperLocation;
        }
    }

    public @Nullable Throwable getAsynchronousChromiumInitLocationOrNull() {
        synchronized (mLock) {
            return mAsynchronousChromiumInitLocation;
        }
    }

    public void setTotalTimeUiThreadChromiumInitMillis(Long time) {
        synchronized (mLock) {
            assert (mTotalTimeUiThreadChromiumInitMillis == null);
            mTotalTimeUiThreadChromiumInitMillis = time;
        }
    }

    public void setMaxTimePerTaskUiThreadChromiumInitMillis(Long time) {
        synchronized (mLock) {
            assert (mMaxTimePerTaskUiThreadChromiumInitMillis == null);
            mMaxTimePerTaskUiThreadChromiumInitMillis = time;
        }
    }

    public void setStartTimeMillis(Long time) {
        synchronized (mLock) {
            assert (mStartTimeMillis == null);
            mStartTimeMillis = time;
        }
    }

    public void setStartupMode(int startupMode) {
        synchronized (mLock) {
            mStartupMode = startupMode;
        }
    }

    public void setCallSites(int startCallSite, int finishCallSite) {
        synchronized (mLock) {
            mStartCallSite = startCallSite;
            mFinishCallSite = finishCallSite;
        }
    }

    public void setSynchronousChromiumInitLocation(Throwable t) {
        synchronized (mLock) {
            assert (mSynchronousChromiumInitLocation == null);
            mSynchronousChromiumInitLocation = t;
        }
    }

    public void setProviderInitOnMainLooperLocation(Throwable t) {
        synchronized (mLock) {
            assert (mProviderInitOnMainLooperLocation == null);
            mProviderInitOnMainLooperLocation = t;
        }
    }

    public void setAsynchronousChromiumInitLocation(Throwable t) {
        synchronized (mLock) {
            assert (mAsynchronousChromiumInitLocation == null);
            mAsynchronousChromiumInitLocation = t;
        }
    }
}
