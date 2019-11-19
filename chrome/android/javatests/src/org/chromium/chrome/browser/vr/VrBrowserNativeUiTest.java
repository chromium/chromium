// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import static org.chromium.chrome.browser.vr.XrTestFramework.NATIVE_URLS_OF_INTEREST;
import static org.chromium.chrome.browser.vr.XrTestFramework.PAGE_LOAD_TIMEOUT_S;
import static org.chromium.chrome.browser.vr.XrTestFramework.POLL_TIMEOUT_LONG_MS;
import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_VIEWER_DAYDREAM_OR_STANDALONE;

import android.graphics.PointF;
import android.graphics.RectF;
import android.os.SystemClock;
import android.support.test.filters.LargeTest;
import android.support.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.payments.ui.PaymentRequestUI;
import org.chromium.chrome.browser.payments.ui.PaymentRequestUI.PaymentRequestObserverForTest;
import org.chromium.chrome.browser.vr.rules.ChromeTabbedActivityVrTestRule;
import org.chromium.chrome.browser.vr.util.NativeUiUtils;
import org.chromium.chrome.browser.vr.util.RenderTestUtils;
import org.chromium.chrome.browser.vr.util.VrBrowserTransitionUtils;
import org.chromium.chrome.browser.vr.util.VrShellDelegateUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.RenderTestRule;
import org.chromium.net.test.ServerCertificate;

import java.io.IOException;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * End-to-end tests for native UI presentation in VR Browser mode.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.
Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, "enable-features=LogJsConsoleMessages"})
@Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM_OR_STANDALONE)
public class VrBrowserNativeUiTest {
    // We need to make sure the port is constant, otherwise the URL changes between test runs, which
    // is really bad for image diff tests. There's nothing special about this port other than that
    // it shouldn't be in use by anything.
    private static final int SERVER_PORT = 39558;

    // We explicitly instantiate a rule here instead of using parameterization since this class
    // only ever runs in ChromeTabbedActivity.
    @Rule
    public ChromeTabbedActivityVrTestRule mVrTestRule = new ChromeTabbedActivityVrTestRule();

    @Rule
    public RenderTestRule mRenderTestRule =
            new RenderTestRule("components/test/data/vr_browser_ui/render_tests");

    private VrBrowserTestFramework mVrBrowserTestFramework;

    private static final String TEST_PAGE_2D_URL =
            VrBrowserTestFramework.getFileUrlForHtmlTestFile("test_navigation_2d_page");

    @Before
    public void setUp() {
        mVrBrowserTestFramework = new VrBrowserTestFramework(mVrTestRule);
        VrBrowserTransitionUtils.forceEnterVrBrowserOrFail(POLL_TIMEOUT_LONG_MS);
    }

    /**
     * Tests that URLs are not shown for native UI.
     */
    @Test
    @MediumTest
    public void testUrlOnNativeUi() throws IllegalArgumentException {
        for (String url : NATIVE_URLS_OF_INTEREST) {
            mVrTestRule.loadUrl(url, PAGE_LOAD_TIMEOUT_S);
            Assert.assertFalse("URL is being shown for native page " + url,
                    TestVrShellDelegate.isDisplayingUrlForTesting());
        }
    }

    /**
     * Tests that URLs are shown for non-native UI.
     */
    @Test
    @MediumTest
    public void testUrlOnNonNativeUi() throws IllegalArgumentException {
        mVrTestRule.loadUrl(TEST_PAGE_2D_URL, PAGE_LOAD_TIMEOUT_S);
        Assert.assertTrue("URL is not being show for non-native page",
                TestVrShellDelegate.isDisplayingUrlForTesting());
    }

    /**
     * Tests that the Payment Request API is supressed in the VR Browser and its native UI does not
     * show. Automation of a manual test from https://crbug.com/862162.
     */
    @Test
    @MediumTest
    public void testPaymentRequest() {
        // We can't request payment on file:// URLs, so use a local server.
        mVrBrowserTestFramework.loadUrlAndAwaitInitialization(
                mVrBrowserTestFramework.getEmbeddedServerUrlForHtmlTestFile("test_payment_request"),
                PAGE_LOAD_TIMEOUT_S);
        // Set up an observer so we'll know if the payment request is shown.
        AtomicBoolean requestShown = new AtomicBoolean(false);
        PaymentRequestObserverForTest observer = new PaymentRequestObserverForTest() {
            @Override
            public void onPaymentRequestReadyForInput(PaymentRequestUI ui) {
                requestShown.set(true);
            }
            @Override
            public void onPaymentRequestReadyToPay(PaymentRequestUI ui) {}
            @Override
            public void onPaymentRequestSelectionChecked(PaymentRequestUI ui) {}
            @Override
            public void onPaymentRequestResultReady(PaymentRequestUI ui) {}
        };
        PaymentRequestUI.setPaymentRequestObserverForTest(observer);
        // Request payment and wait for the promise to auto-reject.
        mVrBrowserTestFramework.executeStepAndWait("stepRequestPayment()");
        // Ensure that the native UI wasn't shown even though the request was rejected.
        // Need to sleep for a bit in order to allow the UI to show if it's going to.
        SystemClock.sleep(1000);
        Assert.assertFalse("Native Payment Request UI was shown", requestShown.get());
        // Ensure we weren't somehow kicked out of VR from this.
        Assert.assertTrue("Payment request caused VR exit",
                VrShellDelegateUtils.getDelegateInstance().isVrEntryComplete());
        mVrBrowserTestFramework.endTest();
    }

