// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.chromium.android_webview.test.OnlyRunIn.ProcessMode.MULTI_PROCESS;

import androidx.test.filters.LargeTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwContentsStatics;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.HistogramWatcher;

/** These tests simply exercise the RendererLibraryPrefetchMode API to ensure there is no crash. */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
@DoNotBatch(reason = "Testing properties of startup")
@OnlyRunIn(MULTI_PROCESS) // Specifically tests renderer initialization.
public class RendererLibraryPrefetchModeTest extends AwParameterizedTest {
    @Rule public AwActivityTestRule mActivityTestRule;

    public RendererLibraryPrefetchModeTest(AwSettingsMutation param) {
        this.mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    // Exercises the renderer code. However, there is no specific observable behavior to assert on.
    public void doTest(int mode, int histogramMode) throws Throwable {
        try (HistogramWatcher ignored =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.WebView.RendererLibraryPrefetchMode", histogramMode)) {
            TestAwContentsClient contentsClient = new TestAwContentsClient();
            AwContents awContents =
                    mActivityTestRule
                            .createAwTestContainerViewOnMainSync(contentsClient)
                            .getAwContents();

            AwContentsStatics.setRendererLibraryPrefetchMode(mode);

            Assert.assertEquals(mode, AwContentsStatics.getRendererLibraryPrefetchMode());

            mActivityTestRule.loadHtmlSync(
                    awContents, contentsClient.getOnPageFinishedHelper(), "test");
        }
    }

    @Test
    @Feature({"AndroidWebView"})
    @LargeTest
    public void testDefault() throws Throwable {
        // RendererLibraryPrefetchMode::kDefault = 0
        doTest(0, 0);
    }

    @Test
    @Feature({"AndroidWebView"})
    @LargeTest
    public void testDisabled() throws Throwable {
        // RendererLibraryPrefetchMode::kDisabled = 1
        doTest(1, 1);
    }

    @Test
    @Feature({"AndroidWebView"})
    @LargeTest
    public void testEnabled() throws Throwable {
        // RendererLibraryPrefetchMode::kEnabled = 2
        doTest(2, 2);
    }

    @Test
    @Feature({"AndroidWebView"})
    @LargeTest
    public void testUnknownMode() throws Throwable {
        // Unrecognized modes should be ignored, resorting to default behavior.
        // The getter should still reflect the set value, but the histogram will log default.
        // (RendererLibraryPrefetchMode::kDefault = 0)
        doTest(-1, 0);
    }
}
