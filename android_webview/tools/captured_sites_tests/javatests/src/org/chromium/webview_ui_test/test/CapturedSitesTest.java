// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webview_ui_test.test;
import static androidx.test.espresso.web.sugar.Web.onWebView;

import static org.junit.Assert.assertTrue;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.MediumTest;
import androidx.test.uiautomator.UiDevice;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.webview_ui_test.WebViewUiTestActivity;
import org.chromium.webview_ui_test.test.util.CapturedSitesTestRule;
import org.chromium.webview_ui_test.test.util.PerformActions;
import org.chromium.webview_ui_test.test.util.UseLayout;
/**
 * Tests for WebView CapturedSites.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class CapturedSitesTest {
    @Rule
    public CapturedSitesTestRule mWebViewActivityRule =
            new CapturedSitesTestRule(WebViewUiTestActivity.class);
    private PerformActions mPerformActions;

    @Before
    public void setUp() {
        mWebViewActivityRule.launchActivity();
        onWebView().forceJavascriptEnabled();
        mPerformActions = new PerformActions(
                UiDevice.getInstance(InstrumentationRegistry.getInstrumentation()),
                mWebViewActivityRule);
    }

    // Tests scrolling to the credit card name field, clicking on it,
    // and getting expected autofill result on static smoke page.
    @Test
    @MediumTest
    @UseLayout("fullscreen_webview")
    public void testSmoke() throws Throwable {
        char esc = '\\';
        String credit_card = "//*[@id=" + esc + "\"credit_card_name" + esc + "\"]";
        mPerformActions.loadUrl("https://rsolomakhin.github.io/autofill/");
        assertTrue(mPerformActions.autofill(credit_card));
        assertTrue(mPerformActions.verifyAutofill(credit_card, "obama"));
    }
}
