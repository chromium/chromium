// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test;

import android.content.Context;

import org.junit.runners.model.FrameworkMethod;

import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.test.BaseJUnit4ClassRunner.TestHook;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.RequiresRestart;

/**
 * PreTestHook used to ensure we don't start the browser process in unit tests.
 * */
public final class UnitTestNoBrowserProcessHook implements TestHook {
    @Override
    public void run(Context targetContext, FrameworkMethod testMethod) {
        Batch annotation = testMethod.getDeclaringClass().getAnnotation(Batch.class);
        if (annotation != null && annotation.value().equals(Batch.UNIT_TESTS)) {
            if (testMethod.getAnnotation(RequiresRestart.class) != null) return;
            LibraryLoader.setBrowserProcessStartupBlockedForTesting();
        }
    }
}
