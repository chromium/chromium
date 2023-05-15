// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.example.jni_generator;

class SampleBidirectionalNonProxy {
    /** The pointer to the native Test. */
    public long nativeTest;
    private static native int nativeStaticMethod(long nativeTest, int arg1);
    private native int nativeMethod(long nativeTest, int arg1);
    @CalledByNative
    private void testMethodWithParam(int iParam);
    @CalledByNative
    private String testMethodWithParamAndReturn(int iParam);
    @CalledByNative
    private static int testStaticMethodWithParam(int iParam);
    @CalledByNative
    private static double testMethodWithNoParam();
    @CalledByNative
    private static String testStaticMethodWithNoParam();

    class MyInnerClass {
        @NativeCall("MyInnerClass")
        private native int nativeInit();
    }
    class MyOtherInnerClass {
        @NativeCall("MyOtherInnerClass")
        private native int nativeInit();
    }
}
