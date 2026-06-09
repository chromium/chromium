// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.android_webview.common.Lifetime;
import org.chromium.build.annotations.NullMarked;

/** Manages Profile-level cache configuration. */
@JNINamespace("android_webview")
@Lifetime.Profile
@NullMarked
public class AwHttpCacheManager extends AwSupportLibIsomorphic {
    private final long mNativeAwHttpCacheManager;

    public AwHttpCacheManager(long nativeAwHttpCacheManager) {
        mNativeAwHttpCacheManager = nativeAwHttpCacheManager;
    }

    @CalledByNative
    private static AwHttpCacheManager create(long nativeAwHttpCacheManager) {
        return new AwHttpCacheManager(nativeAwHttpCacheManager);
    }

    public long getDefaultQuotaBytes() {
        return AwHttpCacheManagerJni.get().getDefaultQuotaBytes(mNativeAwHttpCacheManager);
    }

    public void useDefaultQuota() {
        AwHttpCacheManagerJni.get().useDefaultQuota(mNativeAwHttpCacheManager);
    }

    public boolean isUsingDefaultQuota() {
        return AwHttpCacheManagerJni.get().isUsingDefaultQuota(mNativeAwHttpCacheManager);
    }

    public long getQuotaBytes() {
        return AwHttpCacheManagerJni.get().getQuotaBytes(mNativeAwHttpCacheManager);
    }

    public void setQuotaBytes(long quotaInBytes) {
        if (quotaInBytes < 0) {
            throw new IllegalArgumentException(
                    "Cache quota must be non-negative but was " + quotaInBytes);
        }
        AwHttpCacheManagerJni.get().setQuotaBytes(mNativeAwHttpCacheManager, quotaInBytes);
    }

    @NativeMethods
    interface Natives {
        long getDefaultQuotaBytes(long nativeAwHttpCacheManager);

        void useDefaultQuota(long nativeAwHttpCacheManager);

        boolean isUsingDefaultQuota(long nativeAwHttpCacheManager);

        long getQuotaBytes(long nativeAwHttpCacheManager);

        void setQuotaBytes(long nativeAwHttpCacheManager, long quotaInBytes);
    }
}
