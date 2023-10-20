// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.library_loader;

import androidx.test.filters.SmallTest;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeLibraryLoadedStatus;
import org.jni_zero.NativeMethods;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.build.BuildConfig;

/** Tests for early JNI initialization. */
@RunWith(BaseJUnit4ClassRunner.class)
@JNINamespace("base")
@Batch(Batch.UNIT_TESTS)
public class EarlyNativeTest {
    private boolean mWasInitialized;
    private CallbackHelper mLoadStarted;
    private CallbackHelper mEnsureInitializedFinished;

    @Before
    public void setUp() {
        mWasInitialized = LibraryLoader.getInstance().isInitialized();
        LibraryLoader.getInstance().resetForTesting();
        mLoadStarted = new CallbackHelper();
        mEnsureInitializedFinished = new CallbackHelper();
    }

    @After
    public void tearDown() {
        // Restore the simulated library state (due to the resetForTesting() call).
        if (mWasInitialized) {
            LibraryLoader.getInstance().ensureInitialized();
        }
    }

    @NativeMethods
    interface Natives {
        boolean isCommandLineInitialized();

        boolean isProcessNameEmpty();
    }

    @Test
    @SmallTest
    public void testEnsureInitialized() {
        // Make sure the Native library isn't considered ready for general use.
        Assert.assertFalse(LibraryLoader.getInstance().isInitialized());

        LibraryLoader.getInstance().ensureInitialized();
        Assert.assertTrue(LibraryLoader.getInstance().isInitialized());

        // Test resetForTesting().
        LibraryLoader.getInstance().resetForTesting();
        Assert.assertFalse(LibraryLoader.getInstance().isInitialized());
        LibraryLoader.getInstance().ensureInitialized();
        Assert.assertTrue(LibraryLoader.getInstance().isInitialized());
    }

    @Test
    @SmallTest
    public void testNativeMethodsReadyAfterLibraryInitialized() {
        // Test is a no-op if DCHECK isn't on.
        if (!BuildConfig.ENABLE_ASSERTS) return;

        Assert.assertFalse(
                NativeLibraryLoadedStatus.getProviderForTesting().areNativeMethodsReady());

        LibraryLoader.getInstance().ensureInitialized();
        Assert.assertTrue(
                NativeLibraryLoadedStatus.getProviderForTesting().areNativeMethodsReady());
    }

    @Test
    @SmallTest
    public void testNativeMethodsNotReadyThrows() {
        // Test is a no-op if dcheck isn't on.
        if (!BuildConfig.ENABLE_ASSERTS) return;

        try {
            EarlyNativeTestJni.get().isCommandLineInitialized();
            Assert.fail("Using JNI before the library is loaded should throw an exception.");
        } catch (NativeLibraryLoadedStatus.NativeNotLoadedException e) {
        }
    }
}
