// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test;

import org.junit.runner.Description;
import org.junit.runner.notification.RunListener;

import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.ServiceImpl;
import org.chromium.testing.local.JunitTestMain;

/**
 * Provides a way to know when test suites are started/finished that works with
 * ParameterizedRobolectricTestRunner.
 */
@ServiceImpl(JunitTestMain.ExtraRunListenerProvider.class)
public class BaseRobolectricTestListener extends RunListener
        implements JunitTestMain.ExtraRunListenerProvider {
    private Description mActiveSuite;

    @Override
    public RunListener provideRunListener() {
        ResettersForTesting.enable();
        return this;
    }

    @Override
    public void testSuiteStarted(Description description) throws Exception {
        if (description.getTestClass() != null) {
            assert mActiveSuite == null
                    : String.format("\nFound: %s\nPrevious: %s", description, mActiveSuite);
            mActiveSuite = description;
            ResettersForTesting.beforeClassHooksWillExecute();
        }
    }

    @Override
    public void testSuiteFinished(Description description) throws Exception {
        if (description.equals(mActiveSuite)) {
            mActiveSuite = null;
            ResettersForTesting.afterClassHooksDidExecute();
        }
    }

    // Cannot use testStarted because it is called before a tests's @BeforeClass
    // https://github.com/robolectric/robolectric/issues/8768
}
