// Copyright 2020 The Chromium Authors
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
        Object o1 = new Object();
        Object o2 = new Object();
        Object o3 = new Object();
        ArrayList<WeakReference<Object>> weakList = new ArrayList<>();
        weakList.add(new WeakReference<>(o1));
        weakList.add(new WeakReference<>(o2));
        weakList.add(new WeakReference<>(o3));

        assertEquals(Arrays.asList(o1, o2, o3), CollectionUtil.strengthen(weakList));

        weakList.set(1, new WeakReference<>(null));
        assertEquals(Arrays.asList(o1, o3), CollectionUtil.strengthen(weakList));
    }
}
