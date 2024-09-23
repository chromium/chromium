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
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeSwitches;

/** Unit tests for {@link ContextMenuUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(sdk = Build.VERSION_CODES.O)
public class ContextMenuUtilsUnitTest {

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
    @Config(qualifiers = "sw320dp")
    @CommandLineFlags.Add(ChromeSwitches.FORCE_CONTEXT_MENU_POPUP)
    public void usePopupAllScreen_Small() {
        doTestUsePopupWhenEnabledByFlag();
    }

    @Test
    @Config(qualifiers = "sw600dp")
    @CommandLineFlags.Add(ChromeSwitches.FORCE_CONTEXT_MENU_POPUP)
    public void usePopupAllScreen_Large() {
        doTestUsePopupWhenEnabledByFlag();
    }

    @Test
    @Config(qualifiers = "sw320dp")
    public void doNotUsePopupForSmallScreen() {
        assertFalse(
                "Popup should not be used for small screen.",
                ContextMenuUtils.usePopupContextMenuForContext(mActivity));
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void usePopupForLargeScreen() {
        assertTrue(
                "Popup should not be used for small screen.",
                ContextMenuUtils.usePopupContextMenuForContext(mActivity));
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void nullInputs() {
        assertFalse(
                "Always return false for null input.",
                ContextMenuUtils.usePopupContextMenuForContext(null));
    }

    private void doTestUsePopupWhenEnabledByFlag() {
        assertTrue(
                "Popup should be used when switch FORCE_CONTEXT_MENU_POPUP is enabled.",
                ContextMenuUtils.usePopupContextMenuForContext(mActivity));
    }
}
