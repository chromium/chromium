// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.util;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import static org.chromium.base.GarbageCollectionTestUtils.canBeGarbageCollected;

import android.graphics.Bitmap;

import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.GarbageCollectionTestUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.UiThreadTest;

import java.lang.ref.WeakReference;

/**
 * Tests for {@link GarbageCollectionTestUtils}.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class GarbageCollectionTestUtilsTest {
    @Test
    @SmallTest
    @UiThreadTest
    public void testCanBeGarbageCollected() {
        Bitmap bitmap = Bitmap.createBitmap(1, 2, Bitmap.Config.ARGB_8888);
        WeakReference<Bitmap> bitmapWeakReference = new WeakReference<>(bitmap);
        assertNotNull(bitmapWeakReference.get());
        assertFalse(canBeGarbageCollected(bitmapWeakReference));

        bitmap = null;
        assertTrue(canBeGarbageCollected(bitmapWeakReference));
    }
}
