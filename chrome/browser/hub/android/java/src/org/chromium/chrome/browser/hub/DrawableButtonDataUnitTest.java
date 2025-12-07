// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link DrawableButtonData}. */
@RunWith(BaseRobolectricTestRunner.class)
public class DrawableButtonDataUnitTest {
    @Test
    @SmallTest
    public void testResolveTextAndIconAndContentDescription() {
        Context context = ApplicationProvider.getApplicationContext();
        Drawable drawable = createBitmapDrawable();
        DisplayButtonData buttonData =
                new DrawableButtonData(
                        R.string.button_new_tab, R.string.button_new_incognito_tab, drawable);
        assertNotEquals(0, buttonData.resolveText(context).length());
        assertNotEquals(0, buttonData.resolveContentDescription(context).length());
        assertEquals(drawable, buttonData.resolveIcon(context));
    }

    @Test
    @SmallTest
    public void testHashCode() {
        Drawable drawable = createBitmapDrawable();
        DisplayButtonData buttonData1 =
                new DrawableButtonData(
                        R.string.button_new_tab, R.string.button_new_incognito_tab, drawable);
        DisplayButtonData buttonData2 =
                new DrawableButtonData(
                        R.string.button_new_tab, R.string.button_new_incognito_tab, drawable);
        // Only test positive case, since we're not guaranteed to get different hash codes for
        // different values.
        assertEquals(buttonData1.hashCode(), buttonData2.hashCode());
    }

    @Test
    @SmallTest
    public void testEquals() {
        Drawable drawable = createBitmapDrawable();
        DisplayButtonData buttonData =
                new DrawableButtonData(
                        R.string.button_new_tab, R.string.button_new_incognito_tab, drawable);
        assertEquals(
                buttonData,
                new DrawableButtonData(
                        R.string.button_new_tab, R.string.button_new_incognito_tab, drawable));
        assertNotEquals(
                buttonData,
                new DrawableButtonData(
                        R.string.button_new_incognito_tab,
                        R.string.button_new_incognito_tab,
                        drawable));
        Drawable newDrawable = createBitmapDrawable();
        assertNotEquals(
                buttonData,
                new DrawableButtonData(
                        R.string.button_new_tab, R.string.button_new_incognito_tab, newDrawable));
        assertNotEquals(
                buttonData,
                new DrawableButtonData(R.string.button_new_tab, R.string.button_new_tab, drawable));

        // assert*Equals will not invoke #equals on a null object, manually call it instead.
        assertFalse(buttonData.equals(null));

        assertNotEquals(buttonData, new Object());
    }

    private Drawable createBitmapDrawable() {
        Bitmap image = Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888);
        Resources resources = ApplicationProvider.getApplicationContext().getResources();
        return new BitmapDrawable(resources, image);
    }
}
