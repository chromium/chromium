// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

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

    @After
    public void tearDown() {
        JniCallbacksTestJni.get().resetCounters();
    }

    @Test
    public void testNativeToOnceClosure() {
        Runnable r = JniCallbacksTestJni.get().getOnceClosure();
        r.run();
        Assert.assertEquals(1, JniCallbacksTestJni.get().getOnceClosureRunCount());
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
        Assert.assertEquals(2, JniCallbacksTestJni.get().getRepeatingClosureRunCount());
        r.destroy();
    }

    @Test
    public void testNativeToRepeatingCallback() {
        JniRepeatingCallback<Boolean> c = JniCallbacksTestJni.get().getRepeatingCallback();
        c.onResult(true);
        c.onResult(false);
        Assert.assertEquals(2, JniCallbacksTestJni.get().getRepeatingCallbackResultCount());
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

    @NativeMethods
    interface Natives {
        void resetCounters();

        Runnable getOnceClosure();

        int getOnceClosureRunCount();

        Callback<Boolean> getOnceCallback();

        boolean getOnceCallbackResult();

        JniRepeatingRunnable getRepeatingClosure();

        int getRepeatingClosureRunCount();

        JniRepeatingCallback<Boolean> getRepeatingCallback();

        int getRepeatingCallbackResultCount();

        void passOnceClosure(@JniType("base::OnceClosure") Runnable r);

        void passOnceCallback(@JniType("base::OnceCallback<void(int32_t)>") Callback<Integer> c);

        void passRepeatingClosure(@JniType("base::RepeatingClosure") Runnable r);

        void passRepeatingCallback(
                @JniType("base::RepeatingCallback<void(int32_t)>") Callback<Integer> c);
    }
}
