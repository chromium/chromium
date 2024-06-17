// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.chromium.android_webview.test.OnlyRunIn.ProcessMode.EITHER_PROCESS;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwCrashyClassUtils;
import org.chromium.android_webview.common.AwFeatures;
import org.chromium.android_webview.common.AwSwitches;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features;

/**
 * Tests that WebView only enables test crashes under the right conditions when the correct flags
 * are flipped.
 */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
@OnlyRunIn(EITHER_PROCESS) // These tests don't use the renderer process, they test browser crashes
@DoNotBatch(reason = "needsBrowserProcessStarted false and @Batch are incompatible")
public class AwCrashyClassUtilsTest extends AwParameterizedTest {

    @Rule public AwActivityTestRule mRule;

    public AwCrashyClassUtilsTest(AwSettingsMutation param) {
        this.mRule =
                new AwActivityTestRule(param.getMutation()) {
                    @Override
                    public boolean needsBrowserProcessStarted() {
                        return false;
                    }
                };
    }

    @Before
    public void setUp() throws Exception {
        mRule.startBrowserProcess();
    }

    @Test(expected = RuntimeException.class)
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({AwSwitches.WEBVIEW_FORCE_CRASH_JAVA})
    @Features.EnableFeatures({AwFeatures.WEBVIEW_ENABLE_CRASH})
    public void testJavaCrashWhenEnabled() {
        Assert.assertTrue(AwCrashyClassUtils.shouldCrashJava());
        AwCrashyClassUtils.maybeCrashIfEnabled();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add(AwSwitches.WEBVIEW_FORCE_CRASH_JAVA)
    public void testNoJavaCrashWhenEnabledAndExperimentDisabled() {
        Assert.assertFalse(AwCrashyClassUtils.shouldCrashJava());
        Assert.assertFalse(AwCrashyClassUtils.shouldCrashNative());
        AwCrashyClassUtils.maybeCrashIfEnabled();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add(AwSwitches.WEBVIEW_FORCE_CRASH_NATIVE)
    public void testNoNativeCrashWhenEnabledAndExperimentDisabled() {
        Assert.assertFalse(AwCrashyClassUtils.shouldCrashJava());
        Assert.assertFalse(AwCrashyClassUtils.shouldCrashNative());
        AwCrashyClassUtils.maybeCrashIfEnabled();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testNoCrashWhenCompletelyDisabled() {
        Assert.assertFalse(AwCrashyClassUtils.shouldCrashJava());
        Assert.assertFalse(AwCrashyClassUtils.shouldCrashNative());
        AwCrashyClassUtils.maybeCrashIfEnabled();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testNoCrashWhenDisabledAndTestExperimentEnabled() {
        Assert.assertFalse(AwCrashyClassUtils.shouldCrashJava());
        Assert.assertFalse(AwCrashyClassUtils.shouldCrashNative());
        AwCrashyClassUtils.maybeCrashIfEnabled();
    }
}
