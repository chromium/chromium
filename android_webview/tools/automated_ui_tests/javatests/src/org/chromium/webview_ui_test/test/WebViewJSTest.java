// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webview_ui_test.test;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.RootMatchers.isDialog;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withText;
import static androidx.test.espresso.web.sugar.Web.onWebView;

import static org.chromium.ui.test.util.ViewUtils.VIEW_NULL;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.ui.test.util.ViewUtils;
import org.chromium.webview_ui_test.WebViewUiTestActivity;
import org.chromium.webview_ui_test.test.util.UseLayout;
import org.chromium.webview_ui_test.test.util.WebViewUiTestRule;

/**
 * This test suite is a collection of tests that loads Javascripts and interact with WebView.
 *
 * The ActivityTestRule used in this test ensures that Javascripts and webpages are loaded
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class WebViewJSTest {
    @Rule
    public WebViewUiTestRule mWebViewActivityRule =
            new WebViewUiTestRule(WebViewUiTestActivity.class);

    @Before
    public void setUp() {
        mWebViewActivityRule.launchActivity();
        onWebView().forceJavascriptEnabled();
    }

    @Test
    @MediumTest
    @UseLayout("fullscreen_webview")
    public void testJsLoad() {
        mWebViewActivityRule.loadFileSync("alert.html", false);
        mWebViewActivityRule.loadJavaScriptSync(
                "document.getElementById('alert-button').click();", false);
        // Wait for the correct view to be shown
        onView(withText("Clicked")).inRoot(isDialog()).check(matches(isDisplayed()));
        onView(withText("OK")).check(matches(isDisplayed())).perform(click());
        // "OK" should disappear once we've clicked on it. Wait for that to happen.
        ViewUtils.waitForViewCheckingState(withText("OK"), VIEW_NULL);
    }
}
