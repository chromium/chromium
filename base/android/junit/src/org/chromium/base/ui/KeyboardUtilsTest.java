// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.ui;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import android.graphics.Insets;
import android.os.Build;
import android.view.View;
import android.view.WindowInsets;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.BaseFeatures;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;

/** Unit tests for {@link KeyboardUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
// WindowInsets.Type.ime() and WindowInsets#getInsets(int) were added in R.
@Config(sdk = Build.VERSION_CODES.R)
public class KeyboardUtilsTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private WindowInsets mWindowInsets;

    private View mRootView;

    @Before
    public void setUp() {
        mRootView =
                new View(ContextUtils.getApplicationContext()) {
                    @Override
                    public WindowInsets getRootWindowInsets() {
                        return mWindowInsets;
                    }
                };
    }

    @Test
    public void calculateKeyboardHeight_subtractsVisibleBottomSystemBars() {
        when(mWindowInsets.getInsets(WindowInsets.Type.ime())).thenReturn(Insets.of(0, 0, 0, 392));
        when(mWindowInsets.getInsets(WindowInsets.Type.systemBars()))
                .thenReturn(Insets.of(0, 0, 0, 49));

        assertEquals(343, KeyboardUtils.calculateKeyboardHeightFromWindowInsets(mRootView));
    }

    @Test
    public void calculateKeyboardHeight_clampsAtZero() {
        when(mWindowInsets.getInsets(WindowInsets.Type.ime())).thenReturn(Insets.of(0, 0, 0, 30));
        when(mWindowInsets.getInsets(WindowInsets.Type.systemBars()))
                .thenReturn(Insets.of(0, 0, 0, 49));

        assertEquals(0, KeyboardUtils.calculateKeyboardHeightFromWindowInsets(mRootView));
    }

    @Test
    @DisableFeatures(BaseFeatures.VIRTUAL_KEYBOARD_GEOMETRY_AND_INSET_FIXES)
    public void calculateKeyboardHeight_withKillSwitch_usesLegacyComputation() {
        when(mWindowInsets.getInsets(WindowInsets.Type.ime())).thenReturn(Insets.of(0, 0, 0, 392));
        when(mWindowInsets.getInsets(WindowInsets.Type.systemBars()))
                .thenReturn(Insets.of(0, 0, 0, 0));
        when(mWindowInsets.getInsetsIgnoringVisibility(WindowInsets.Type.navigationBars()))
                .thenThrow(new NullPointerException("should not be called"));

        assertEquals(392, KeyboardUtils.calculateKeyboardHeightFromWindowInsets(mRootView));
    }

    @Test
    public void isAndroidSoftKeyboardShowing_checksImeVisibilityDirectly() {
        when(mWindowInsets.getInsets(WindowInsets.Type.ime())).thenReturn(Insets.of(0, 0, 0, 0));
        when(mWindowInsets.isVisible(WindowInsets.Type.ime())).thenReturn(true);

        assertEquals(0, KeyboardUtils.calculateKeyboardHeightFromWindowInsets(mRootView));
        assertTrue(KeyboardUtils.isAndroidSoftKeyboardShowing(mRootView));

        when(mWindowInsets.isVisible(WindowInsets.Type.ime())).thenReturn(false);
        assertFalse(KeyboardUtils.isAndroidSoftKeyboardShowing(mRootView));
    }

    @Test
    @DisableFeatures(BaseFeatures.SPLIT_SCREEN_IME_INSET_VISIBILITY)
    public void isAndroidSoftKeyboardShowing_withKillSwitch_checksKeyboardHeight() {
        when(mWindowInsets.getInsets(WindowInsets.Type.ime())).thenReturn(Insets.of(0, 0, 0, 0));
        when(mWindowInsets.isVisible(WindowInsets.Type.ime())).thenReturn(true);

        assertFalse(KeyboardUtils.isAndroidSoftKeyboardShowing(mRootView));
    }
}
