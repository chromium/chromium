// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test;

import org.junit.rules.TestRule;

/**
 * An interface for TestRules that can be configured to automatically run set-up logic prior
 * to &#064;Before.
 *
 * TestRules that implement this interface should return a {@link SetUpStatement} from their {@link
 * TestRule#apply} method
 *
 * @param <T> TestRule type that implements this SetUpTestRule
 */
public interface SetUpTestRule<T extends TestRule> {
    /**
     * Set whether the TestRule should run setUp automatically.
     *
     * So TestRule can be declared in test like this:
     * <code>
     * &#064;Rule TestRule mRule = new MySetUpTestRule().shouldSetUp(true);
     * </code>
     *
     * @return itself to chain up the calls for convenience
     */
    T shouldSetUp(boolean runSetUp);

    /** Specify setUp action in this method */
    void setUp();
}
