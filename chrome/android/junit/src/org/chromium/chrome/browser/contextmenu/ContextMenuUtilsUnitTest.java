// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.os.Build;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/** Unit tests for {@link ContextMenuUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(sdk = Build.VERSION_CODES.O)
public class ContextMenuUtilsUnitTest {
    @Rule
    public TestRule featureProcessor = new Features.JUnitProcessor();

    Activity mActivity;

    @Before
    public void setup() {
        mActivity = Robolectric.buildActivity(Activity.class).create().get();
    }

    @After
    public void tearDown() {
        mActivity.finish();
    }

    @Test
    @EnableFeatures({ChromeFeatureList.CONTEXT_MENU_POPUP_FOR_ALL_SCREEN_SIZES})
    @Config(qualifiers = "sw320dp")
    public void usePopupAllScreen_Small() {
        doTestUsePopupWhenEnabledByFlag();
    }

    @Test
    @EnableFeatures({ChromeFeatureList.CONTEXT_MENU_POPUP_FOR_ALL_SCREEN_SIZES})
    @Config(qualifiers = "sw600dp")
    public void usePopupAllScreen_Large() {
        doTestUsePopupWhenEnabledByFlag();
    }

    @Test
    @EnableFeatures({ChromeFeatureList.CONTEXT_MENU_POPUP_FOR_ALL_SCREEN_SIZES})
    @Config(qualifiers = "sw600dp", sdk = Build.VERSION_CODES.N)
    public void usePopupAllScreen_AndroidN() {
        doTestUsePopupWhenEnabledByFlag();
    }

    @Test
    @DisableFeatures({ChromeFeatureList.CONTEXT_MENU_POPUP_FOR_ALL_SCREEN_SIZES})
    @Config(qualifiers = "sw320dp")
    public void doNotUsePopupForSmallScreen() {
        assertFalse("Popup should not be used for small screen.",
                ContextMenuUtils.usePopupContextMenuForContext(mActivity));
    }

    @Test
    @DisableFeatures({ChromeFeatureList.CONTEXT_MENU_POPUP_FOR_ALL_SCREEN_SIZES})
    @Config(qualifiers = "sw600dp")
    public void usePopupForLargeScreen() {
        assertTrue("Popup should not be used for small screen.",
                ContextMenuUtils.usePopupContextMenuForContext(mActivity));
    }

    @Test
    @DisableFeatures({ChromeFeatureList.CONTEXT_MENU_POPUP_FOR_ALL_SCREEN_SIZES})
    @Config(qualifiers = "sw600dp", sdk = Build.VERSION_CODES.N)
    public void doNotUsePopupForAndroidN() {
        assertFalse("Should not use popup on Android N-.",
                ContextMenuUtils.usePopupContextMenuForContext(mActivity));
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void nullInputs() {
        assertFalse("Always return false for null input.",
                ContextMenuUtils.usePopupContextMenuForContext(null));
    }

    private void doTestUsePopupWhenEnabledByFlag() {
        assertTrue("Popup should be used when CONTEXT_MENU_POPUP_FOR_ALL_SCREEN_SIZES enabled.",
                ContextMenuUtils.usePopupContextMenuForContext(mActivity));
    }
}
