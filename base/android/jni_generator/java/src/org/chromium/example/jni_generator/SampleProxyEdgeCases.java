// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.example.jni_generator;

import org.chromium.example.jni_generator.Boolean;

@SomeAnnotation("that contains class Foo ")
class SampleProxyEdgeCases {
    enum Integer {}

    @NativeMethods
    interface Natives {
        void foo__weirdly__escaped_name1();
        String[][] crazyTypes(int[] a, Object[][] b);
        void fooForTest();
        void fooForTesting();

        // Tests passing a nested class from another class in the same package.
        void addStructB(SampleForTests caller, SampleForTests.InnerStructB b);

        // Tests a java.lang class.
        void setStringBuilder(StringBuilder sb);

        // Tests name collisions with java.lang classes.
        void setBool(Boolean b, Integer i);
    }

    // Non-proxy natives in same file.
    native void nativeInstanceMethod(long nativeInstance);
    static native void nativeStaticMethod();
}