    /**
     * Tests that the current URL is automatically populated when opening up the omnibox text
     * entry mode.
     */
    @Test
    @MediumTest
    public void testOmniboxContainsCurrentUrl() throws InterruptedException {
        // This is a really roundabout way of checking the automatically populated text in the
        // omnibox without having to add yet more native plumbing to read the current text.
        // The idea is that by deleting the last four characters, we should be left with just
        // "chrome://", so we can then input another chrome:// URL based off that. The only way
        // the second navigation will complete successfully is if the omnibox had "chrome://gpu/"
        // when we opened it.
        NativeUiUtils.enableMockedKeyboard();
        mVrTestRule.loadUrl("chrome://gpu/");
        NativeUiUtils.clickElementAndWaitForUiQuiescence(UserFriendlyElementName.URL, new PointF());
        // Click near the righthand side of the text input field so the cursor is at the end instead
        // of selecting the existing text.
        NativeUiUtils.clickElementAndWaitForUiQuiescence(
                UserFriendlyElementName.OMNIBOX_TEXT_FIELD, new PointF(0.4f, 0.0f));
        // Delete "gpu/".
        for (int i = 0; i < 4; ++i) {
            NativeUiUtils.inputBackspace();
        }
        // Wait for suggestions to change so that our navigation succeeds.
        NativeUiUtils.performActionAndWaitForVisibilityStatus(
                UserFriendlyElementName.SUGGESTION_BOX, true /* visible */,
                () -> { NativeUiUtils.inputString("version/"); });
        NativeUiUtils.inputEnter();
        ChromeTabUtils.waitForTabPageLoaded(
                mVrTestRule.getActivity().getActivityTab(), "chrome://version/");
    }

    /**
     * Tests that the the omnibox automatically scrolls the text when it is longer than the box
     * can fit.
     */
    @Test
    @MediumTest
    public void testOmniboxTextScrolls() throws InterruptedException {
        // This is a roundabout way of checking that the text scrolls. By inputting a long string,
        // clicking near the beginning and deleting a character, we can check that the text scrolled
        // by checking whether a character at the beginning of the string was deleted or not - if
        // the text scrolled, then a character later in the string should have been deleted.
        NativeUiUtils.enableMockedKeyboard();
        NativeUiUtils.clickElementAndWaitForUiQuiescence(UserFriendlyElementName.URL, new PointF());
        String expectedString = "chrome://aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
        NativeUiUtils.inputString(expectedString + "a");
        // Click near the lefthand side of the input field to place the cursor near the beginning
        // of the visible part of the string.
        NativeUiUtils.clickElementAndWaitForUiQuiescence(
                UserFriendlyElementName.OMNIBOX_TEXT_FIELD, new PointF(-0.45f, 0.0f));
        // We expect this to delete an "a" instead of anything in "chrome://". Do so and wait for
        // the suggestions to appear.
        NativeUiUtils.performActionAndWaitForVisibilityStatus(
                UserFriendlyElementName.SUGGESTION_BOX, true /* visible */,
                () -> { NativeUiUtils.inputBackspace(); });
        NativeUiUtils.inputEnter();
        // Navigating automatically appends a "/".
        ChromeTabUtils.waitForTabPageLoaded(
                mVrTestRule.getActivity().getActivityTab(), expectedString + "/");
    }

    /**
     * Tests that the omnibox automatically clears any text that gets input but not committed.
     */
    @Test
    @MediumTest
    public void testOmniboxClearsUncommittedText() throws InterruptedException {
        // Similar to testOmniboxContainsCurrentUrl, we check that the uncommitted text is not
        // present the second time we open the omnibox by deleting a number of characters from
        // the expected text and entering additional text.
        NativeUiUtils.enableMockedKeyboard();
        NativeUiUtils.clickElementAndWaitForUiQuiescence(UserFriendlyElementName.URL, new PointF());
        // Input text and close the omnibox without committing.
        NativeUiUtils.performActionAndWaitForVisibilityStatus(
                UserFriendlyElementName.SUGGESTION_BOX, true /* visible */,
                () -> { NativeUiUtils.inputString("chrome://version/"); });
        NativeUiUtils.clickElementAndWaitForUiQuiescence(
                UserFriendlyElementName.OMNIBOX_CLOSE_BUTTON, new PointF());
        NativeUiUtils.clickElementAndWaitForUiQuiescence(UserFriendlyElementName.URL, new PointF());
        // Click near the righthand side of the text input field so the cursor is at the end instead
        // of selecting the existing text.
        NativeUiUtils.clickElementAndWaitForUiQuiescence(
                UserFriendlyElementName.OMNIBOX_TEXT_FIELD, new PointF(0.4f, 0.0f));
        // Delete "about:blank".
        for (int i = 0; i < 11; ++i) {
            NativeUiUtils.inputBackspace();
        }
        NativeUiUtils.performActionAndWaitForVisibilityStatus(
                UserFriendlyElementName.SUGGESTION_BOX, true /* visibile */,
                () -> { NativeUiUtils.inputString("chrome://version/"); });
        NativeUiUtils.inputEnter();
        // This should only succeed if the original "chrome://version/" we input is not present -
        // if it is, we'll end up navigating to "chromechrome://version/".
        ChromeTabUtils.waitForTabPageLoaded(
                mVrTestRule.getActivity().getActivityTab(), "chrome://version/");
    }

