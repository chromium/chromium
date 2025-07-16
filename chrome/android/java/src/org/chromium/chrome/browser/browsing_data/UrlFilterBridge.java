// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browsing_data;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;

/**
 * A {@link UrlFilter} that delegates the matching to the native side.
 *
 * <p>BrowsingDataRemover on the C++ side will instantiate this class through its C++ counterpart
 * and pass it to browsing data storage backends on the Java side.
 */
@NullMarked
public class UrlFilterBridge implements UrlFilter {
    private long mNativeUrlFilterBridge;

    @Override
    public boolean matchesUrl(String url) {
        assert mNativeUrlFilterBridge != 0;
        return UrlFilterBridgeJni.get().matchesUrl(mNativeUrlFilterBridge, url);
    }

    /** Destroys the native counterpart of this object. */
    public void destroy() {
        assert mNativeUrlFilterBridge != 0;
        UrlFilterBridgeJni.get().destroy(mNativeUrlFilterBridge);
        mNativeUrlFilterBridge = 0;
    }

    /**
     * Called from C++ by |nativeUrlFilterBridge| to instantiate this class.
     * Note that this is the only way to construct an UrlFilterBridge; the constructor is private.
     *
     * @param nativeUrlFilterBridge The native counterpart that creates and owns this object.
     */
    @CalledByNative
    private static UrlFilterBridge create(long nativeUrlFilterBridge) {
        return new UrlFilterBridge(nativeUrlFilterBridge);
    }

    private UrlFilterBridge(long nativeUrlFilterBridge) {
        mNativeUrlFilterBridge = nativeUrlFilterBridge;
    }

    @NativeMethods
    interface Natives {
        boolean matchesUrl(long nativeUrlFilterBridge, @JniType("std::string") String url);

        void destroy(long nativeUrlFilterBridge);
    }
}
