// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwContents;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.HistoryUtils;
import org.chromium.content_public.browser.test.util.TestCallbackHelperContainer.OnPageFinishedHelper;

/** Tests for a wanted clearHistory method. */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
public class ClearHistoryTest extends AwParameterizedTest {
    @Rule public AwActivityTestRule mActivityTestRule;

    private static final String[] URLS = new String[3];

    {
        for (int i = 0; i < URLS.length; i++) {
            URLS[i] =
                    UrlUtils.encodeHtmlDataUri("<html><head></head><body>" + i + "</body></html>");
        }
    }

    public ClearHistoryTest(AwSettingsMutation param) {
        this.mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    @Test
    @SmallTest
    @Feature({"History", "Main"})
    public void testClearHistory() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(contentsClient);
        final AwContents awContents = testContainerView.getAwContents();
        final WebContents webContents = awContents.getWebContents();
        OnPageFinishedHelper onPageFinishedHelper = contentsClient.getOnPageFinishedHelper();
        for (int i = 0; i < 3; i++) {
            mActivityTestRule.loadUrlSync(awContents, onPageFinishedHelper, URLS[i]);
        }

        HistoryUtils.goBackSync(
                InstrumentationRegistry.getInstrumentation(), webContents, onPageFinishedHelper);
        Assert.assertTrue(
                "Should be able to go back",
                HistoryUtils.canGoBackOnUiThread(
                        InstrumentationRegistry.getInstrumentation(), webContents));
        Assert.assertTrue(
                "Should be able to go forward",
                HistoryUtils.canGoForwardOnUiThread(
                        InstrumentationRegistry.getInstrumentation(), webContents));

        HistoryUtils.clearHistoryOnUiThread(
                InstrumentationRegistry.getInstrumentation(), webContents);
        Assert.assertFalse(
                "Should not be able to go back",
                HistoryUtils.canGoBackOnUiThread(
                        InstrumentationRegistry.getInstrumentation(), webContents));
        Assert.assertFalse(
                "Should not be able to go forward",
                HistoryUtils.canGoForwardOnUiThread(
                        InstrumentationRegistry.getInstrumentation(), webContents));
    }
}