    /**
     * Tests that omnibox autocompletion is functional.
     */
    @Test
    @MediumTest
    public void testOmniboxAutocompletion() throws InterruptedException {
        // At least with chrome:// URLs, autocompletion only kicks in when there's one valid option
        // left. So, test that:
        // 1. Autocompletion works if only one option is available.
        // 2. Autocompletion updates successfully if additional characters are added.
        // 3. Autocompletion doesn't work if multiple options are available.
        // 4. Autocompletion cancels if a non-matching character is input.
        NativeUiUtils.enableMockedKeyboard();
        // Test that autocompletion works with only one option left.
        NativeUiUtils.clickElementAndWaitForUiQuiescence(UserFriendlyElementName.URL, new PointF());
        // This should autocomplete to "chrome://version".
        NativeUiUtils.performActionAndWaitForVisibilityStatus(
                UserFriendlyElementName.SUGGESTION_BOX, true /* visible */,
                () -> { NativeUiUtils.inputString("chrome://v"); });
        NativeUiUtils.inputEnter();
        ChromeTabUtils.waitForTabPageLoaded(
                mVrTestRule.getActivity().getActivityTab(), "chrome://version/");
        // Navigate away from chrome://version/ so waitForTabPageLoaded doesn't no-op the next time.
        mVrTestRule.loadUrl("about:blank");

        // Test that autocompletion updates successfully when entering more text.
        NativeUiUtils.clickElementAndWaitForUiQuiescence(UserFriendlyElementName.URL, new PointF());
        NativeUiUtils.performActionAndWaitForVisibilityStatus(
                UserFriendlyElementName.SUGGESTION_BOX, true /* visible */,
                () -> { NativeUiUtils.inputString("chrome://v"); });
        NativeUiUtils.inputString("e");
        // Since suggestions are already visible, we need to wait for suggestions to update via
        // a glorified sleep instead of waiting for suggestions to appear.
        NativeUiUtils.waitNumFrames(NativeUiUtils.NUM_FRAMES_FOR_SUGGESTION_UPDATE);
        NativeUiUtils.inputEnter();
        ChromeTabUtils.waitForTabPageLoaded(
                mVrTestRule.getActivity().getActivityTab(), "chrome://version/");

        // Test that autocompletion doesn't work with more than one option available.
        NativeUiUtils.clickElementAndWaitForUiQuiescence(UserFriendlyElementName.URL, new PointF());
        // This could be either "chrome://net-export" or "chrome://net-internals", so it shouldn't
        // autocomplete to anything.
        NativeUiUtils.performActionAndWaitForVisibilityStatus(
                UserFriendlyElementName.SUGGESTION_BOX, true /* visible */,
                () -> { NativeUiUtils.inputString("chrome://net-"); });
        NativeUiUtils.inputEnter();
        ChromeTabUtils.waitForTabPageLoaded(
                mVrTestRule.getActivity().getActivityTab(), "chrome://net-/");

        // Test that autocompletion cancels if a non-matching character is input.
        NativeUiUtils.clickElementAndWaitForUiQuiescence(UserFriendlyElementName.URL, new PointF());
        // This should autocomplete to "chrome://version".
        NativeUiUtils.performActionAndWaitForVisibilityStatus(
                UserFriendlyElementName.SUGGESTION_BOX, true /* visible */,
                () -> { NativeUiUtils.inputString("chrome://v"); });
        NativeUiUtils.inputString("a");
        NativeUiUtils.waitNumFrames(NativeUiUtils.NUM_FRAMES_FOR_SUGGESTION_UPDATE);
        NativeUiUtils.inputEnter();
        ChromeTabUtils.waitForTabPageLoaded(
                mVrTestRule.getActivity().getActivityTab(), "chrome://va/");
    }

    /**
     * Tests that the keyboard appears when clicking on the URL bar.
     * Also contains a regression test for https://crbug.com/874671 where inputting text into the
     * URL bar would cause a browser crash.
     */
    @Test
    @LargeTest
    @Feature({"Browser", "RenderTest"})
    public void testKeyboardAppearsOnUrlBarClick() throws InterruptedException, IOException {
        NativeUiUtils.clickElementAndWaitForUiQuiescence(UserFriendlyElementName.URL, new PointF());
        // For whatever reason, the laser has a lot of random noise (not visible to an actual user)
        // when the keyboard is present on certain OS/hardware configurations (currently known to
        // happen on Pixel XL w/ N). So, allow pixels to differ by a small amount without failing.
        mRenderTestRule.setPixelDiffThreshold(5);
        RenderTestUtils.dumpAndCompare(NativeUiUtils.FRAME_BUFFER_SUFFIX_BROWSER_UI,
                "keyboard_visible_browser_ui", mRenderTestRule);
        // Regression test for https://crbug.com/874671
        // We need to use the VrCore-side emulated controller because the keyboard isn't a UI
        // element, meaning we can't specify it as a click target for the Chrome-side controller.
        NativeUiUtils.revertToRealInput();
        // Point at the keyboard and click an arbitrary key
        EmulatedVrController controller = new EmulatedVrController(mVrTestRule.getActivity());
        controller.recenterView();
        controller.moveControllerInstant(0.0f, -0.259f, -0.996f, -0.0f);
        // Spam clicks to ensure we're getting one in.
        for (int i = 0; i < 5; i++) {
            controller.performControllerClick();
        }
    }

    /**
     * Tests that the overflow menu appears when the overflow menu button is clicked.
     */
    @Test
    @LargeTest
    @Feature({"Browser", "RenderTest"})
    public void testOverflowMenuAppears() throws InterruptedException, IOException {
        // TODO(https://crbug.com/930840): Remove this when the weird gradient behavior is fixed.
        mRenderTestRule.setPixelDiffThreshold(2);
        NativeUiUtils.clickElementAndWaitForUiQuiescence(
                UserFriendlyElementName.OVERFLOW_MENU, new PointF());
        RenderTestUtils.dumpAndCompare(NativeUiUtils.FRAME_BUFFER_SUFFIX_BROWSER_UI,
                "overflow_menu_visible_browser_ui", mRenderTestRule);
    }

