// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;
import org.junit.After;
import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Robolectric tests for JNI callbacks. */
@JNINamespace("base::android")
@RunWith(BaseRobolectricTestRunner.class)
public class JniCallbacksTest {
    private static int sRunCount;

    @After
    public void tearDown() {
        JniCallbacksTestJni.get().resetCounters();
        sRunCount = 0;
    }

    @CalledByNative
    static void callOnceClosure(@JniType("base::OnceClosure&&") Runnable r) {
        sRunCount++;
        r.run();
    }

    @CalledByNative
    static void callOnceCallback(
            @JniType("base::OnceCallback<void(int32_t)>&&") Callback<Integer> c) {
        sRunCount++;
        c.onResult(42);
    }

    @CalledByNative
    static void callOnceCallback2(
            @JniType("base::OnceCallback<void(bool, int32_t)>&&") Callback2<Boolean, Integer> c) {
        sRunCount++;
        c.onResult(true, 100);
    }

    @CalledByNative
    static void callRepeatingClosure(@JniType("base::RepeatingClosure") JniRepeatingRunnable r) {
        sRunCount++;
        r.run();
        r.run();
        r.destroy();
    }

    @CalledByNative
    static void callRepeatingClosureMoveOnly(
            @JniType("base::RepeatingClosure&&") JniRepeatingRunnable r) {
        sRunCount++;
        r.run();
        r.run();
        r.destroy();
    }

    @CalledByNative
    static void callRepeatingCallback(
            @JniType("base::RepeatingCallback<void(int32_t)>") JniRepeatingCallback<Integer> c) {
        sRunCount++;
        c.onResult(1);
        c.onResult(2);
        c.destroy();
    }

    @CalledByNative
    static void callRepeatingCallback2(
            @JniType("base::RepeatingCallback<void(bool, int32_t)>")
                    JniRepeatingCallback2<Boolean, Integer> c) {
        sRunCount++;
        c.onResult(true, 1);
        c.onResult(false, 2);
        c.destroy();
    }

    @Test
    public void testCalledByNativeOnceClosure() {
        JniCallbacksTestJni.get().triggerOnceClosure();
        Assert.assertEquals(1, sRunCount);
        Assert.assertEquals(1, JniCallbacksTestJni.get().getRunCount());
    }

    @Test
    public void testCalledByNativeOnceCallback() {
        JniCallbacksTestJni.get().triggerOnceCallback();
        Assert.assertEquals(1, sRunCount);
        Assert.assertEquals(42, JniCallbacksTestJni.get().getOnceCallback2Result2());
    }

    @Test
    public void testCalledByNativeOnceCallback2() {
        JniCallbacksTestJni.get().triggerOnceCallback2();
        Assert.assertEquals(1, sRunCount);
        Assert.assertTrue(JniCallbacksTestJni.get().getOnceCallback2Result1());
        Assert.assertEquals(100, JniCallbacksTestJni.get().getOnceCallback2Result2());
    }

    @Test
    public void testCalledByNativeRepeatingClosure() {
        JniCallbacksTestJni.get().triggerRepeatingClosure();
        Assert.assertEquals(1, sRunCount);
        Assert.assertEquals(2, JniCallbacksTestJni.get().getRunCount());
    }

    @Test
    public void testCalledByNativeRepeatingCallback() {
        JniCallbacksTestJni.get().triggerRepeatingCallback();
        Assert.assertEquals(1, sRunCount);
        Assert.assertEquals(2, JniCallbacksTestJni.get().getRunCount());
    }

    @Test
    public void testCalledByNativeRepeatingCallback2() {
        JniCallbacksTestJni.get().triggerRepeatingCallback2();
        Assert.assertEquals(1, sRunCount);
        Assert.assertEquals(2, JniCallbacksTestJni.get().getRunCount());
    }

    @Test
    public void testNativeToOnceClosure() {
        Runnable r = JniCallbacksTestJni.get().getOnceClosure();
        r.run();
        Assert.assertEquals(1, JniCallbacksTestJni.get().getRunCount());
    }

