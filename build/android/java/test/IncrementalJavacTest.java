// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package test;

import static org.junit.Assert.assertEquals;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

/**
 * Checks that build picked up changes to
 * {@link NoSignatureChangeIncrementalJavacTestHelper#foo()}.
 */
@RunWith(BaseRobolectricTestRunner.class)
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