    /**
     * Tests that data URLs have the data portion of the URL emphasized like in 2D browsing.
     */
    @Test
    @LargeTest
    @Feature({"Browser", "RenderTest"})
    public void testDataUrlEmphasis() throws InterruptedException, IOException {
        NativeUiUtils.enableMockedInput();
        mVrTestRule.loadUrl("data:,Hello%2C%20World!", PAGE_LOAD_TIMEOUT_S);
        NativeUiUtils.waitForUiQuiescence();
        RenderTestUtils.dumpAndCompare(NativeUiUtils.FRAME_BUFFER_SUFFIX_BROWSER_UI,
                "data_url_emphasis_browser_ui", mRenderTestRule);
    }

    /**
     * Tests that file URLs have the entire URL emphasized like in 2D browsing.
     */
    @Test
    @LargeTest
    @Feature({"Browser", "RenderTest"})
    public void testFileUrlEmphasis() throws InterruptedException, IOException {
        NativeUiUtils.enableMockedInput();
        mVrTestRule.loadUrl(VrBrowserTestFramework.getFileUrlForHtmlTestFile("2d_permission_page"),
                PAGE_LOAD_TIMEOUT_S);
        NativeUiUtils.waitForUiQuiescence();
        RenderTestUtils.dumpAndCompare(NativeUiUtils.FRAME_BUFFER_SUFFIX_BROWSER_UI,
                "file_url_emphasis_browser_ui", mRenderTestRule);
    }

    /**
     * Tests that the reposition bar does not appear if the keyboard is open.
     */
    @Test
    @MediumTest
    @Feature({"Browser", "RenderTest"})
    public void testRepositionBarDoesNotAppearWithKeyboardOpen()
            throws InterruptedException, TimeoutException, IOException {
        // Use the mock keyboard so it doesn't show, reducing the chance of flakes due to AA.
        NativeUiUtils.enableMockedKeyboard();
        mVrTestRule.loadUrl(
                VrBrowserTestFramework.getFileUrlForHtmlTestFile("generic_text_entry_page"),
                PAGE_LOAD_TIMEOUT_S);
        NativeUiUtils.clickContentNode(
                "textfield", new PointF(), 1 /* numClicks */, mVrBrowserTestFramework);
        NativeUiUtils.waitForUiQuiescence();
        NativeUiUtils.hoverElement(
                UserFriendlyElementName.CONTENT_QUAD, NativeUiUtils.REPOSITION_BAR_COORDINATES);
        NativeUiUtils.waitForUiQuiescence();
        // Due to the way the repositioner works, the reposition bar is technically always visible
        // in the element hierarchy, so we can't just assert that it's invisible. Instead, we have
        // to resort to pixel diffing.
        RenderTestUtils.dumpAndCompare(NativeUiUtils.FRAME_BUFFER_SUFFIX_BROWSER_UI,
                "reposition_bar_keyboard_open", mRenderTestRule);
    }

    /*
     * Tests that hovering over various elements in the URL bar looks as expected.
     */
    @Test
    @LargeTest
    @Feature({"Browser", "RenderTest"})
    public void testUrlBarHovering() throws InterruptedException, IOException {
        testUrlBarHoveringImpl(false);
    }

    /**
     * Tests that hovering over various elements in the URL bar looks as expected while in Incognito
     * mode.
     */
    @Test
    @LargeTest
    @Feature({"Browser", "RenderTest"})
    public void testUrlBarHoveringIncognito() throws InterruptedException, IOException {
        mVrBrowserTestFramework.openIncognitoTab("about:blank");
        testUrlBarHoveringImpl(true);
    }

    private void testUrlBarHoveringImpl(boolean incognito)
            throws InterruptedException, IOException {
        // Back button hovering doesn't do anything unless the back button is actually active. so
        // navigate to do that.
        mVrTestRule.loadUrl("chrome://version/", PAGE_LOAD_TIMEOUT_S);
        // Back button.
        NativeUiUtils.hoverElement(UserFriendlyElementName.BACK_BUTTON, new PointF());
        NativeUiUtils.waitForUiQuiescence();
        RenderTestUtils.dumpAndCompare(NativeUiUtils.FRAME_BUFFER_SUFFIX_BROWSER_UI,
                generateRenderTestIdentifier("back_button_hover", incognito), mRenderTestRule);
        // Security icon.
        NativeUiUtils.hoverElement(UserFriendlyElementName.PAGE_INFO_BUTTON, new PointF());
        NativeUiUtils.waitForUiQuiescence();
        RenderTestUtils.dumpAndCompare(NativeUiUtils.FRAME_BUFFER_SUFFIX_BROWSER_UI,
                generateRenderTestIdentifier("security_icon_hover", incognito), mRenderTestRule);
        // URL bar.
        NativeUiUtils.hoverElement(UserFriendlyElementName.URL, new PointF());
        NativeUiUtils.waitForUiQuiescence();
        RenderTestUtils.dumpAndCompare(NativeUiUtils.FRAME_BUFFER_SUFFIX_BROWSER_UI,
                generateRenderTestIdentifier("url_bar_hover", incognito), mRenderTestRule);
        // Overflow menu.
        NativeUiUtils.hoverElement(UserFriendlyElementName.OVERFLOW_MENU, new PointF());
        NativeUiUtils.waitForUiQuiescence();
        RenderTestUtils.dumpAndCompare(NativeUiUtils.FRAME_BUFFER_SUFFIX_BROWSER_UI,
                generateRenderTestIdentifier("overflow_menu_hover", incognito), mRenderTestRule);
    }

