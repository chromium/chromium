// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwCrashyClassUtils;
import org.chromium.android_webview.common.AwFeatures;
import org.chromium.android_webview.common.AwSwitches;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features;

/**
 * Tests that WebView only enables test crashes under the right conditions when the correct flags
 * are flipped.
 */
@Batch(Batch.PER_CLASS)
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
public class AwCrashyClassUtilsTest extends AwParameterizedTest {

    @Rule public AwActivityTestRule mRule;
    @Rule public TestRule mProcessor = new Features.InstrumentationProcessor();

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
    public void testJavaCrashWhenEnabled() {
        Assert.assertTrue(AwCrashyClassUtils.shouldCrashJava());
        AwCrashyClassUtils.maybeCrashIfEnabled();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @Features.DisableFeatures({AwFeatures.WEBVIEW_ENABLE_CRASH})
    @CommandLineFlags.Add(AwSwitches.WEBVIEW_FORCE_CRASH_JAVA)
    public void testNoJavaCrashWhenEnabledAndExperimentDisabled() {
        Assert.assertFalse(AwCrashyClassUtils.shouldCrashJava());
        Assert.assertFalse(AwCrashyClassUtils.shouldCrashNative());
        AwCrashyClassUtils.maybeCrashIfEnabled();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @Features.DisableFeatures({AwFeatures.WEBVIEW_ENABLE_CRASH})
    @CommandLineFlags.Add(AwSwitches.WEBVIEW_FORCE_CRASH_NATIVE)
    public void testNoNativeCrashWhenEnabledAndExperimentDisabled() {
        Assert.assertFalse(AwCrashyClassUtils.shouldCrashJava());
        Assert.assertFalse(AwCrashyClassUtils.shouldCrashNative());
        AwCrashyClassUtils.maybeCrashIfEnabled();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @Features.DisableFeatures({AwFeatures.WEBVIEW_ENABLE_CRASH})
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
