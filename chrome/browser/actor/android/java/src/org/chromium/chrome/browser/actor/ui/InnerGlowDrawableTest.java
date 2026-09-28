// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor.ui;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.GraphicsMode;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.base.TestActivity;

/**
 * Tests for {@link InnerGlowDrawable}.
 *
 * <p>Uses {@link GraphicsMode.Mode#NATIVE} so Robolectric rasterizes the Skia {@code
 * BlurMaskFilter} and {@code Canvas.clipRect} onto the backing {@link Bitmap}. To export the
 * rendered bitmap to a PNG locally for visual inspection:
 *
 * <pre>{@code
 * try (var out = new java.io.FileOutputStream("/tmp/inner_glow.png")) {
 *     bitmap.compress(Bitmap.CompressFormat.PNG, 100, out);
 * }
 * }</pre>
 */
@RunWith(BaseRobolectricTestRunner.class)
@GraphicsMode(GraphicsMode.Mode.NATIVE)
public class InnerGlowDrawableTest {
    @Test
    public void testDrawClipsGlowToBounds() {
        Activity activity = Robolectric.buildActivity(TestActivity.class).setup().get();

        int size = 200;
        int inset = 40;
        Bitmap bitmap = Bitmap.createBitmap(size, size, Bitmap.Config.ARGB_8888);
        InnerGlowDrawable drawable = InnerGlowDrawable.createMainWebpageGlow(activity);
        drawable.setBounds(inset, inset, size - inset, size - inset);
        drawable.draw(new Canvas(bitmap));

        int mid = size / 2;
        // Pixels outside the bounds on all four sides must remain transparent.
        Assert.assertEquals(Color.TRANSPARENT, bitmap.getPixel(mid, inset - 1));
        Assert.assertEquals(Color.TRANSPARENT, bitmap.getPixel(mid, size - inset));
        Assert.assertEquals(Color.TRANSPARENT, bitmap.getPixel(inset - 1, mid));
        Assert.assertEquals(Color.TRANSPARENT, bitmap.getPixel(size - inset, mid));

        // Pixels just inside the bounds on all four sides must have non-zero glow alpha.
        Assert.assertTrue(Color.alpha(bitmap.getPixel(mid, inset + 2)) > 0);
        Assert.assertTrue(Color.alpha(bitmap.getPixel(mid, size - inset - 2)) > 0);
        Assert.assertTrue(Color.alpha(bitmap.getPixel(inset + 2, mid)) > 0);
        Assert.assertTrue(Color.alpha(bitmap.getPixel(size - inset - 2, mid)) > 0);
    }
}