    /**
     * Tests that hovering over various elements in the overflow menu looks as expected.
     */
    @Test
    @LargeTest
    @Feature({"Browser", "RenderTest"})
    public void testOverflowMenuHovering() throws InterruptedException, IOException {
        testOverflowMenuHoveringImpl(false);
    }

    /**
     * Tests that hovering over various elements in the overflow menu looks as expected while in
     * Incognito mode.
     */
    @Test
    @LargeTest
    @Feature({"Browser", "RenderTest"})
    public void testOverflowMenuHoveringIncognito() throws InterruptedException, IOException {
        mVrBrowserTestFramework.openIncognitoTab("about:blank");
        testOverflowMenuHoveringImpl(true);
    }

    private void testOverflowMenuHoveringImpl(boolean incognito)
            throws InterruptedException, IOException {
        // TODO(https://crbug.com/930840): Remove this when the weird gradient behavior is fixed.
        mRenderTestRule.setPixelDiffThreshold(2);
        // The forward button only has a hover state if the button is actually active, so navigate
        // a bit.
        mVrTestRule.loadUrl("chrome://version/", PAGE_LOAD_TIMEOUT_S);
        VrBrowserTransitionUtils.navigateBack();
        ChromeTabUtils.waitForTabPageLoaded(
                mVrTestRule.getActivity().getActivityTab(), "about:blank");
        // Make the overflow menu appear.
        NativeUiUtils.performActionAndWaitForVisibilityStatus(
                UserFriendlyElementName.RELOAD_BUTTON, true /* visible */, () -> {
                    NativeUiUtils.clickElement(UserFriendlyElementName.OVERFLOW_MENU, new PointF());
                });
        // Reload button.
        NativeUiUtils.hoverElement(UserFriendlyElementName.RELOAD_BUTTON, new PointF());
        NativeUiUtils.waitForUiQuiescence();
        RenderTestUtils.dumpAndCompare(NativeUiUtils.FRAME_BUFFER_SUFFIX_BROWSER_UI,
                generateRenderTestIdentifier("reload_button_hover", incognito), mRenderTestRule);
        // Forward Button.
        NativeUiUtils.hoverElement(UserFriendlyElementName.FORWARD_BUTTON, new PointF());
        NativeUiUtils.waitForUiQuiescence();
        RenderTestUtils.dumpAndCompare(NativeUiUtils.FRAME_BUFFER_SUFFIX_BROWSER_UI,
                generateRenderTestIdentifier("forward_button_hover", incognito), mRenderTestRule);
        // New Incognito tab button/close Incognito tabs button.
        if (incognito) {
            NativeUiUtils.hoverElement(UserFriendlyElementName.CLOSE_INCOGNITO_TABS, new PointF());
            NativeUiUtils.waitForUiQuiescence();
            RenderTestUtils.dumpAndCompare(NativeUiUtils.FRAME_BUFFER_SUFFIX_BROWSER_UI,
                    generateRenderTestIdentifier("close_incognito_tabs_hover", incognito),
                    mRenderTestRule);
        } else {
            NativeUiUtils.hoverElement(UserFriendlyElementName.NEW_INCOGNITO_TAB, new PointF());
            NativeUiUtils.waitForUiQuiescence();
            RenderTestUtils.dumpAndCompare(NativeUiUtils.FRAME_BUFFER_SUFFIX_BROWSER_UI,
                    generateRenderTestIdentifier("new_incognito_tab_hover", incognito),
                    mRenderTestRule);
        }
    }

    private String generateRenderTestIdentifier(String name, boolean incognito) {
        return name + (incognito ? "_incognito" : "") + "_browser_ui";
    }

