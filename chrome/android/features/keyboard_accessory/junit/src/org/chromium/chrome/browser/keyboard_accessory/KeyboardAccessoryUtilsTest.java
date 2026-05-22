// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.res.Configuration;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.DeviceInput;

/** Unit tests for {@link KeyboardAccessoryUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class KeyboardAccessoryUtilsTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private KeyboardVisibilityDelegate mMockKeyboardDelegate;

    private Activity mActivity;
    private Configuration mConfiguration;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mConfiguration = mActivity.getResources().getConfiguration();
        DeviceInput.setSupportsAlphabeticKeyboardForTesting(null);
    }

    @Test
    public void testIsLargeFormFactor_returnsFalseForPhoneDimensions() {
        mConfiguration.screenWidthDp = 360;
        mConfiguration.screenHeightDp = 640;
        when(mMockKeyboardDelegate.isKeyboardShowing(any())).thenReturn(false);
        DeviceInput.setSupportsAlphabeticKeyboardForTesting(false);

        assertFalse(KeyboardAccessoryUtils.isLargeFormFactor(mActivity, mMockKeyboardDelegate));
    }

    @Test
    public void testIsLargeFormFactor_returnsTrueForExpandedTabletDimensions() {
        mConfiguration.screenWidthDp =
                900; // Width > 840dp (EXPANDED_WINDOW_WIDTH_FOR_UNDOCKED_BAR_DP)
        mConfiguration.screenHeightDp = 600; // Height > 480dp
        when(mMockKeyboardDelegate.isKeyboardShowing(any())).thenReturn(false);
        DeviceInput.setSupportsAlphabeticKeyboardForTesting(false);

        assertTrue(KeyboardAccessoryUtils.isLargeFormFactor(mActivity, mMockKeyboardDelegate));
    }

    @Test
    public void testIsLargeFormFactor_returnsTrueForTabletDimensionsWithPhysicalKeyboard() {
        mConfiguration.screenWidthDp = 720; // Width between 600dp and 840dp
        mConfiguration.screenHeightDp = 600;
        when(mMockKeyboardDelegate.isKeyboardShowing(any())).thenReturn(false);
        DeviceInput.setSupportsAlphabeticKeyboardForTesting(true); // Physical keyboard connected

        assertTrue(KeyboardAccessoryUtils.isLargeFormFactor(mActivity, mMockKeyboardDelegate));
    }

    @Test
    public void testIsLargeFormFactor_returnsFalseForTabletDimensionsWithoutPhysicalKeyboard() {
        mConfiguration.screenWidthDp = 720; // Width between 600dp and 840dp
        mConfiguration.screenHeightDp = 600;
        when(mMockKeyboardDelegate.isKeyboardShowing(any())).thenReturn(false);
        DeviceInput.setSupportsAlphabeticKeyboardForTesting(false); // No physical keyboard

        assertFalse(KeyboardAccessoryUtils.isLargeFormFactor(mActivity, mMockKeyboardDelegate));
    }

    @Test
    public void testIsLargeFormFactor_returnsFalseForTabletWithSoftKeyboardShowing() {
        mConfiguration.screenWidthDp = 720;
        mConfiguration.screenHeightDp = 600;
        when(mMockKeyboardDelegate.isKeyboardShowing(any()))
                .thenReturn(true); // Soft keyboard active
        DeviceInput.setSupportsAlphabeticKeyboardForTesting(true);

        assertFalse(KeyboardAccessoryUtils.isLargeFormFactor(mActivity, mMockKeyboardDelegate));
    }
}
