// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util;

import org.junit.runners.model.FrameworkMethod;

/** Check whether a test case should be skipped. */
public abstract class SkipCheck {
    /**
     * Checks whether the given test method should be skipped.
     *
     * @param testMethod The test method to check.
     * @return Whether the test case should be skipped.
     */
    public abstract boolean shouldSkip(FrameworkMethod testMethod);
}
