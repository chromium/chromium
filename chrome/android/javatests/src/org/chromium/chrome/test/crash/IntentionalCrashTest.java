// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.crash;

import androidx.test.filters.SmallTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Manual;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;

/**
 * Tests that intentionally crash in different ways.
 *
 * <p>These are all purposefully disabled and should only be run manually.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
public class IntentionalCrashTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Test
    @SmallTest
    @Manual(message = "Intentional crash.")
    public void testRendererCrash() {
        mActivityTestRule.startMainActivityFromLauncher();
        mActivityTestRule.loadUrl("chrome://crash");
    }

    @Test
    @SmallTest
    @Manual(message = "Intentional crash.")
    public void testBrowserCrash() {
        mActivityTestRule.startMainActivityFromLauncher();
        mActivityTestRule.loadUrl("chrome://inducebrowsercrashforrealz");
    }

    @Test
    @SmallTest
    @Manual(message = "Intentional crash.")
    public void testJavaCrash() {
        mActivityTestRule.startMainActivityFromLauncher();
        mActivityTestRule.loadUrl("chrome://java-crash/");
    }

    @Test
    @SmallTest
    @Manual(message = "Intentional crash.")
    public void testGpuCrash() {
        mActivityTestRule.startMainActivityFromLauncher();
        mActivityTestRule.loadUrl("chrome://gpucrash");
    }
}
