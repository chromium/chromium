// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.when;

import android.content.Context;
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
}