    /**
     * Tests that highlighting suggestions looks correct and that clicking just outside of the
     * suggestion doesn't trigger its onclick. Regression test for https://crbug.com/799593.
     */
    @Test
    @MediumTest
    @Feature({"Browser", "RenderTest"})
    public void testSuggestionHovering() throws InterruptedException, IOException {
        // Input some text to get suggestions.
        NativeUiUtils.enableMockedKeyboard();
        NativeUiUtils.clickElementAndWaitForUiQuiescence(UserFriendlyElementName.URL, new PointF());
        NativeUiUtils.performActionAndWaitForVisibilityStatus(
                UserFriendlyElementName.SUGGESTION_BOX, true /* visible */,
                () -> { NativeUiUtils.inputString("chrome://"); });

        // We need to crop the image before comparing to avoid the blinking cursor in the omnibox.
        // So, crop roughly around the suggestion box. This tends to chop off the bottom half of
        // the bottom suggestion on larger devices, but that's preferable to accidentally getting
        // the omnibox in the image, and should still be sufficient to catch the intended issues
        // (hover states, clicks actually registering).
        final RectF cropBounds = new RectF(0.1f, 0.4f, 0.6f, 0.625f);

        // There should be three suggestions, so hover the top then the middle one to ensure that
        // the hover effect properly moves between the two.
        NativeUiUtils.hoverElement(UserFriendlyElementName.SUGGESTION_BOX, new PointF(0.0f, 0.3f));
        NativeUiUtils.waitForUiQuiescence();
        RenderTestUtils.dumpAndCompareWithCrop(NativeUiUtils.FRAME_BUFFER_SUFFIX_BROWSER_UI,
                "suggestion_hovering_top", cropBounds, mRenderTestRule);
        NativeUiUtils.hoverElement(UserFriendlyElementName.SUGGESTION_BOX, new PointF());
        NativeUiUtils.waitForUiQuiescence();
        RenderTestUtils.dumpAndCompareWithCrop(NativeUiUtils.FRAME_BUFFER_SUFFIX_BROWSER_UI,
                "suggestion_hovering_middle", cropBounds, mRenderTestRule);

        // Ensure that the hover effect disappears when slightly to the right of the suggestion and
        // that clicking doesn't do anything.
        NativeUiUtils.clickElementAndWaitForUiQuiescence(
                UserFriendlyElementName.SUGGESTION_BOX, new PointF(0.51f, 0.0f));
        RenderTestUtils.dumpAndCompareWithCrop(NativeUiUtils.FRAME_BUFFER_SUFFIX_BROWSER_UI,
                "suggestion_clicking_right", cropBounds, mRenderTestRule);
        // Again on the left side.
        NativeUiUtils.hoverElement(UserFriendlyElementName.SUGGESTION_BOX, new PointF());
        NativeUiUtils.clickElementAndWaitForUiQuiescence(
                UserFriendlyElementName.SUGGESTION_BOX, new PointF(-0.51f, 0.0f));
        RenderTestUtils.dumpAndCompareWithCrop(NativeUiUtils.FRAME_BUFFER_SUFFIX_BROWSER_UI,
                "suggestion_clicking_left", cropBounds, mRenderTestRule);
        // Again above the top suggestion.
        NativeUiUtils.hoverElement(UserFriendlyElementName.SUGGESTION_BOX, new PointF(0.0f, 0.3f));
        NativeUiUtils.clickElementAndWaitForUiQuiescence(
                UserFriendlyElementName.SUGGESTION_BOX, new PointF(0.0f, 0.51f));
        RenderTestUtils.dumpAndCompareWithCrop(NativeUiUtils.FRAME_BUFFER_SUFFIX_BROWSER_UI,
                "suggestion_clicking_top", cropBounds, mRenderTestRule);
        // Again below the bottom suggestion.
        NativeUiUtils.hoverElement(UserFriendlyElementName.SUGGESTION_BOX, new PointF(0.0f, -0.3f));
        // For some reason, we have to aim slightly more offset than in other directions in order
        // to not actually hit the suggestion (probably due to the way we calculate where to click
        // not taking into account where the controller is, so hit testing can produce a slightly
        // different result).
        NativeUiUtils.clickElementAndWaitForUiQuiescence(
                UserFriendlyElementName.SUGGESTION_BOX, new PointF(0.0f, -0.55f));
        RenderTestUtils.dumpAndCompareWithCrop(NativeUiUtils.FRAME_BUFFER_SUFFIX_BROWSER_UI,
                "suggestion_clicking_bottom", cropBounds, mRenderTestRule);
    }

    private void resizeContentWindowToMinimum() throws InterruptedException {
        NativeUiUtils.selectRepositionBar();
        NativeUiUtils.scrollFling(NativeUiUtils.ScrollDirection.DOWN);
        // We need to ensure that the scroll has finished, but we can't use waitForUiQuiescence()
        // because the UI is never quiescent while the reposition bar is being used. So, wait a
        // suitable number of frames.
        NativeUiUtils.waitNumFrames(2 * NativeUiUtils.NUM_STEPS_FLING_SCROLL);
        NativeUiUtils.deselectRepositionBar();
        NativeUiUtils.waitForUiQuiescence();
    }

    /**
     * Tests that scrolling while holding the reposition bar causes the content window to be
     * resized and that the resize doesn't affect the dimensions reported to the webpage.
     */
    @Test
    @MediumTest
    @Feature({"Browser", "RenderTest"})
    public void testScrollResizing() throws InterruptedException, IOException {
        mVrBrowserTestFramework.loadUrlAndAwaitInitialization(
                VrBrowserTestFramework.getFileUrlForHtmlTestFile(
                        "test_content_resizing_does_not_affect_webpage"),
                PAGE_LOAD_TIMEOUT_S);
        mVrBrowserTestFramework.executeStepAndWait("stepGetInitialDimensions()");
        resizeContentWindowToMinimum();
        RenderTestUtils.dumpAndCompare(
                NativeUiUtils.FRAME_BUFFER_SUFFIX_BROWSER_UI, "scroll_resizing", mRenderTestRule);
        mVrBrowserTestFramework.executeStepAndWait("stepCheckDimensionsAfterResize()");
        mVrBrowserTestFramework.endTest();
    }

