// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.chromium.android_webview.StartupTasksRunner.StartupTimings;
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
    private @Nullable StartupTimings mTimings;

    @GuardedBy("mLock")
    private @Nullable Throwable mSynchronousChromiumInitLocation;

    @GuardedBy("mLock")
    private @Nullable Throwable mProviderInitOnMainLooperLocation;

    @GuardedBy("mLock")
    private @Nullable Throwable mAsynchronousChromiumInitLocation;

    public void setStartupTimings(StartupTimings timings) {
        synchronized (mLock) {
            assert mTimings == null;
            mTimings = timings;
        }
    }

    public @Nullable StartupTimings getStartupTimings() {
        synchronized (mLock) {
            return mTimings;
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
