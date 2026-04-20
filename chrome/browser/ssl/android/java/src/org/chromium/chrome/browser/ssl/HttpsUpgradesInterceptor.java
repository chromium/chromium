// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ssl;

import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;

/** Java interface for the C++ HttpsUpgradesInterceptor to allow testing. */
@NullMarked
public class HttpsUpgradesInterceptor {
    public static void setHttpsPortForTesting(int port) {
        HttpsUpgradesInterceptorJni.get().setHttpsPortForTesting(port);
    }

    public static void setHttpPortForTesting(int port) {
        HttpsUpgradesInterceptorJni.get().setHttpPortForTesting(port);
    }

    @NativeMethods
    public interface Natives {
        void setHttpsPortForTesting(int port);

        void setHttpPortForTesting(int port);
    }
}
