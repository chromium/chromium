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
import android.os.Build;
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
import org.chromium.android_webview.contextmenu.AwContextMenuCoordinator;
import org.chromium.android_webview.contextmenu.AwContextMenuHeaderCoordinator;
import org.chromium.android_webview.contextmenu.AwContextMenuHelper;
import org.chromium.android_webview.contextmenu.AwContextMenuItem;
import org.chromium.android_webview.contextmenu.AwContextMenuItem.Item;
import org.chromium.android_webview.contextmenu.AwContextMenuItemDelegate;
import org.chromium.android_webview.contextmenu.AwContextMenuPopulator;
import org.chromium.android_webview.test.AwActivityTestRule.TestDependencyFactory;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features;
import org.chromium.blink_public.common.ContextMenuDataMediaType;
import org.chromium.components.embedder_support.contextmenu.ContextMenuParams;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.TestTouchUtils;
import org.chromium.content_public.browser.test.util.WebContentsUtils;
import org.chromium.net.test.util.TestWebServer;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.mojom.MenuSourceType;
import org.chromium.url.GURL;

import java.util.List;
import java.util.concurrent.TimeoutException;

/** Tests for context menu methods */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
@Batch(Batch.PER_CLASS)
@Features.EnableFeatures({AwFeatures.WEBVIEW_HYPERLINK_CONTEXT_MENU})
public class ContextMenuTest extends AwParameterizedTest {
    /** Callback helper that notifies when the context menu is shown or attempted to be shown. */
    private static class OnContextMenuShownHelper extends CallbackHelper {
        private AwContextMenuCoordinator mCoordinator;

        public void notifyCalled(AwContextMenuCoordinator coordinator) {
            mCoordinator = coordinator;
            notifyCalled();
        }

        AwContextMenuCoordinator getContextMenuCoordinator() {
            assert getCallCount() > 0;
            return mCoordinator;
        }
    }

    private static final String FILE = "/main.html";
    private static final String DATA =
            "<html>"
                    + "<head>"
                    + "</head>"
                    + "<body>"
                    + "    <a href=\"test_link.html\" id=\"testLink\">Test Link</a>"
                    + "    <p> <img src=\"test_image.jpg\" id=\"testImage\"> </p>"
                    + "    <p> <video width=\"320\" height=\"240\" controls src=\"test_video.mp4\""
                    + "     id=\"testVideo\"></video> </p>"
                    + "    <p> <a href=\"test_link.html\" id=\"testLinkImage\">"
                    + "            <img src=\"test_image.jpg\" /> </a> </p>"
                    + "</body>"
                    + "</html>";

    @Rule public AwActivityTestRule mRule;

