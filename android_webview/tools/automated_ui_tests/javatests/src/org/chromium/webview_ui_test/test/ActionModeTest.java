// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webview_ui_test.test;

import static androidx.test.espresso.Espresso.onData;
import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.actionWithAssertions;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.intent.Intents.assertNoUnverifiedIntents;
import static androidx.test.espresso.intent.Intents.intended;
import static androidx.test.espresso.intent.Intents.intending;
import static androidx.test.espresso.intent.matcher.BundleMatchers.hasEntry;
import static androidx.test.espresso.intent.matcher.IntentMatchers.anyIntent;
import static androidx.test.espresso.intent.matcher.IntentMatchers.hasAction;
import static androidx.test.espresso.intent.matcher.IntentMatchers.hasExtra;
import static androidx.test.espresso.intent.matcher.IntentMatchers.hasExtras;
import static androidx.test.espresso.intent.matcher.IntentMatchers.hasType;
import static androidx.test.espresso.matcher.RootMatchers.withDecorView;
import static androidx.test.espresso.matcher.ViewMatchers.isClickable;
import static androidx.test.espresso.matcher.ViewMatchers.isEnabled;
import static androidx.test.espresso.matcher.ViewMatchers.withContentDescription;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;
import static androidx.test.espresso.web.assertion.WebViewAssertions.webMatches;
import static androidx.test.espresso.web.sugar.Web.onWebView;
import static androidx.test.espresso.web.webdriver.DriverAtoms.findElement;
import static androidx.test.espresso.web.webdriver.DriverAtoms.getText;

import static org.hamcrest.CoreMatchers.allOf;
import static org.hamcrest.Matchers.containsString;
import static org.hamcrest.Matchers.equalTo;
import static org.hamcrest.core.AnyOf.anyOf;
import static org.junit.Assert.assertTrue;

import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;

import android.app.Activity;
import android.app.Instrumentation;
import android.content.Intent;
import android.view.MenuItem;

import androidx.test.espresso.NoMatchingViewException;
import androidx.test.espresso.PerformException;
import androidx.test.espresso.Root;
import androidx.test.espresso.action.GeneralClickAction;
import androidx.test.espresso.action.GeneralLocation;
import androidx.test.espresso.action.Press;
import androidx.test.espresso.action.Tap;
import androidx.test.espresso.intent.Intents;
import androidx.test.espresso.web.webdriver.Locator;
import androidx.test.filters.SmallTest;

import org.hamcrest.Description;
import org.hamcrest.Matcher;
import org.hamcrest.TypeSafeMatcher;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.webview_ui_test.R;
import org.chromium.webview_ui_test.WebViewUiTestActivity;
import org.chromium.webview_ui_test.test.util.UseLayout;
import org.chromium.webview_ui_test.test.util.WebViewUiTestRule;

/** Tests for WebView ActionMode. */
@DisabledTest(message = "https://crbug.com/947352")
@RunWith(BaseJUnit4ClassRunner.class)
public class ActionModeTest {
    private static final String TAG = "ActionModeTest";

    // Actions available in action mode
    private static final String ASSIST_ACTION = "Assist";
    private static final String COPY_ACTION = "Copy";
    private static final String MORE_OPTIONS_ACTION = "More options";
    private static final String PASTE_ACTION = "Paste";
    private static final String SHARE_ACTION = "Share";
    private static final String SELECT_ALL_ACTION = "Select all";
    private static final String WEB_SEARCH_ACTION = "Web search";

    private static final String QUICK_SEARCH_BOX_PKG = "com.google.android.googlequicksearchbox";
    private static final long ASSIST_TIMEOUT = scaleTimeout(5000);

    @Rule
    public WebViewUiTestRule mWebViewActivityRule =
            new WebViewUiTestRule(WebViewUiTestActivity.class);

    @Before
    public void setUp() {
        mWebViewActivityRule.launchActivity();
        onWebView().forceJavascriptEnabled();
        mWebViewActivityRule.loadDataSync(
                "<html><body><p>Hello world</p></body></html>", "text/html", "utf-8", false);
        onWebView(withId(R.id.webview))
                .withElement(findElement(Locator.TAG_NAME, "p"))
                .check(webMatches(getText(), containsString("Hello world")));
    }

    /** Test Copy and Paste */
    @Test
    @SmallTest
    @UseLayout("edittext_webview")
    public void testCopyPaste() {
        longClickOnLastWord(R.id.webview);
        clickPopupAction(COPY_ACTION);
        longClickOnLastWord(R.id.edittext);
        clickPopupAction(PASTE_ACTION);
        onView(withId(R.id.edittext)).check(matches(withText("world")));
    }

