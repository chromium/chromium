// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.NullMarked;

/** Unit tests for {@link TriStateUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@NullMarked
public class TriStateUtilsTest {
    @Test
    public void testFromBoolean() {
        assertEquals(TriState.TRUE, TriStateUtils.from(true));
        assertEquals(TriState.FALSE, TriStateUtils.from(false));
    }

    @Test
    public void testFromNullableBoolean() {
        assertEquals(TriState.NOT_SET, TriStateUtils.from((Boolean) null));
        assertEquals(TriState.TRUE, TriStateUtils.from(Boolean.TRUE));
        assertEquals(TriState.FALSE, TriStateUtils.from(Boolean.FALSE));
    }

    @Test
    public void testToNullableBoolean() {
        assertNull(TriStateUtils.toNullableBoolean(TriState.NOT_SET));
        assertEquals(true, TriStateUtils.toNullableBoolean(TriState.TRUE));
        assertEquals(false, TriStateUtils.toNullableBoolean(TriState.FALSE));
    }
}