    private TestWebServer mWebServer;
    private AwTestContainerView mTestContainerView;
    private TestAwContentsClient mContentsClient;
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
            reason =
                    "This test uses DOMUtils.longPressNode() which is known"
                            + " to be flaky under modified scaling factor, see crbug.com/40840940")
    @DisableIf.Build(sdk_equals = Build.VERSION_CODES.Q, message = "crbug.com/391715753")
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
            reason =
                    "This test uses DOMUtils.longPressNode() which is known"
                            + " to be flaky under modified scaling factor, see crbug.com/40840940")
    @DisableIf.Build(sdk_equals = Build.VERSION_CODES.Q, message = "crbug.com/391715753")
    public void testCopyLinkURL() throws Throwable {
        int item = Item.COPY_LINK_ADDRESS;

        final String url = mWebServer.setResponse(FILE, DATA, null);
        loadUrlSync(url);

        DOMUtils.waitForNonZeroNodeBounds(mAwContents.getWebContents(), "testLink");
        DOMUtils.longPressNode(mAwContents.getWebContents(), "testLink");

        onView(withText(getTitle(mContext, item))).perform(click());
        assertStringContains("test_link.html", getClipBoardTextOnUiThread(mContext));

        // In a link with a nested image, copy link should copy the URL of the anchor link and
        // not the src of the image.
        DOMUtils.waitForNonZeroNodeBounds(mAwContents.getWebContents(), "testLinkImage");
        DOMUtils.longPressNode(mAwContents.getWebContents(), "testLinkImage");

        onView(withText(getTitle(mContext, item))).perform(click());
        assertStringContains("test_link.html", getClipBoardTextOnUiThread(mContext));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    @SkipMutations(
            reason =
                    "This test uses DOMUtils.longPressNode() which is known"
                            + " to be flaky under modified scaling factor, see crbug.com/40840940")
    @DisableIf.Build(sdk_equals = Build.VERSION_CODES.Q, message = "crbug.com/391715753")
    public void testOpenInBrowser() throws Throwable {
        try {
            Intents.init();
            // Before triggering the viewing intent, stub it out to avoid cascading that into
            // further intents and opening the web browser.
            intending(IntentMatchers.hasAction(equalTo(Intent.ACTION_VIEW)))
                    .respondWith(new Instrumentation.ActivityResult(Activity.RESULT_OK, null));

            int item = Item.OPEN_LINK;

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
            reason =
                    "This test uses DOMUtils.longPressNode() which is known"
                            + " to be flaky under modified scaling factor, see crbug.com/40840940")
    @DisableIf.Build(sdk_equals = Build.VERSION_CODES.Q, message = "crbug.com/391715753")
    public void testDismissContextMenuOnBack() throws Throwable {
        final String url = mWebServer.setResponse(FILE, DATA, null);
        loadUrlSync(url);

        Assert.assertTrue(
                "Context menu should be properly created for link elements",
                openContextMenuByJs("testLink"));

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
            reason =
                    "This test uses DOMUtils.longPressNode() which is known"
                            + " to be flaky under modified scaling factor, see crbug.com/40840940")
    @DisableIf.Build(sdk_equals = Build.VERSION_CODES.Q, message = "crbug.com/391715753")
    public void testDismissContextMenuOnClick() throws Throwable {
        final String url = mWebServer.setResponse(FILE, DATA, null);
        loadUrlSync(url);

        Assert.assertTrue(
                "Context menu should be properly created for link elements",
                openContextMenuByJs("testLink"));

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
    @MediumTest
    @Feature({"AndroidWebView"})
    @SkipMutations(
            reason =
                    "This test uses DOMUtils.longPressNode() which is known"
                            + " to be flaky under modified scaling factor, see crbug.com/40840940")
    @DisableIf.Build(sdk_equals = Build.VERSION_CODES.Q, message = "crbug.com/391715753")
    public void testDismissContextMenuOnOutsideTap() throws Throwable {
        final String url = mWebServer.setResponse(FILE, DATA, null);
        loadUrlSync(url);

        Assert.assertTrue(
                "Context menu should be properly created for link elements",
                openContextMenuByJs("testLink"));

        CriteriaHelper.pollUiThread(
                () -> {
                    return !mRule.getActivity().hasWindowFocus();
                },
                "Context menu did not have window focus");

        // Define a point near the top-left corner of the root view as the outside tap.
        final int tapX = mTestContainerView.getLeft() + 10;
        final int tapY = mTestContainerView.getTop() + 10;

        TestTouchUtils.singleClickView(
                InstrumentationRegistry.getInstrumentation(), mTestContainerView, tapX, tapY);

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
            R.id.contextmenu_copy_link_address,
            R.id.contextmenu_copy_link_text,
            R.id.contextmenu_open_link_id,
        };

        ContextMenuParams params =
                new ContextMenuParams(
                        0,
                        ContextMenuDataMediaType.NONE,
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
                        MenuSourceType.TOUCH,
                        false,
                        /* openedFromInterestTarget= */ false,
                        /* interestTargetNodeID= */ 0,
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
                        ContextMenuDataMediaType.NONE,
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
                        MenuSourceType.TOUCH,
                        false,
                        /* openedFromInterestTarget= */ false,
                        /* interestTargetNodeID= */ 0,
                        /* additionalNavigationParams= */ null);

        AwContextMenuHeaderCoordinator headerCoordinator =
                new AwContextMenuHeaderCoordinator(params);

        String actualHeaderTitle = headerCoordinator.getTitle();

        Assert.assertEquals(expectedHeaderText, actualHeaderTitle);
    }

    @Test
    @SmallTest
    @Feature("AndroidWebView")
    @SkipMutations(
            reason =
                    "This test uses DOMUtils.longPressNode() which is known"
                            + " to be flaky under modified scaling factor, see crbug.com/40840940")
    @DisableIf.Build(sdk_equals = Build.VERSION_CODES.Q, message = "crbug.com/391715753")
    public void testContextMenuNotDisplayedForImagesOrVideos() throws Throwable {
        String url = mWebServer.setResponse(FILE, DATA, null);
        loadUrlSync(url);

        // Long press on the image.
        Assert.assertFalse(
                "Context menu should not be created for an image",
                openContextMenuByJs("testImage"));

        // Check that the context menu is not displayed (Activity retains focus).
        CriteriaHelper.pollUiThread(
                () -> {
                    return mRule.getActivity().hasWindowFocus();
                },
                "Context menu should not have window focus on an image");

        // Long press on an anchor link with a nested image. The menu should be shown since this
        // is technically a link.
        Assert.assertTrue(
                "Context menu should be properly created for a link",
                openContextMenuByJs("testLinkImage"));

        CriteriaHelper.pollUiThread(
                () -> {
                    return !mRule.getActivity().hasWindowFocus();
                },
                "Context menu did not have window focus");

        // To dismiss it.
        onView(withText(getTitle(mContext, Item.COPY_LINK_ADDRESS))).perform(click());

        // Long press on the video.
        Assert.assertFalse(
                "Context menu should not be created for a video", openContextMenuByJs("testVideo"));

        CriteriaHelper.pollUiThread(
                () -> {
                    return mRule.getActivity().hasWindowFocus();
                },
                "Context menu should not have window focus on a video");
    }

    @Test
    @SmallTest
    @Feature("AndroidWebView")
    public void doNotShowContextMenuForNonLinkItems() {
        final ContextMenuParams params =
                new ContextMenuParams(
                        /* nativePtr= */ 0,
                        ContextMenuDataMediaType.NONE,
                        /* pageUrl= */ GURL.emptyGURL(),
                        /* linkUrl= */ GURL.emptyGURL(),
                        /* linkText= */ "Test link text",
                        /* unfilteredLinkUrl= */ GURL.emptyGURL(),
                        /* srcUrl= */ GURL.emptyGURL(),
                        /* titleText= */ "Test title",
                        /* referrer= */ null,
                        /* canSaveMedia= */ false,
                        /* triggeringTouchXDp= */ 0,
                        /* triggeringTouchYDp= */ 0,
                        /* sourceType= */ 0,
                        /* openedFromHighlight= */ false,
                        /* openedFromInterestTarget= */ false,
                        /* interestTargetNodeID= */ 0,
                        /* additionalNavigationParams= */ null);

        AwContextMenuHelper helper = AwContextMenuHelper.create(mAwContents.getWebContents());
        Assert.assertFalse(helper.showContextMenu(params, mTestContainerView));
    }

    private void loadUrlSync(String url) throws Exception {
        CallbackHelper done = mContentsClient.getOnPageCommitVisibleHelper();
        int callCount = done.getCallCount();
        mRule.loadUrlSync(
                mTestContainerView.getAwContents(), mContentsClient.getOnPageFinishedHelper(), url);
        done.waitForCallback(callCount);
        ThreadUtils.runOnUiThreadBlocking(
                () -> WebContentsUtils.simulateEndOfPaintHolding(mAwContents.getWebContents()));
    }

    private void assertStringContains(String subString, String superString) {
        Assert.assertTrue(
                "String '" + superString + "' does not contain '" + subString + "'",
                superString.contains(subString));
    }

    private String getTitle(Context context, @Item int item) {
        return AwContextMenuItem.getTitle(context, item).toString();
    }

    /**
     * Attempts to open a context menu by performing a long press on a DOM node.
     *
     * <p>It sets up a callback to listen for the context menu being shown (or an attempt to show
     * it) and then waits for that callback to be triggered.
     *
     * @param nodeId The node ID on the DOM to long press to open the context menu for.
     * @return True if a context menu was successfully shown, false otherwise.
     */
    private boolean openContextMenuByJs(String nodeId) throws TimeoutException {
        OnContextMenuShownHelper helper = new OnContextMenuShownHelper();
        AwContextMenuHelper.setMenuShownCallbackForTests(helper::notifyCalled);
        int callCount = helper.getCallCount();

        DOMUtils.waitForNonZeroNodeBounds(mAwContents.getWebContents(), nodeId);
        DOMUtils.longPressNode(mAwContents.getWebContents(), nodeId);

        helper.waitForCallback(callCount);
        return helper.getContextMenuCoordinator() != null;
    }
}
