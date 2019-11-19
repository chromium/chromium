// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.profiling_host;

import android.support.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.heap_profiling.HeapProfilingTestShim;

/**
 * Test suite for out of process heap profiling.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class ProfilingProcessHostAndroidTest {
    private static final String TAG = "ProfilingProcessHostAndroidTest";
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    @Test
    @MediumTest
    @DisableIf
            .Build(sdk_is_greater_than = 20, message = "https://crbug.com/964502")
            @CommandLineFlags.Add({"memlog=browser",
                    "memlog-stack-mode=native-include-thread-names", "memlog-sampling-rate=1"})
            public void
            testModeBrowser() {
        HeapProfilingTestShim shim = new HeapProfilingTestShim();
        Assert.assertTrue(
                shim.runTestForMode("browser", false, "native-include-thread-names", false, false));
    }

    @DisabledTest(message = "https://crbug.com/970205")
    @Test
    @MediumTest
    public void testModeBrowserDynamicNonStreaming() {
        HeapProfilingTestShim shim = new HeapProfilingTestShim();
        Assert.assertTrue(shim.runTestForMode("browser", true, "native", false, false));
    }

    @DisabledTest(message = "https://crbug.com/970205")
    @Test
    @MediumTest
    public void testModeBrowserDynamicPseudoNonStreaming() {
        HeapProfilingTestShim shim = new HeapProfilingTestShim();
        Assert.assertTrue(shim.runTestForMode("browser", true, "pseudo", false, false));
    }

    // Non-browser processes must be profiled with a command line flag, since
    // otherwise, profiling will start after the relevant processes have been
    // created, thus that process will be not be profiled.
    // TODO(erikchen): Figure out what makes this test flaky and re-enable.
    // https://crbug.com/833590.
    @DisabledTest
    @Test
    @MediumTest
    @CommandLineFlags.
    Add({"memlog=all-renderers", "memlog-stack-mode=pseudo", "memlog-sampling-rate=1"})
    public void testModeRendererPseudo() {
        HeapProfilingTestShim shim = new HeapProfilingTestShim();
        Assert.assertTrue(shim.runTestForMode("all-renderers", false, "pseudo", false, false));
    }

    @DisabledTest(message = "https://crbug.com/970205")
    @Test
    @MediumTest
    @CommandLineFlags.Add({"memlog=gpu", "memlog-stack-mode=pseudo", "memlog-sampling-rate=1"})
    public void testModeGpuPseudo() {
        HeapProfilingTestShim shim = new HeapProfilingTestShim();
        Assert.assertTrue(shim.runTestForMode("gpu", false, "native", false, false));
    }

    @DisabledTest(message = "https://crbug.com/970205")
    @Test
    @MediumTest
    public void testModeBrowserDynamicPseudoSamplePartial() {
        HeapProfilingTestShim shim = new HeapProfilingTestShim();
        Assert.assertTrue(shim.runTestForMode("browser", true, "pseudo", true, false));
    }

    @DisabledTest(message = "https://crbug.com/986667")
    @Test
    @MediumTest
    public void testModeBrowserAndAllUtility() {
        HeapProfilingTestShim shim = new HeapProfilingTestShim();
        Assert.assertTrue(shim.runTestForMode("utility-and-browser", true, "pseudo", true, false));
    }
}
