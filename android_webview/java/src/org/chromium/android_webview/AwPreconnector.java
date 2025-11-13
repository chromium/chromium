// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.android_webview.common.Lifetime;
import org.chromium.url.GURL;

/**
 * The WebView preconnect API is similar to link rel=preconnect [1]. It performs a DNS lookup to the
 * provided domain and then opens a connection. This will speed up the next load to that domain.
 *
 * <p>Note: preconnect operates on origins, but for convenience we allow developers to provide us
 * with full URLs and we'll transform it into an origin.
 *
 * <p>This class (owned by its C++ counterpart) allows performing preconnect requests.
 *
 * <p>[1]: https://developer.mozilla.org/en-US/docs/Web/HTML/Reference/Attributes/rel/preconnect
 */
@JNINamespace("android_webview")
@Lifetime.Profile
public class AwPreconnector {
    private long mNativeAwPreconnector;

    public AwPreconnector(long nativeAwPreconnector) {
        mNativeAwPreconnector = nativeAwPreconnector;
    }

    @CalledByNative
    public static AwPreconnector create(long nativeAwPreconnector) {
        return new AwPreconnector(nativeAwPreconnector);
    }

    @CalledByNative
    private void destroy() {
        mNativeAwPreconnector = 0;
    }

    /** Preconnects to the domain in the given URL, see class javadoc for more info. */
    public void preconnect(GURL url) {
        if (mNativeAwPreconnector == 0) {
            throw new IllegalArgumentException("Preconnect called after object destroyed.");
        }
        boolean validUrl = AwPreconnectorJni.get().preconnect(mNativeAwPreconnector, url);

        if (!validUrl) {
            throw new IllegalArgumentException("Invalid URL: " + url.getPossiblyInvalidSpec());
        }
    }

    @NativeMethods
    interface Natives {
        boolean preconnect(long nativeAwPreconnector, @JniType("GURL") GURL url);
    }
}
