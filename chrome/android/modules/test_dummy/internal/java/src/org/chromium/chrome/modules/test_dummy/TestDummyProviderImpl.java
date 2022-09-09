// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.modules.test_dummy;

import org.chromium.chrome.browser.test_dummy.TestDummy;
import org.chromium.chrome.browser.test_dummy.TestDummyImpl;

/** Provides the test dummy implementation inside the test dummy module. */
public class TestDummyProviderImpl implements TestDummyProvider {
    private final TestDummyImpl mTestDummy = new TestDummyImpl();

    @Override
    public TestDummy getTestDummy() {
        return mTestDummy;
    }
}
