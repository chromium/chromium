// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webview_ui_test.test;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.RootMatchers.withDecorView;
import static androidx.test.espresso.matcher.ViewMatchers.isEnabled;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;
import static androidx.test.espresso.web.assertion.WebViewAssertions.webMatches;
import static androidx.test.espresso.web.sugar.Web.onWebView;
import static androidx.test.espresso.web.webdriver.DriverAtoms.findElement;

import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertEquals;

import android.graphics.Point;
import android.view.View;
import android.webkit.WebView;

import androidx.test.espresso.web.sugar.Web;
import androidx.test.espresso.web.webdriver.Locator;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.webview_ui_test.R;
import org.chromium.webview_ui_test.WebViewUiTestActivity;
import org.chromium.webview_ui_test.test.util.Actions;
import org.chromium.webview_ui_test.test.util.Atoms;
import org.chromium.webview_ui_test.test.util.UseLayout;
import org.chromium.webview_ui_test.test.util.WebViewUiTestRule;

/** Tests for WebView ActionMode. */
// TODO(aluo): Re-enable once crbug.com/947352 is fixed.
@DisabledTest(message = "https://crbug.com/947352")
@RunWith(BaseJUnit4ClassRunner.class)
public class DropDownListTest {
    @Rule
    public WebViewUiTestRule mWebViewActivityRule =
            new WebViewUiTestRule(WebViewUiTestActivity.class);

    private static final String AFTER_VALUE = "Value2";
    private static final String BEFORE_VALUE = "Value1";
    private static final String HTML_NON_SCALED = "webview_nonscaled.html";
    private static final String HTML_SCALED = "webview_scaled.html";
    private static final String HTML_SCROLL = "webview_scroll.html";

    @Before
    public void setUp() {
        mWebViewActivityRule.launchActivity();
        onWebView().forceJavascriptEnabled();
    }

    /** Test Drop Down List works in ViewPort Scale Factor = 1 */
    @Test
    @SmallTest
    @UseLayout("edittext_webview")
    public void testDropDownNonScaledViewPort() {
        mWebViewActivityRule.loadFileSync(HTML_NON_SCALED, false);
        changeAllSelectValues();
    }

    /** Test Drop Down List works in ViewPort Scale Factor > 1 */
    @Test
    @SmallTest
    @UseLayout("edittext_webview")
    public void testDropDownScaledViewPort() {
        mWebViewActivityRule.loadFileSync(HTML_SCALED, false);
        changeAllSelectValues();
    }

    /** Test Drop Down List works in ViewPort Scale Factor > 1 in wideViewPortMode */
    @Test
    @SmallTest
    @UseLayout("edittext_webview")
    public void testDropDownScaledViewPortUseWideViewPort() {
        onView(withId(R.id.webview)).perform(Actions.setUseWideViewPort());
        mWebViewActivityRule.loadFileSync(HTML_SCALED, false);
        WebView webView = (WebView) mWebViewActivityRule.getActivity().findViewById(R.id.webview);
        int w = webView.getWidth();
        int h = webView.getHeight();
        changeSelectValue("select1");
        onView(withId(R.id.webview)).perform(Actions.scrollBy(w, 0));
        changeSelectValue("select2");
        onView(withId(R.id.webview)).perform(Actions.scrollBy(0, h));
        changeSelectValue("select3");
        onView(withId(R.id.webview)).perform(Actions.scrollBy(-w, 0));
        changeSelectValue("select4");
    }

    /** Test that WebView does not scroll when a drop down menu is selected */
    @Test
    @SmallTest
    @UseLayout("edittext_webview")
    public void testSelectNoScroll() {
        mWebViewActivityRule.loadFileSync(HTML_SCROLL, false);
        WebView webView = (WebView) mWebViewActivityRule.getActivity().findViewById(R.id.webview);
        int w = webView.getWidth();
        int h = webView.getHeight();
        onView(withId(R.id.webview)).perform(Actions.scrollBy(w, h));
        Point beforeSelectScroll =
                getScroll((WebView) mWebViewActivityRule.getActivity().findViewById(R.id.webview));
        changeSelectValue("select");
        Point afterSelectScroll =
                getScroll((WebView) mWebViewActivityRule.getActivity().findViewById(R.id.webview));
        assertEquals(
                "Scroll should not move after clicking select element",
                beforeSelectScroll,
                afterSelectScroll);
    }

    /** Get the scroll position of the view */
    private Point getScroll(View v) {
        int x = v.getScrollX();
        int y = v.getScrollY();
        return new Point(x, y);
    }

    /** Change all select box values */
    private void changeAllSelectValues() {
        changeSelectValue("select1");
        changeSelectValue("select2");
        changeSelectValue("select3");
        changeSelectValue("select4");
    }

    /** Change select box value from Value1 to Value2 */
    private void changeSelectValue(String id) {
        Web.WebInteraction<Void> selectElement =
                onWebView().withElement(findElement(Locator.ID, id));
        selectElement.check(webMatches(Atoms.currentSelection(), is(BEFORE_VALUE)));
        selectElement.perform(Atoms.webSelect());
        onView(withText(AFTER_VALUE)).inRoot(withDecorView(isEnabled())).perform(click());
        selectElement.check(webMatches(Atoms.currentSelection(), is(AFTER_VALUE)));
    }
}
