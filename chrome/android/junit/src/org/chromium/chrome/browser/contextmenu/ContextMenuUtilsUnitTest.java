// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.app.Activity;

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
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.content_public.common.ContentFeatures;

/** Unit tests for {@link ContextMenuUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
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
    @Features.EnableFeatures({ContentFeatures.TOUCH_DRAG_AND_CONTEXT_MENU})
    public void forcePopupWhenDragDropEnabled() {
        assertTrue("ForcePopupStyle should be enabled.", ContextMenuUtils.forcePopupStyleEnabled());
    }

    @Test
    @Features.DisableFeatures({ContentFeatures.TOUCH_DRAG_AND_CONTEXT_MENU})
    public void doNotForcePopupWhenDragDropDisabled() {
        assertFalse(
                "ForcePopupStyle should be disabled.", ContextMenuUtils.forcePopupStyleEnabled());
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.CONTEXT_MENU_POPUP_FOR_ALL_SCREEN_SIZES})
    @Config(qualifiers = "sw320dp")
    public void usePopupAllScreen_Small() {
        doTestUsePopupWhenEnabledByFlag();
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.CONTEXT_MENU_POPUP_FOR_ALL_SCREEN_SIZES})
    @Config(qualifiers = "sw600dp")
    public void usePopupAllScreen_Large() {
        doTestUsePopupWhenEnabledByFlag();
    }

    @Test
    @Features.DisableFeatures({ChromeFeatureList.CONTEXT_MENU_POPUP_FOR_ALL_SCREEN_SIZES})
    @Config(qualifiers = "sw320dp")
    public void doNotUsePopupForSmallScreen() {
        assertFalse("Popup should not be used for small screen.",
                ContextMenuUtils.usePopupContextMenuForContext(mActivity));
    }

    @Test
    @Features.DisableFeatures({ChromeFeatureList.CONTEXT_MENU_POPUP_FOR_ALL_SCREEN_SIZES})
    @Config(qualifiers = "sw600dp")
    public void usePopupForLargeScreen() {
        assertTrue("Popup should not be used for small screen.",
                ContextMenuUtils.usePopupContextMenuForContext(mActivity));
    }

    private void doTestUsePopupWhenEnabledByFlag() {
        assertTrue("Popup should be used when CONTEXT_MENU_POPUP_FOR_ALL_SCREEN_SIZES enabled.",
                ContextMenuUtils.usePopupContextMenuForContext(mActivity));
    }
}
