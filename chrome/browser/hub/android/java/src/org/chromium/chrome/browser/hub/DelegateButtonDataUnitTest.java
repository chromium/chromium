// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link DelegateButtonData}. */
@RunWith(BaseRobolectricTestRunner.class)
public class DelegateButtonDataUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private DisplayButtonData mDisplayButtonData;
    @Mock private Runnable mRunnable;
    @Mock private Drawable mExpectedDrawable;

    @Test
    @SmallTest
    public void testFocusChangesPane() {
        Context context = ApplicationProvider.getApplicationContext();
        String expectedText = "foo";
        String expectedContentDescription = "bar";
        when(mDisplayButtonData.resolveText(context)).thenReturn(expectedText);
        when(mDisplayButtonData.resolveContentDescription(context))
                .thenReturn(expectedContentDescription);
        when(mDisplayButtonData.resolveIcon(context)).thenReturn(mExpectedDrawable);
        FullButtonData buttonData = new DelegateButtonData(mDisplayButtonData, mRunnable);

        assertEquals(expectedText, buttonData.resolveText(context));
        assertEquals(expectedContentDescription, buttonData.resolveContentDescription(context));
        assertEquals(mExpectedDrawable, buttonData.resolveIcon(context));
        assertEquals(mRunnable, buttonData.getOnPressRunnable());
    }

    @Test
    @SmallTest
    public void testButtonDataEquals() {
        Drawable drawable = createBitmapDrawable();
        DisplayButtonData displayButtonData1 =
                new DrawableButtonData(
                        R.string.button_new_tab, R.string.button_new_incognito_tab, drawable);
        DisplayButtonData displayButtonData2 =
                new DrawableButtonData(
                        R.string.button_new_tab, R.string.button_new_incognito_tab, drawable);
        DisplayButtonData differentDisplayButtonData =
                new DrawableButtonData(
                        R.string.button_new_incognito_tab, R.string.button_new_tab, drawable);

        Runnable runnable1 = () -> {};
        Runnable runnable2 = () -> {};

        DelegateButtonData buttonData = new DelegateButtonData(displayButtonData1, runnable1);

        // Test button data equality with same DisplayButtonData (different Runnable should not
        // affect equality)
        assertTrue(
                buttonData.buttonDataEquals(new DelegateButtonData(displayButtonData2, runnable2)));
        assertTrue(buttonData.buttonDataEquals(new DelegateButtonData(displayButtonData1, null)));

        // Additional checks to ensure object inequality is correctly identified
        assertNotEquals(buttonData, new DelegateButtonData(displayButtonData1, runnable2));
        assertNotEquals(buttonData, new DelegateButtonData(displayButtonData1, null));

        // Test inequality with different DisplayButtonData
        assertFalse(
                buttonData.buttonDataEquals(
                        new DelegateButtonData(differentDisplayButtonData, runnable1)));

        // Test inequality with null
        assertFalse(buttonData.buttonDataEquals(null));

        // Test inequality with different object type
        assertFalse(buttonData.buttonDataEquals(new Object()));
    }

    private Drawable createBitmapDrawable() {
        Bitmap image = Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888);
        Resources resources = ApplicationProvider.getApplicationContext().getResources();
        return new BitmapDrawable(resources, image);
    }
}
