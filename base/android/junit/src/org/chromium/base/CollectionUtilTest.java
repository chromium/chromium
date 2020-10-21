// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import static org.junit.Assert.assertEquals;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.Arrays;

/** Unit tests for {@link Log}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class CollectionUtilTest {
    /** Tests that the computed call origin is the correct one. */
    @Test
    public void testStrengthen() {
        // Java never GC's small constants, so there's no risk of the weak refs becoming null.
        ArrayList<WeakReference<Integer>> weakList = new ArrayList<>();
        weakList.add(new WeakReference<>(0));
        weakList.add(new WeakReference<>(1));
        weakList.add(new WeakReference<>(2));

        assertEquals(Arrays.asList(0, 1, 2), CollectionUtil.strengthen(weakList));

        weakList.set(1, new WeakReference<>(null));
        assertEquals(Arrays.asList(0, 2), CollectionUtil.strengthen(weakList));
    }
}
