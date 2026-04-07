// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.actions.button;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.view.View;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ui.actions.R;

/** Unit tests for {@link DelegateButtonData}. */
@RunWith(BaseRobolectricTestRunner.class)
public class DelegateButtonDataUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private DisplayButtonData mDisplayButtonData;
    @Mock private Callback<View> mCallback;
    @Mock private Callback<View> mOnLongPressCallback;
    @Mock private Drawable mExpectedDrawable;

    @Test
    @SmallTest
    public void testDelegateButtonData_withCallbacks() {
        Context context = ApplicationProvider.getApplicationContext();
        String expectedText = "foo";
        String expectedContentDescription = "bar";
        when(mDisplayButtonData.resolveText(context)).thenReturn(expectedText);
        when(mDisplayButtonData.resolveContentDescription(context))
                .thenReturn(expectedContentDescription);
        when(mDisplayButtonData.resolveIcon(context)).thenReturn(mExpectedDrawable);
        FullButtonData buttonData =
                new DelegateButtonData.Builder(mDisplayButtonData)
                        .setOnPress(mCallback)
                        .setOnLongPress(mOnLongPressCallback)
                        .build();

        assertEquals(expectedText, buttonData.resolveText(context));
        assertEquals(expectedContentDescription, buttonData.resolveContentDescription(context));
        assertEquals(mExpectedDrawable, buttonData.resolveIcon(context));
        assertEquals(mCallback, buttonData.getOnPress());
        assertEquals(mOnLongPressCallback, buttonData.getOnLongPress());
    }

    @Test
    @SmallTest
    public void testDelegateButtonData_noLongPressCallback() {
        FullButtonData buttonData =
                new DelegateButtonData.Builder(mDisplayButtonData).setOnPress(mCallback).build();

        assertEquals(mCallback, buttonData.getOnPress());
        assertNull(buttonData.getOnLongPress());
    }

    @Test
    @SmallTest
    public void testButtonDataEquals_sameContentDifferentCallbacks() {
        Drawable drawable = createBitmapDrawable();
        DisplayButtonData displayData1 =
                new DrawableButtonData(
                        R.string.button_new_tab, R.string.button_new_incognito_tab, drawable);
        DisplayButtonData displayData2 =
                new DrawableButtonData(
                        R.string.button_new_tab, R.string.button_new_incognito_tab, drawable);

        Callback<View> callback1 = view -> {};
        Callback<View> callback2 = view -> {};
        Callback<View> longCallback1 = view -> {};
        Callback<View> longCallback2 = view -> {};

        DelegateButtonData buttonData =
                new DelegateButtonData.Builder(displayData1)
                        .setOnPress(callback1)
                        .setOnLongPress(longCallback1)
                        .build();

        assertTrue(
                buttonData.buttonDataEquals(
                        new DelegateButtonData.Builder(displayData2)
                                .setOnPress(callback2)
                                .setOnLongPress(longCallback2)
                                .build()));
        assertTrue(
                buttonData.buttonDataEquals(
                        new DelegateButtonData.Builder(displayData2)
                                .setOnPress(callback2)
                                .setOnLongPress(longCallback1)
                                .build()));
        assertTrue(
                buttonData.buttonDataEquals(
                        new DelegateButtonData.Builder(displayData2)
                                .setOnPress(callback1)
                                .setOnLongPress(longCallback2)
                                .build()));
        assertFalse(
                buttonData.buttonDataEquals(new DelegateButtonData.Builder(displayData1).build()));
    }

    @Test
    @SmallTest
    public void testButtonDataEquals_differentDisplayData() {
        Drawable drawable = createBitmapDrawable();
        DisplayButtonData displayData =
                new DrawableButtonData(
                        R.string.button_new_tab, R.string.button_new_incognito_tab, drawable);
        DisplayButtonData differentDisplayData =
                new DrawableButtonData(
                        R.string.button_new_incognito_tab, R.string.button_new_tab, drawable);

        DelegateButtonData buttonData = new DelegateButtonData.Builder(displayData).build();
        DelegateButtonData differentButtonData =
                new DelegateButtonData.Builder(differentDisplayData).build();

        assertFalse(buttonData.buttonDataEquals(differentButtonData));
    }

    @Test
    @SmallTest
    public void testButtonDataEquals_differentIsEnabled() {
        Drawable drawable = createBitmapDrawable();
        DisplayButtonData displayData =
                new DrawableButtonData(
                        R.string.button_new_tab, R.string.button_new_incognito_tab, drawable);

        DelegateButtonData buttonData = new DelegateButtonData.Builder(displayData).build();
        DelegateButtonData enabledButtonData =
                new DelegateButtonData.Builder(displayData).setOnPress(mCallback).build();

        assertFalse(buttonData.buttonDataEquals(enabledButtonData));
    }

    @Test
    @SmallTest
    public void testButtonDataEquals_differentToggledState() {
        Drawable drawable = createBitmapDrawable();
        DisplayButtonData displayData =
                new DrawableButtonData(
                        R.string.button_new_tab, R.string.button_new_incognito_tab, drawable);

        DelegateButtonData buttonData = new DelegateButtonData.Builder(displayData).build();
        DelegateButtonData toggledButtonData =
                new DelegateButtonData.Builder(displayData).setIsToggled(true).build();

        assertFalse(buttonData.buttonDataEquals(toggledButtonData));
    }

    @Test
    @SmallTest
    public void testButtonDataEquals_nullObject() {
        Drawable drawable = createBitmapDrawable();
        DisplayButtonData displayData =
                new DrawableButtonData(
                        R.string.button_new_tab, R.string.button_new_incognito_tab, drawable);

        DelegateButtonData buttonData = new DelegateButtonData.Builder(displayData).build();

        assertFalse(buttonData.buttonDataEquals(null));
    }

    @Test
    @SmallTest
    public void testButtonDataEquals_differentObjectType() {
        Drawable drawable = createBitmapDrawable();
        DisplayButtonData displayData =
                new DrawableButtonData(
                        R.string.button_new_tab, R.string.button_new_incognito_tab, drawable);

        DelegateButtonData buttonData = new DelegateButtonData.Builder(displayData).build();

        assertFalse(buttonData.buttonDataEquals(new Object()));
    }

    @Test
    @SmallTest
    public void testButtonDataEquals_differentButtonState_sameIsEnabled() {
        Drawable drawable = createBitmapDrawable();
        DisplayButtonData displayData =
                new DrawableButtonData(
                        R.string.button_new_tab, R.string.button_new_incognito_tab, drawable);

        DelegateButtonData buttonData1 =
                new DelegateButtonData.Builder(displayData)
                        .setButtonState(ButtonState.UNCLICKABLE)
                        .build();
        DelegateButtonData buttonData2 =
                new DelegateButtonData.Builder(displayData)
                        .setButtonState(ButtonState.INVISIBLE_AND_CLICKABLE)
                        .build();

        assertTrue(buttonData1.buttonDataEquals(buttonData2));
    }

    @Test
    @SmallTest
    public void testButtonDataEquals_sameObject() {
        Drawable drawable = createBitmapDrawable();
        DisplayButtonData displayData =
                new DrawableButtonData(
                        R.string.button_new_tab, R.string.button_new_incognito_tab, drawable);

        DelegateButtonData buttonData = new DelegateButtonData.Builder(displayData).build();

        assertTrue(buttonData.buttonDataEquals(buttonData));
    }

    @Test
    @SmallTest
    public void testButtonState() {
        DelegateButtonData buttonData =
                new DelegateButtonData.Builder(mDisplayButtonData).setOnPress(mCallback).build();

        // Default state.
        assertEquals(ButtonState.DEFAULT, buttonData.getButtonState());
        assertTrue(buttonData.canPress());
        assertTrue(buttonData.isEnabled());

        // Change state to UNCLICKABLE.
        buttonData.setButtonState(ButtonState.UNCLICKABLE);
        assertEquals(ButtonState.UNCLICKABLE, buttonData.getButtonState());
        assertFalse(buttonData.canPress());
        assertTrue(buttonData.isEnabled()); // Remains enabled to avoid grey-out blink

        // Change to INVISIBLE_AND_CLICKABLE.
        buttonData.setButtonState(ButtonState.INVISIBLE_AND_CLICKABLE);
        assertEquals(ButtonState.INVISIBLE_AND_CLICKABLE, buttonData.getButtonState());
        assertTrue(buttonData.canPress());
        assertTrue(buttonData.isEnabled()); // Remains enabled to avoid grey-out blink

        // Change back to DEFAULT.
        buttonData.setButtonState(ButtonState.DEFAULT);
        assertEquals(ButtonState.DEFAULT, buttonData.getButtonState());
        assertTrue(buttonData.canPress());
        assertTrue(buttonData.isEnabled());
    }

    @Test
    @SmallTest
    public void testOnPress_triggersCallback() {
        View view = new View(ApplicationProvider.getApplicationContext());
        FullButtonData buttonData =
                new DelegateButtonData.Builder(mDisplayButtonData).setOnPress(mCallback).build();

        buttonData.onPress(view);

        verify(mCallback).onResult(view);
    }

    @Test
    @SmallTest
    public void testOnLongPress_triggersCallback() {
        View view = new View(ApplicationProvider.getApplicationContext());
        FullButtonData buttonData =
                new DelegateButtonData.Builder(mDisplayButtonData)
                        .setOnLongPress(mOnLongPressCallback)
                        .build();

        buttonData.onLongPress(view);

        verify(mOnLongPressCallback).onResult(view);
    }

    @Test
    @SmallTest
    public void testToggledState() {
        DelegateButtonData buttonData = new DelegateButtonData.Builder(mDisplayButtonData).build();

        // Default state.
        assertFalse(buttonData.isToggled());

        // Toggle the button.
        buttonData.setIsToggled(true);
        assertTrue(buttonData.isToggled());

        // Untoggle the button.
        buttonData.setIsToggled(false);
        assertFalse(buttonData.isToggled());
    }

    private Drawable createBitmapDrawable() {
        Bitmap image = Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888);
        Resources resources = ApplicationProvider.getApplicationContext().getResources();
        return new BitmapDrawable(resources, image);
    }
}
