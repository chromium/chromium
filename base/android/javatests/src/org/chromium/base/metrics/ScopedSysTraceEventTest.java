// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.metrics;

import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;

@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public final class ScopedSysTraceEventTest {
    @SmallTest
    @Test
    public void testShortSectionName() {
        try (var traceEvent =
                ScopedSysTraceEvent.scoped("A".repeat(ScopedSysTraceEvent.MAX_SECTION_NAME_LEN))) {}
    }

    @SmallTest
    @Test
    public void testLongSectionName() {
        try (var traceEvent =
                ScopedSysTraceEvent.scoped(
                        "A".repeat(ScopedSysTraceEvent.MAX_SECTION_NAME_LEN + 1))) {}
    }
}
