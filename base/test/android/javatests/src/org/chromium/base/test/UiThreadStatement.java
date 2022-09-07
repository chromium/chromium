// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test;

import android.support.test.InstrumentationRegistry;

import org.junit.runners.model.FrameworkMethod;
import org.junit.runners.model.Statement;

import java.util.concurrent.atomic.AtomicReference;

/**
 * junit {@link Statement} that runs a test method on the UI thread.
 */
/* package */ class UiThreadStatement extends Statement {
    private final Statement mBase;

    public UiThreadStatement(Statement base) {
        mBase = base;
    }

    @Override
    public void evaluate() throws Throwable {
        final AtomicReference<Throwable> exceptionRef = new AtomicReference<>();
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            try {
                mBase.evaluate();
            } catch (Throwable throwable) {
                exceptionRef.set(throwable);
            }
        });
        Throwable throwable = exceptionRef.get();
        if (throwable != null) throw throwable;
    }

    /**
     * @return True if the method is annotated with {@link UiThreadTest}.
     */
    public static boolean shouldRunOnUiThread(FrameworkMethod method) {
        return method.getAnnotation(UiThreadTest.class) != null;
    }
}