    /**
     * Tests that the overflow menu and keyboard properly follow the content quad when it is
     * repositioned.
     */
    @Test
    @MediumTest
    @Feature({"Browser", "RenderTest"})
    public void testOverflowAndKeyboardFollowContentQuad()
            throws InterruptedException, TimeoutException, IOException {
        mVrTestRule.loadUrl(
                VrBrowserTestFramework.getFileUrlForHtmlTestFile("generic_text_entry_page"),
                PAGE_LOAD_TIMEOUT_S);
        // Drag the content quad up and to the left.
        NativeUiUtils.selectRepositionBar();
        NativeUiUtils.hoverElement(UserFriendlyElementName.CONTENT_QUAD, new PointF(-0.5f, 1.0f));
        NativeUiUtils.deselectRepositionBar();
        // Click coordinates are determined when we queue the command, not when it's actually
        // executed, so ensure the quad has moved before attempting to click.
        NativeUiUtils.waitForUiQuiescence();
        NativeUiUtils.clickElementAndWaitForUiQuiescence(
                UserFriendlyElementName.OVERFLOW_MENU, new PointF());
        RenderTestUtils.dumpAndCompare(NativeUiUtils.FRAME_BUFFER_SUFFIX_BROWSER_UI,
                "repositioned_overflow_menu", mRenderTestRule);
        NativeUiUtils.clickElementAndWaitForUiQuiescence(
                UserFriendlyElementName.OVERFLOW_MENU, new PointF());
        NativeUiUtils.clickContentNode(
                "textfield", new PointF(), 1 /* numClicks */, mVrBrowserTestFramework);
        NativeUiUtils.waitForUiQuiescence();
        RenderTestUtils.dumpAndCompare(NativeUiUtils.FRAME_BUFFER_SUFFIX_BROWSER_UI,
                "repositioned_keyboard", mRenderTestRule);
    }

    /**
     * Tests that window and WebXR rAFs continue to fire while repositioning the content quad.
     */
    @Test
    @MediumTest
    @CommandLineFlags.Add({"enable-features=WebXR"})
    public void testRAFsFireWhileRepositioning() {
        mVrBrowserTestFramework.loadUrlAndAwaitInitialization(
                VrBrowserTestFramework.getFileUrlForHtmlTestFile(
                        "test_rafs_fire_while_repositioning"),
                PAGE_LOAD_TIMEOUT_S);
        NativeUiUtils.selectRepositionBar();
        mVrBrowserTestFramework.executeStepAndWait("stepCheckForRafs()");
        mVrBrowserTestFramework.endTest();
    }

    /**
     * Tests that the reposition bar is not active while a permission prompt is displayed.
     */
    @Test
    @MediumTest
    @Feature({"Browser", "RenderTest"})
    public void testRepositionBarDoesNotAppearWithPermissionPromptVisible()
            throws InterruptedException, IOException {
        // We don't need to actually accept the prompt, so we don't need to use the local server.
        mVrBrowserTestFramework.loadUrlAndAwaitInitialization(
                VrBrowserTestFramework.getFileUrlForHtmlTestFile("2d_permission_page"),
                PAGE_LOAD_TIMEOUT_S);
        NativeUiUtils.enableMockedInput();
        NativeUiUtils.waitForUiQuiescence();
        NativeUiUtils.performActionAndWaitForUiQuiescence(() -> {
            NativeUiUtils.performActionAndWaitForVisibilityStatus(
                    UserFriendlyElementName.BROWSING_DIALOG, true /* visible */, () -> {
                        mVrBrowserTestFramework.runJavaScriptOrFail(
                                "navigator.getUserMedia({audio: true}, onGranted, onDenied)",
                                POLL_TIMEOUT_LONG_MS);
                    });
        });
        NativeUiUtils.hoverElement(
                UserFriendlyElementName.CONTENT_QUAD, NativeUiUtils.REPOSITION_BAR_COORDINATES);
        NativeUiUtils.waitForUiQuiescence();
        RenderTestUtils.dumpAndCompare(NativeUiUtils.FRAME_BUFFER_SUFFIX_BROWSER_UI,
                "reposition_bar_permission_prompt_open", mRenderTestRule);
    }

    /**
     * Tests that pressing the app button on the Daydream controller exists voice input mode. This
     * would fit better in VrBrowserControllerInputTest, but having it here allows code reuse.
     */
    @Test
    @MediumTest
    public void testAppButtonExitsVoiceInput() throws InterruptedException {
        Runnable exitAction = () -> {
            NativeUiUtils.clickAppButton(UserFriendlyElementName.CURRENT_POSITION, new PointF());
        };
        testExitVoiceInputImpl(exitAction);
    }

    /**
     * Tests that pressing the voice input exit button exits voice input mode.
     */
    @Test
    @MediumTest
    public void testVoiceInputExitButtonExitsVoiceInput() throws InterruptedException {
        Runnable exitAction = () -> {
            // We have to wait a bit for the button to actually become clickable, but we can't
            // wait of UI quiescence since the microphone icon is constantly animating. So, wait
            // a set number of frames.
            NativeUiUtils.waitNumFrames(30);
            NativeUiUtils.clickElement(
                    UserFriendlyElementName.VOICE_INPUT_CLOSE_BUTTON, new PointF());
        };
        testExitVoiceInputImpl(exitAction);
    }

    private void testExitVoiceInputImpl(final Runnable exitAction) throws InterruptedException {
        NativeUiUtils.enableMockedKeyboard();
        NativeUiUtils.performActionAndWaitForVisibilityStatus(
                UserFriendlyElementName.OMNIBOX_VOICE_INPUT_BUTTON, true /* visible */,
                () -> { NativeUiUtils.clickElement(UserFriendlyElementName.URL, new PointF()); });

        // Input a valid URL. Wait for suggestions to appear as an indicator that text entry has
        // completed.
        NativeUiUtils.performActionAndWaitForVisibilityStatus(
                UserFriendlyElementName.SUGGESTION_BOX, true /* visible */,
                () -> { NativeUiUtils.inputString("chrome://version/"); });

        // Click the voice input button and wait until we're in voice input mode.
        NativeUiUtils.performActionAndWaitForVisibilityStatus(
                UserFriendlyElementName.VOICE_INPUT_CLOSE_BUTTON, true /* visible */, () -> {
                    NativeUiUtils.clickElement(
                            UserFriendlyElementName.OMNIBOX_VOICE_INPUT_BUTTON, new PointF());
                });

        // Do whatever exit action we're testing and wait for the omnibox to come back.
        NativeUiUtils.performActionAndWaitForVisibilityStatus(
                UserFriendlyElementName.OMNIBOX_TEXT_FIELD, true /* visible */,
                () -> { exitAction.run(); });

        // Ensure that the omnibox text was not cleared by going to voice input by navigating.
        NativeUiUtils.inputEnter();
        ChromeTabUtils.waitForTabPageLoaded(
                mVrTestRule.getActivity().getActivityTab(), "chrome://version/");
    }

