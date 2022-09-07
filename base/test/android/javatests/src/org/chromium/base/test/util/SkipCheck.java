// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util;

import junit.framework.TestCase;

import org.junit.runners.model.FrameworkMethod;

import org.chromium.base.Log;

import java.lang.reflect.Method;

/**
 * Check whether a test case should be skipped.
 */
public abstract class SkipCheck {

    private static final String TAG = "base_test";

    /**
     *
     * Checks whether the given test method should be skipped.
     *
     * @param testMethod The test method to check.
     * @return Whether the test case should be skipped.
     */
    public abstract boolean shouldSkip(FrameworkMethod testMethod);

    /**
     *
     * Checks whether the given test case should be skipped.
     *
     * @param testCase The test case to check.
     * @return Whether the test case should be skipped.
     */
    public boolean shouldSkip(TestCase testCase) {
        try {
            Method m = testCase.getClass().getMethod(testCase.getName(), (Class[]) null);
            return shouldSkip(new FrameworkMethod(m));
        } catch (NoSuchMethodException e) {
            Log.e(TAG, "Unable to find %s in %s", testCase.getName(),
                    testCase.getClass().getName(), e);
            return false;
        }
    }
}

