// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.chromium.android_webview.test.OnlyRunIn.ProcessMode.MULTI_PROCESS;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.components.heap_profiling.multi_process.HeapProfilingTestShim;

/** Tests suite for heap profiling. */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
@OnlyRunIn(MULTI_PROCESS)
public class HeapProfilingTest extends AwParameterizedTest {
    @Rule public AwActivityTestRule mActivityTestRule;

    public HeapProfilingTest(AwSettingsMutation param) {
        this.mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    @Before
    public void setUp() {}

    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/1163744")
    @CommandLineFlags.Add({
        "memlog=browser",
        "memlog-stack-mode=native-include-thread-names",
        "memlog-sampling-rate=1"
    })
    public void testModeBrowser() {
        HeapProfilingTestShim shim = new HeapProfilingTestShim();
        Assert.assertTrue(
                shim.runTestForMode("browser", false, "native-include-thread-names", false, false));
    }

    @Test
    @MediumTest
    public void testModeBrowserDynamicPseudo() {
        HeapProfilingTestShim shim = new HeapProfilingTestShim();
        Assert.assertTrue(shim.runTestForMode("browser", true, "pseudo", false, false));
    }

    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/1173304")
    public void testModeBrowserDynamicPseudoSampleEverything() {
        HeapProfilingTestShim shim = new HeapProfilingTestShim();
        Assert.assertTrue(shim.runTestForMode("browser", true, "pseudo", true, true));
    }

    @Test
    @MediumTest
    public void testModeBrowserDynamicPseudoSamplePartial() {
        HeapProfilingTestShim shim = new HeapProfilingTestShim();
        Assert.assertTrue(shim.runTestForMode("browser", true, "pseudo", true, false));
    }
}
