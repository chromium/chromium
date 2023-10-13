// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.ComponentCallbacks2;
import android.content.res.Configuration;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;

/**
 * Android part of NearOomMonitor. This collects Java memory pressure signals
 * and delivers them to C++ counterpart.
 */
class NearOomMonitor implements ComponentCallbacks2 {
    private final long mNearOomMonitor;

    @CalledByNative
    private static NearOomMonitor create(long nearOomMonitor) {
        return new NearOomMonitor(nearOomMonitor);
    }

    private NearOomMonitor(long nearOomMonitor) {
        mNearOomMonitor = nearOomMonitor;
        ContextUtils.getApplicationContext().registerComponentCallbacks(this);
    }

    @Override
    public void onTrimMemory(int level) {}

    @Override
    public void onLowMemory() {
        NearOomMonitorJni.get().onLowMemory(mNearOomMonitor, NearOomMonitor.this);
    }

    @Override
    public void onConfigurationChanged(Configuration config) {}

    @NativeMethods
    interface Natives {
        void onLowMemory(long nativeNearOomMonitor, NearOomMonitor caller);
    }
}
