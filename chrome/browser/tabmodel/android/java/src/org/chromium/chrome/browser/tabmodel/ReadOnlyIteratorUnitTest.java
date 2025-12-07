// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.Iterator;
import java.util.List;

/** Unit tests for {@link ReadOnlyIterator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ReadOnlyIteratorUnitTest {
    @Test(expected = UnsupportedOperationException.class)
    public void testMakeIterableReadOnly() {
        List<Integer> list = List.of();
        Iterator<Integer> readOnlyIterator = ReadOnlyIterator.maybeCreate(list.iterator());
        readOnlyIterator.remove();
    }
}
