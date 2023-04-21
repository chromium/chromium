// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.build;

import static org.junit.Assert.assertEquals;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.Config;

import test.NoSignatureChangeIncrementalJavacTestHelper;

/**
 * Checks that build picked up changes to
 * {@link NoSignatureChangeIncrementalJavacTestHelper#foo()}.
 */
@RunWith(RobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public final class IncrementalJavacTest {
    @Test
    public void testNoSignatureChange() {
        NoSignatureChangeIncrementalJavacTestHelper helper =
                new NoSignatureChangeIncrementalJavacTestHelper();
        // #foo() should return updated value.
        assertEquals("foo2", helper.foo());

        // #bar() should not crash.
        assertEquals("bar", helper.bar());
    }
}
