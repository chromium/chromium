// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.intent.Intents.intended;
import static androidx.test.espresso.intent.Intents.intending;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.equalTo;

import static org.chromium.android_webview.test.devui.DeveloperUiTestUtils.getClipBoardTextOnUiThread;

import android.app.Activity;
import android.app.Instrumentation;
import android.content.Context;
import android.content.Intent;
import android.util.Pair;
import android.view.KeyEvent;

import androidx.test.espresso.intent.Intents;
import androidx.test.espresso.intent.matcher.IntentMatchers;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.common.AwFeatures;
import org.chromium.android_webview.contextmenu.AwContextMenuHeaderCoordinator;
import org.chromium.android_webview.contextmenu.AwContextMenuItem;
import org.chromium.android_webview.contextmenu.AwContextMenuItem.Item;
import org.chromium.android_webview.contextmenu.AwContextMenuItemDelegate;
import org.chromium.android_webview.contextmenu.AwContextMenuPopulator;
import org.chromium.android_webview.test.AwActivityTestRule.TestDependencyFactory;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features;
import org.chromium.components.embedder_support.contextmenu.ContextMenuParams;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.net.test.util.TestWebServer;
import org.chromium.ui.base.MenuSourceType;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.url.GURL;

import java.util.List;

/** Tests for context menu methods */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
@Batch(Batch.PER_CLASS)
@Features.EnableFeatures({AwFeatures.WEBVIEW_HYPERLINK_CONTEXT_MENU})
public class ContextMenuTest extends AwParameterizedTest {
    private static final String FILE = "/main.html";
    private static final String DATA =
            "<html><head></head><body>"
                    + "<a href='test_link.html' id='testLink'>Test Link</a>"
                    + "</body></html>";

    @Rule public AwActivityTestRule mRule;

    private TestWebServer mWebServer;
    private AwTestContainerView mTestContainerView;
    private TestAwContentsClient mContentsClient;
    private CallbackHelper mCallbackHelper = new CallbackHelper();
    private AwContents mAwContents;
    private Context mContext;

    public ContextMenuTest(AwSettingsMutation param) {
        mRule = new AwActivityTestRule(param.getMutation());
    }