    @Test
    public void testNativeToOnceCallback() {
        Callback<Boolean> c = JniCallbacksTestJni.get().getOnceCallback();
        c.onResult(true);
        Assert.assertTrue(JniCallbacksTestJni.get().getOnceCallbackResult());
    }

    @Test
    public void testNativeToRepeatingClosure() {
        JniRepeatingRunnable r = JniCallbacksTestJni.get().getRepeatingClosure();
        r.run();
        r.run();
        Assert.assertEquals(2, JniCallbacksTestJni.get().getRunCount());
        r.destroy();
    }

    @Test
    public void testNativeToRepeatingCallback() {
        JniRepeatingCallback<Boolean> c = JniCallbacksTestJni.get().getRepeatingCallback();
        c.onResult(true);
        c.onResult(false);
        Assert.assertEquals(2, JniCallbacksTestJni.get().getRunCount());
        c.destroy();
    }

    @Test
    public void testNativeToOnceCallback2() {
        Callback2<Boolean, Integer> c = JniCallbacksTestJni.get().getOnceCallback2();
        c.onResult(true, 100);
        Assert.assertTrue(JniCallbacksTestJni.get().getOnceCallback2Result1());
        Assert.assertEquals(100, JniCallbacksTestJni.get().getOnceCallback2Result2());
    }

    @Test
    public void testNativeToRepeatingCallback2() {
        JniRepeatingCallback2<Boolean, Integer> c =
                JniCallbacksTestJni.get().getRepeatingCallback2();
        c.onResult(true, 1);
        c.onResult(false, 2);
        Assert.assertEquals(2, JniCallbacksTestJni.get().getRunCount());
        c.destroy();
    }

    @Test
    public void testJavaToOnceClosure() {
        boolean[] run = {false};
        JniCallbacksTestJni.get().passOnceClosure(() -> run[0] = true);
        Assert.assertTrue(run[0]);
    }

    @Test
    public void testJavaToOnceCallback() {
        int[] result = {-1};
        JniCallbacksTestJni.get().passOnceCallback(r -> result[0] = r);
        Assert.assertEquals(42, result[0]);
    }

    @Test
    public void testJavaToRepeatingClosure() {
        int[] runCount = {0};
        JniCallbacksTestJni.get().passRepeatingClosure(() -> runCount[0]++);
        Assert.assertEquals(2, runCount[0]);
    }

    @Test
    public void testJavaToRepeatingCallback() {
        int[] resultCount = {0};
        JniCallbacksTestJni.get().passRepeatingCallback(r -> resultCount[0]++);
        Assert.assertEquals(2, resultCount[0]);
    }

    @Test
    public void testJavaToOnceCallback2() {
        boolean[] result1 = {false};
        int[] result2 = {0};
        JniCallbacksTestJni.get()
                .passOnceCallback2(
                        (r1, r2) -> {
                            result1[0] = r1;
                            result2[0] = r2;
                        });
        Assert.assertTrue(result1[0]);
        Assert.assertEquals(100, result2[0]);
    }

    @Test
    public void testJavaToRepeatingCallback2() {
        int[] resultCount = {0};
        JniCallbacksTestJni.get().passRepeatingCallback2((r1, r2) -> resultCount[0]++);
        Assert.assertEquals(2, resultCount[0]);
    }

    @Test
    public void testJavaToOnceCallbackWithSubtype() {
        String[] result = {null};
        JniCallbacksTestJni.get().passOnceCallbackWithSubtype(r -> result[0] = r);
        Assert.assertEquals("test string", result[0]);
    }

    @Test
    public void testJavaToOnceCallbackWithScopedSubtype() {
        String[] result = {null};
        JniCallbacksTestJni.get().passOnceCallbackWithScopedSubtype(r -> result[0] = r);
        Assert.assertEquals("scoped string", result[0]);
    }

    @Test
    public void testJavaToRepeatingCallbackWithSubtype() {
        String[] results = new String[2];
        int[] resultCount = {0};
        JniCallbacksTestJni.get()
                .passRepeatingCallbackWithSubtype(
                        r -> {
                            results[resultCount[0]] = r;
                            resultCount[0]++;
                        });
        Assert.assertEquals(2, resultCount[0]);
        Assert.assertEquals("s1", results[0]);
        Assert.assertEquals("s2", results[1]);
    }