    /**
     * Tests that the voice input button is not visible if the webpage has the microphone
     * permission.
     */
    @Test
    @MediumTest
    public void testVoiceInputUnavailableIfSiteUsingMicrophone() throws InterruptedException {
        NativeUiUtils.enableMockedInput();
        mVrBrowserTestFramework.loadUrlAndAwaitInitialization(
                mVrBrowserTestFramework.getEmbeddedServerUrlForHtmlTestFile("2d_permission_page"),
                PAGE_LOAD_TIMEOUT_S);
        // Wait for any residual animations from navigating.
        NativeUiUtils.waitForUiQuiescence();
        // Display and accept the permission.
        NativeUiUtils.performActionAndWaitForUiQuiescence(() -> {
            NativeUiUtils.performActionAndWaitForVisibilityStatus(
                    UserFriendlyElementName.BROWSING_DIALOG, true /* visible */, () -> {
                        mVrBrowserTestFramework.runJavaScriptOrFail(
                                "navigator.getUserMedia({audio: true}, onGranted, onDenied)",
                                POLL_TIMEOUT_LONG_MS);
                    });
        });
        NativeUiUtils.clickFallbackUiPositiveButton();
        mVrBrowserTestFramework.pollJavaScriptBooleanOrFail(
                "lastPermissionGranted === true", POLL_TIMEOUT_LONG_MS);

        // Click on the URL bar, wait a for the omnibox to appear, and assert that the voice input
        // button is not next to it like it normally is.
        NativeUiUtils.performActionAndWaitForVisibilityStatus(
                UserFriendlyElementName.OMNIBOX_TEXT_FIELD, true /* visible */,
                () -> { NativeUiUtils.clickElement(UserFriendlyElementName.URL, new PointF()); });
        NativeUiUtils.performActionAndWaitForVisibilityStatus(
                UserFriendlyElementName.OMNIBOX_VOICE_INPUT_BUTTON, false /* visible */, () -> {});
    }

    /**
     * Tests that the security token and page info popup look correct on HTTP sites.
     */
    @Test
    @MediumTest
    @Feature({"Browser", "RenderTest"})
    public void testSecurityTokenOnHttp() throws InterruptedException, IOException {
        mVrTestRule.getEmbeddedTestServerRule().setServerPort(SERVER_PORT);
        testSecurityTokenImpl("security_token_http");
    }

    /**
     * Tests that the security token and page info popup look correct on HTTPS sites.
     */
    @Test
    @MediumTest
    @Feature({"Browser", "RenderTest"})
    public void testSecurityTokenOnHttps() throws InterruptedException, IOException {
        mVrTestRule.getEmbeddedTestServerRule().setServerPort(SERVER_PORT);
        mVrTestRule.getEmbeddedTestServerRule().setServerUsesHttps(true);
        testSecurityTokenImpl("security_token_https");
    }

    /**
     * Tests that the security token and page info popup look correct on HTTPS sites when the
     * server provides a bad certificate.
     */
    @Test
    @MediumTest
    @Feature({"Browser", "RenderTest"})
    public void testSecurityTokenOnHttpsBadCertificate() throws InterruptedException, IOException {
        mVrTestRule.getEmbeddedTestServerRule().setServerPort(SERVER_PORT);
        mVrTestRule.getEmbeddedTestServerRule().setServerUsesHttps(true);
        mVrTestRule.getEmbeddedTestServerRule().setCertificateType(ServerCertificate.CERT_EXPIRED);
        testSecurityTokenImpl("security_token_https_bad_cert");
    }

    private void testSecurityTokenImpl(String identifier) throws InterruptedException, IOException {
        NativeUiUtils.enableMockedInput();
        mVrTestRule.loadUrl(
                mVrBrowserTestFramework.getEmbeddedServerUrlForHtmlTestFile("2d_permission_page"),
                PAGE_LOAD_TIMEOUT_S);
        // Wait for any residual animations from loading to go away.
        NativeUiUtils.waitForUiQuiescence();
        RenderTestUtils.dumpAndCompare(NativeUiUtils.FRAME_BUFFER_SUFFIX_BROWSER_UI,
                identifier + "_url_bar_token", mRenderTestRule);
        // Make the content window as small as possible to hide as much of it behind the upcoming
        // popup. See https://crbug.com/947117.
        // TODO(https://crbug.com/947117): Remove the resizing once the interstitials are no longer
        // offset slightly vertically at random.
        resizeContentWindowToMinimum();
        NativeUiUtils.clickElementAndWaitForUiQuiescence(
                UserFriendlyElementName.PAGE_INFO_BUTTON, new PointF());
        // Workaround for https://crbug.com/893291, where the text doesn't actually show up until a
        // bit after the element is drawn.
        SystemClock.sleep(2000);
        RenderTestUtils.dumpAndCompare(NativeUiUtils.FRAME_BUFFER_SUFFIX_BROWSER_UI,
                identifier + "_popup", mRenderTestRule);
    }
}