    @Before
    public void setUp() throws Exception {
        mWebServer = TestWebServer.start();
        mContentsClient = new TestAwContentsClient();
        mTestContainerView =
                mRule.createAwTestContainerViewOnMainSync(
                        mContentsClient, false, new TestDependencyFactory());
        mAwContents = mTestContainerView.getAwContents();
        mContext = mAwContents.getWebContents().getTopLevelNativeWindow().getContext().get();
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);
    }

    @After
    public void tearDown() {
        if (mWebServer != null) {
            mWebServer.shutdown();
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @SkipMutations(
        reason = "This test uses DOMUtils.longPressNode() which is known"
        + " to be flaky under modified scaling factor, see crbug.com/40840940")
    public void testCopyLinkText() throws Throwable {
        int item = Item.COPY_LINK_TEXT;

        final String url = mWebServer.setResponse(FILE, DATA, null);
        loadUrlSync(url);
        DOMUtils.waitForNonZeroNodeBounds(mAwContents.getWebContents(), "testLink");

        DOMUtils.longPressNode(mAwContents.getWebContents(), "testLink");

        onView(withText(getTitle(mContext, item))).perform(click());

        Assert.assertEquals("Test Link", getClipBoardTextOnUiThread(mContext));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @SkipMutations(
        reason = "This test uses DOMUtils.longPressNode() which is known"
        + " to be flaky under modified scaling factor, see crbug.com/40840940")
    public void testCopyLinkURL() throws Throwable {
        int item = Item.COPY_LINK_ADDRESS;

        final String url = mWebServer.setResponse(FILE, DATA, null);
        loadUrlSync(url);
        DOMUtils.waitForNonZeroNodeBounds(mAwContents.getWebContents(), "testLink");

        DOMUtils.longPressNode(mAwContents.getWebContents(), "testLink");

        onView(withText(getTitle(mContext, item))).perform(click());

        assertStringContains("test_link.html", getClipBoardTextOnUiThread(mContext));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    @SkipMutations(
        reason = "This test uses DOMUtils.longPressNode() which is known"
        + " to be flaky under modified scaling factor, see crbug.com/40840940")
    public void testOpenInBrowser() throws Throwable {
        try {
            Intents.init();
            // Before triggering the viewing intent, stub it out to avoid cascading that into
            // further intents and opening the web browser.
            intending(IntentMatchers.hasAction(equalTo(Intent.ACTION_VIEW)))
                    .respondWith(new Instrumentation.ActivityResult(Activity.RESULT_OK, null));

            int item = Item.OPEN_IN_BROWSER;

            final String url = mWebServer.setResponse(FILE, DATA, null);
            loadUrlSync(url);
            DOMUtils.waitForNonZeroNodeBounds(mAwContents.getWebContents(), "testLink");

            DOMUtils.longPressNode(mAwContents.getWebContents(), "testLink");

            onView(withText(getTitle(mContext, item))).perform(click());

            intended(IntentMatchers.hasAction(equalTo(Intent.ACTION_VIEW)));
        } finally {
            Intents.release();
        }
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    @SkipMutations(
        reason = "This test uses DOMUtils.longPressNode() which is known"
        + " to be flaky under modified scaling factor, see crbug.com/40840940")
    public void testDismissContextMenuOnBack() throws Throwable {
        final String url = mWebServer.setResponse(FILE, DATA, null);
        loadUrlSync(url);
        DOMUtils.waitForNonZeroNodeBounds(mAwContents.getWebContents(), "testLink");

        DOMUtils.longPressNode(mAwContents.getWebContents(), "testLink");

        CriteriaHelper.pollUiThread(
                () -> {
                    return !mRule.getActivity().hasWindowFocus();
                },
                "Context menu did not have window focus");

        InstrumentationRegistry.getInstrumentation().sendKeyDownUpSync(KeyEvent.KEYCODE_BACK);
        CriteriaHelper.pollUiThread(
                () -> {
                    return mRule.getActivity().hasWindowFocus();
                },
                "Activity did not regain focus.");
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    @SkipMutations(
        reason = "This test uses DOMUtils.longPressNode() which is known"
        + " to be flaky under modified scaling factor, see crbug.com/40840940")
    public void testDismissContextMenuOnClick() throws Throwable {
        final String url = mWebServer.setResponse(FILE, DATA, null);
        loadUrlSync(url);
        DOMUtils.waitForNonZeroNodeBounds(mAwContents.getWebContents(), "testLink");

        DOMUtils.longPressNode(mAwContents.getWebContents(), "testLink");

        CriteriaHelper.pollUiThread(
                () -> {
                    return !mRule.getActivity().hasWindowFocus();
                },
                "Context menu did not have window focus");

        onView(withText(getTitle(mContext, Item.COPY_LINK_ADDRESS))).perform(click());

        CriteriaHelper.pollUiThread(
                () -> {
                    return mRule.getActivity().hasWindowFocus();
                },
                "Activity did not regain focus.");
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testBuildingContextMenuItems() throws Throwable {
        Integer[] expectedItems = {
            R.id.contextmenu_copy_link_text,
            R.id.contextmenu_copy_link_address,
            R.id.contextmenu_open_in_browser_id,
        };

        ContextMenuParams params =
                new ContextMenuParams(
                        0,
                        0,
                        new GURL("http://www.example.com/page_url"),
                        new GURL("http://www.example.com/other_example"),
                        "BLAH!",
                        GURL.emptyGURL(),
                        GURL.emptyGURL(),
                        "",
                        null,
                        false,
                        0,
                        0,
                        MenuSourceType.MENU_SOURCE_TOUCH,
                        false,
                        /* additionalNavigationParams= */ null);

        AwContextMenuItemDelegate itemDelegate =
                new AwContextMenuItemDelegate(
                        mRule.getActivity(), mAwContents.getWebContents(), params);

        AwContextMenuPopulator populator =
                new AwContextMenuPopulator(mContext, itemDelegate, params);

        List<Pair<Integer, ModelList>> contextMenuState = populator.buildContextMenu();

        ModelList items = contextMenuState.get(0).second;

        Integer[] actualItems = new Integer[items.size()];

        for (int i = 0; i < items.size(); i++) {
            actualItems[i] = populator.getMenuId(items.get(i).model);
        }

        Assert.assertArrayEquals(actualItems, expectedItems);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testHeaderHasURLText() throws Throwable {
        String expectedHeaderText = "http://www.testurl.com/first_page";

        ContextMenuParams params =
                new ContextMenuParams(
                        0,
                        0,
                        new GURL("http://www.example.com/page_url"),
                        GURL.emptyGURL(),
                        "BLAH!",
                        new GURL(expectedHeaderText),
                        GURL.emptyGURL(),
                        "",
                        null,
                        false,
                        0,
                        0,
                        MenuSourceType.MENU_SOURCE_TOUCH,
                        false,
                        /* additionalNavigationParams= */ null);

        AwContextMenuHeaderCoordinator headerCoordinator =
                new AwContextMenuHeaderCoordinator(params);

        String actualHeaderTitle = headerCoordinator.getTitle();

        Assert.assertEquals(actualHeaderTitle, expectedHeaderText);
    }

    private void loadUrlSync(String url) throws Exception {
        CallbackHelper done = mContentsClient.getOnPageCommitVisibleHelper();
        int callCount = done.getCallCount();
        mRule.loadUrlSync(
                mTestContainerView.getAwContents(), mContentsClient.getOnPageFinishedHelper(), url);
        done.waitForCallback(callCount);
    }

    private void assertStringContains(String subString, String superString) {
        Assert.assertTrue(
                "String '" + superString + "' does not contain '" + subString + "'",
                superString.contains(subString));
    }

    private String getTitle(Context context, @Item int item) {
        return AwContextMenuItem.getTitle(context, item).toString();
    }
}
