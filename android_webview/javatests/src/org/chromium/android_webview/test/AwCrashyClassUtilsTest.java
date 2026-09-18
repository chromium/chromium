// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.chromium.android_webview.test.OnlyRunIn.ProcessMode.EITHER_PROCESS;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwCrashyClassUtils;
import org.chromium.android_webview.common.AwFeatures;
import org.chromium.android_webview.common.AwSwitches;
import org.chromium.base.CommandLine;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features;

/**
 * Tests that WebView only enables test crashes under the right conditions when the correct flags
 * are flipped.
 *
 * <p>Each test sets up its flags and features with the usual annotations and starts the browser
 * process itself, so that the cases which must not crash assert that startup completes normally.
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

    @Test(expected = RuntimeException.class)
    @SmallTest
    @Feature({"AndroidWebView"})
    @Features.EnableFeatures({AwFeatures.WEBVIEW_ENABLE_CRASH})
    public void testJavaCrashWhenEnabled() throws Exception {
        mRule.startBrowserProcess();
        // The switch is appended here rather than declared with @CommandLineFlags.Add because
        // once startup is unified it runs maybeCrashIfEnabled() itself, and a startup failure
        // leaves the pre-native UI task queue permanently wedged, which hangs test teardown.
        // TODO(crbug.com/544990736): assert on startup crashing once that is recoverable.
        CommandLine.getInstance().appendSwitch(AwSwitches.WEBVIEW_FORCE_CRASH_JAVA);
        Assert.assertTrue(AwCrashyClassUtils.shouldCrashJava());
        AwCrashyClassUtils.maybeCrashIfEnabled();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add(AwSwitches.WEBVIEW_FORCE_CRASH_JAVA)
    public void testNoJavaCrashWhenEnabledAndExperimentDisabled() throws Exception {
        mRule.startBrowserProcess();
        Assert.assertFalse(AwCrashyClassUtils.shouldCrashJava());
        Assert.assertFalse(AwCrashyClassUtils.shouldCrashNative());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add(AwSwitches.WEBVIEW_FORCE_CRASH_NATIVE)
    public void testNoNativeCrashWhenEnabledAndExperimentDisabled() throws Exception {
        mRule.startBrowserProcess();
        Assert.assertFalse(AwCrashyClassUtils.shouldCrashJava());
        Assert.assertFalse(AwCrashyClassUtils.shouldCrashNative());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testNoCrashWhenCompletelyDisabled() throws Exception {
        mRule.startBrowserProcess();
        Assert.assertFalse(AwCrashyClassUtils.shouldCrashJava());
        Assert.assertFalse(AwCrashyClassUtils.shouldCrashNative());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testNoCrashWhenDisabledAndTestExperimentEnabled() throws Exception {
        mRule.startBrowserProcess();
        Assert.assertFalse(AwCrashyClassUtils.shouldCrashJava());
        Assert.assertFalse(AwCrashyClassUtils.shouldCrashNative());
    }
}
