// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.example.jni_generator;

class SampleProxyEdgeCases {
    @NativeMethods
    interface Natives {
        void foo__weirdly__escaped_name1();
        String[][] crazyTypes(int[] a, Object[][] b);
        void fooForTest();
        void fooForTesting();
    }

    // Non-proxy natives in same file.
    native void nativeInstanceMethod(long nativeInstance);
    static native void nativeStaticMethod();
}
