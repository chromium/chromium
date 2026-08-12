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
            // The setter should only be called once.
            assert (mTotalTimeUiThreadChromiumInitMillis == null);
            mTotalTimeUiThreadChromiumInitMillis = time;
        }
    }

    public void setMaxTimePerTaskUiThreadChromiumInitMillis(Long time) {
        synchronized (mLock) {
            // The setter should only be called once.
            assert (mMaxTimePerTaskUiThreadChromiumInitMillis == null);
            mMaxTimePerTaskUiThreadChromiumInitMillis = time;
        }
    }

    public void setSynchronousChromiumInitLocation(Throwable t) {
        synchronized (mLock) {
            // The setter should only be called once.
            assert (mSynchronousChromiumInitLocation == null);
            mSynchronousChromiumInitLocation = t;
        }
    }

    public void setProviderInitOnMainLooperLocation(Throwable t) {
        synchronized (mLock) {
            // The setter should only be called once.
            assert (mProviderInitOnMainLooperLocation == null);
            mProviderInitOnMainLooperLocation = t;
        }
    }

    public void setAsynchronousChromiumInitLocation(Throwable t) {
        synchronized (mLock) {
            // The setter should only be called once.
            assert (mAsynchronousChromiumInitLocation == null);
            mAsynchronousChromiumInitLocation = t;
        }
    }
}