    @Test
    public void testNativeToOnceCallbackWithSubtype() {
        Callback<String> c = JniCallbacksTestJni.get().getOnceCallbackWithSubtype();
        c.onResult("native subtype");
        Assert.assertEquals(
                "native subtype", JniCallbacksTestJni.get().getOnceCallbackWithSubtypeResult());
    }

    @Test
    public void testNativeToRepeatingCallbackWithSubtype() {
        JniRepeatingCallback<String> c =
                JniCallbacksTestJni.get().getRepeatingCallbackWithSubtype();
        c.onResult("s1");
        c.onResult("s2");
        Assert.assertEquals(2, JniCallbacksTestJni.get().getRunCount());
        c.destroy();
    }

    @Test
    public void testPassReturnedOnceCallbackWithSubtype() {
        Callback<String> c = JniCallbacksTestJni.get().getOnceCallbackWithSubtype();
        JniCallbacksTestJni.get().passOnceCallbackWithSubtype(c);
        Assert.assertEquals(
                "test string", JniCallbacksTestJni.get().getOnceCallbackWithSubtypeResult());
    }

    @NativeMethods
    interface Natives {
        void resetCounters();

        void triggerOnceClosure();

        void triggerOnceCallback();

        void triggerOnceCallback2();

        void triggerRepeatingClosure();

        void triggerRepeatingClosureMoveOnly();

        void triggerRepeatingCallback();

        void triggerRepeatingCallback2();

        @JniType("base::OnceClosure")
        Runnable getOnceClosure();

        int getRunCount();

        @JniType("base::OnceCallback<void(bool)>")
        Callback<Boolean> getOnceCallback();

        boolean getOnceCallbackResult();

        @JniType("base::RepeatingClosure")
        JniRepeatingRunnable getRepeatingClosure();

        @JniType("base::RepeatingCallback<void(bool)>")
        JniRepeatingCallback<Boolean> getRepeatingCallback();

        @JniType("base::OnceCallback<void(bool, jni_zero::JavaRef<jobject>)>")
        Callback2<Boolean, Integer> getOnceCallback2();

        boolean getOnceCallback2Result1();

        int getOnceCallback2Result2();

        @JniType("base::RepeatingCallback<void(bool, int32_t)>")
        JniRepeatingCallback2<Boolean, Integer> getRepeatingCallback2();

        @JniType("base::OnceCallback<void(const jni_zero::JavaRef<jstring>&)>")
        Callback<String> getOnceCallbackWithSubtype();

        String getOnceCallbackWithSubtypeResult();

        @JniType("base::RepeatingCallback<void(const jni_zero::JavaRef<jstring>&)>")
        JniRepeatingCallback<String> getRepeatingCallbackWithSubtype();

        void passOnceClosure(@JniType("base::OnceClosure") Runnable r);

        void passOnceCallback(@JniType("base::OnceCallback<void(int32_t)>") Callback<Integer> c);

        void passRepeatingClosure(@JniType("base::RepeatingClosure") Runnable r);

        void passRepeatingCallback(
                @JniType("base::RepeatingCallback<void(int32_t)>") Callback<Integer> c);

        void passOnceCallback2(
                @JniType("base::OnceCallback<void(bool, int32_t)>") Callback2<Boolean, Integer> c);

        void passRepeatingCallback2(
                @JniType("base::RepeatingCallback<void(bool, int32_t)>")
                        Callback2<Boolean, Integer> c);

        void passOnceCallbackWithSubtype(
                @JniType("base::OnceCallback<void(const jni_zero::JavaRef<jstring>&)>")
                        Callback<String> c);

        void passOnceCallbackWithScopedSubtype(
                @JniType("base::OnceCallback<void(jni_zero::ScopedJavaLocalRef<jstring>)>")
                        Callback<String> c);

        void passRepeatingCallbackWithSubtype(
                @JniType("base::RepeatingCallback<void(const jni_zero::JavaRef<jstring>&)>")
                        Callback<String> c);
    }
}
