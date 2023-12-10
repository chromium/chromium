// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.util;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import static org.chromium.base.GarbageCollectionTestUtils.canBeGarbageCollected;

import android.graphics.Bitmap;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.GarbageCollectionTestUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;

import java.lang.ref.WeakReference;

/** Tests for {@link GarbageCollectionTestUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class GarbageCollectionTestUtilsUnitTest {
    @Test
    public void testCanBeGarbageCollected() {
        Bitmap bitmap = Bitmap.createBitmap(1, 2, Bitmap.Config.ARGB_8888);
        WeakReference<Bitmap> bitmapWeakReference = new WeakReference<>(bitmap);
        assertNotNull(bitmapWeakReference.get());
        assertFalse(canBeGarbageCollected(bitmapWeakReference));

        bitmap = null;
        assertTrue(canBeGarbageCollected(bitmapWeakReference));
    }
}