    /** Test Select All */
    @Test
    @SmallTest
    @UseLayout("edittext_webview")
    public void testSelectAll() {
        longClickOnLastWord(R.id.webview);
        clickPopupAction(SELECT_ALL_ACTION);
        clickPopupAction(COPY_ACTION);
        longClickOnLastWord(R.id.edittext);
        clickPopupAction(PASTE_ACTION);
        onView(withId(R.id.edittext)).check(matches(withText("Hello world")));
    }

    /** Test Share */
    @Test
    @SmallTest
    @UseLayout("edittext_webview")
    public void testShare() {
        Intents.init();
        intending(anyIntent())
                .respondWith(new Instrumentation.ActivityResult(Activity.RESULT_OK, new Intent()));

        longClickOnLastWord(R.id.webview);
        clickPopupAction(SHARE_ACTION);

        intended(
                allOf(
                        hasAction(Intent.ACTION_CHOOSER),
                        hasExtras(
                                allOf(
                                        hasEntry(Intent.EXTRA_TITLE, SHARE_ACTION),
                                        hasEntry(
                                                Intent.EXTRA_INTENT,
                                                allOf(
                                                        hasAction(Intent.ACTION_SEND),
                                                        hasType("text/plain"),
                                                        hasExtra(Intent.EXTRA_TEXT, "world")))))));
        assertNoUnverifiedIntents();
    }

    /** Test Web Search */
    @Test
    @SmallTest
    @UseLayout("edittext_webview")
    public void testWebSearch() {
        Intents.init();
        intending(anyIntent())
                .respondWith(new Instrumentation.ActivityResult(Activity.RESULT_OK, new Intent()));
        longClickOnLastWord(R.id.webview);
        clickPopupAction(WEB_SEARCH_ACTION);
        intended(
                allOf(
                        hasAction(Intent.ACTION_WEB_SEARCH),
                        hasExtras(
                                allOf(
                                        hasEntry(
                                                "com.android.browser.application_id",
                                                "org.chromium.webview_ui_test"),
                                        hasEntry("query", "world"),
                                        hasEntry("new_search", true)))));
        assertNoUnverifiedIntents();
    }

    /** Click an item on the Action Mode popup */
    public void clickPopupAction(final String name) {
        Matcher<Root> rootMatcher = withDecorView(isEnabled());

        try {
            onView(allOf(anyOf(withText(name), withContentDescription(name)), isClickable()))
                    .inRoot(rootMatcher)
                    .perform(click());
        } catch (PerformException | NoMatchingViewException e) {
            // Take care of case when the item is in the overflow menu
            onView(allOf(withContentDescription(MORE_OPTIONS_ACTION), isClickable()))
                    .inRoot(rootMatcher)
                    .perform(click());
            onData(new MenuItemMatcher(equalTo(name))).inRoot(rootMatcher).perform(click());
        }

        // After select all action is clicked, the PopUp Menu may disappear briefly due to selection
        // change, wait for the menu to reappear
        if (name.equals(SELECT_ALL_ACTION)) {
            assertTrue(mWebViewActivityRule.waitForActionBarPopup());
        }
    }

    /**
     * Perform a view action that clicks on the last word and start the idling resource
     * to wait for completion of the popup menu
     */
    private final void longClickOnLastWord(int viewId) {
        // TODO(aluo): This function is not guaranteed to click on element. Change to
        // implementation that gets bounding box for elements using Javascript.
        onView(withId(viewId))
                .perform(
                        actionWithAssertions(
                                new GeneralClickAction(
                                        Tap.LONG, GeneralLocation.CENTER_RIGHT, Press.FINGER)));
        assertTrue(mWebViewActivityRule.waitForActionBarPopup());
    }

    /** Matches an item on the Action Mode popup by the title */
    private static class MenuItemMatcher extends TypeSafeMatcher<MenuItem> {
        private Matcher<String> mTitleMatcher;

        public MenuItemMatcher(Matcher<String> titleMatcher) {
            mTitleMatcher = titleMatcher;
        }

        @Override
        protected boolean matchesSafely(MenuItem item) {
            return mTitleMatcher.matches(item.getTitle());
        }

        @Override
        public void describeTo(Description description) {
            description.appendText("has MenuItem with title: ");
            description.appendDescriptionOf(mTitleMatcher);
        }
    }
}
