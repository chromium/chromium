// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.chromium.build.annotations.NullMarked;
import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

/** Settings for toggling back/forward cache behavior. */
@JNINamespace("android_webview")
@NullMarked
public class AwBackForwardCacheSettings extends AwSupportLibIsomorphic {
    private final int mTimeoutInSeconds;
    private final int mMaxPagesInCache;

    public AwBackForwardCacheSettings(int timeoutInSeconds, int maxPagesInCache) {
        mTimeoutInSeconds = timeoutInSeconds;
        mMaxPagesInCache = maxPagesInCache;
    }

    @CalledByNative
    public int getTimeoutInSeconds() {
        return mTimeoutInSeconds;
    }

    @CalledByNative
    public int getMaxPagesInCache() {
        return mMaxPagesInCache;
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) {
            return true;
        }
        if (!(o instanceof AwBackForwardCacheSettings)) {
            return false;
        }
        AwBackForwardCacheSettings that = (AwBackForwardCacheSettings) o;
        return mTimeoutInSeconds == that.mTimeoutInSeconds
                && mMaxPagesInCache == that.mMaxPagesInCache;
    }
}
