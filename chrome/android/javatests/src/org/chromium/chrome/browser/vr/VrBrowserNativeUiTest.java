// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import static org.chromium.chrome.browser.vr.XrTestFramework.NATIVE_URLS_OF_INTEREST;
import static org.chromium.chrome.browser.vr.XrTestFramework.PAGE_LOAD_TIMEOUT_S;
import static org.chromium.chrome.browser.vr.XrTestFramework.POLL_TIMEOUT_LONG_MS;
import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_VIEWER_DAYDREAM_OR_STANDALONE;

import android.graphics.PointF;
import android.os.SystemClock;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.payments.ui.PaymentRequestUI;
import org.chromium.chrome.browser.payments.ui.PaymentRequestUI.PaymentRequestObserverForTest;
import org.chromium.chrome.browser.vr.rules.ChromeTabbedActivityVrTestRule;
import org.chromium.chrome.browser.vr.util.NativeUiUtils;
import org.chromium.chrome.browser.vr.util.VrBrowserTransitionUtils;
import org.chromium.chrome.browser.vr.util.VrShellDelegateUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * End-to-end tests for native UI presentation in VR Browser mode.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, "enable-webvr"})
@Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM_OR_STANDALONE)
public class VrBrowserNativeUiTest {
    // We explicitly instantiate a rule here instead of using parameterization since this class
    // only ever runs in ChromeTabbedActivity.
    @Rule
    public ChromeTabbedActivityVrTestRule mVrTestRule = new ChromeTabbedActivityVrTestRule();

    private VrBrowserTestFramework mVrBrowserTestFramework;

    private static final String TEST_PAGE_2D_URL =
            VrBrowserTestFramework.getFileUrlForHtmlTestFile("test_navigation_2d_page");

    @Before
    public void setUp() throws Exception {
        mVrBrowserTestFramework = new VrBrowserTestFramework(mVrTestRule);
        VrBrowserTransitionUtils.forceEnterVrBrowserOrFail(POLL_TIMEOUT_LONG_MS);
    }

    /**
     * Tests that URLs are not shown for native UI.
     */
    @Test
    @MediumTest
    public void testUrlOnNativeUi()
            throws IllegalArgumentException, InterruptedException, TimeoutException {
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
    public void testUrlOnNonNativeUi()
            throws IllegalArgumentException, InterruptedException, TimeoutException {
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
    public void testPaymentRequest() throws InterruptedException {
        // We can't request payment on file:// URLs, so use a local server.
        EmbeddedTestServer server =
                EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        mVrBrowserTestFramework.loadUrlAndAwaitInitialization(
                server.getURL(VrBrowserTestFramework.getEmbeddedServerPathForHtmlTestFile(
                        "test_payment_request")),
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
        server.stopAndDestroyServer();
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
        NativeUiUtils.performActionAndWaitForVisibilityChange(
                UserFriendlyElementName.SUGGESTION_BOX,
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
        NativeUiUtils.performActionAndWaitForVisibilityChange(
                UserFriendlyElementName.SUGGESTION_BOX, () -> { NativeUiUtils.inputBackspace(); });
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
        NativeUiUtils.performActionAndWaitForVisibilityChange(
                UserFriendlyElementName.SUGGESTION_BOX,
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
        NativeUiUtils.performActionAndWaitForVisibilityChange(
                UserFriendlyElementName.SUGGESTION_BOX,
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
        NativeUiUtils.performActionAndWaitForVisibilityChange(
                UserFriendlyElementName.SUGGESTION_BOX,
                () -> { NativeUiUtils.inputString("chrome://v"); });
        NativeUiUtils.inputEnter();
        ChromeTabUtils.waitForTabPageLoaded(
                mVrTestRule.getActivity().getActivityTab(), "chrome://version/");
        // Navigate away from chrome://version/ so waitForTabPageLoaded doesn't no-op the next time.
        mVrTestRule.loadUrl("about:blank");

        // Test that autocompletion updates successfully when entering more text.
        NativeUiUtils.clickElementAndWaitForUiQuiescence(UserFriendlyElementName.URL, new PointF());
        NativeUiUtils.performActionAndWaitForVisibilityChange(
                UserFriendlyElementName.SUGGESTION_BOX,
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
        NativeUiUtils.performActionAndWaitForVisibilityChange(
                UserFriendlyElementName.SUGGESTION_BOX,
                () -> { NativeUiUtils.inputString("chrome://net-"); });
        NativeUiUtils.inputEnter();
        ChromeTabUtils.waitForTabPageLoaded(
                mVrTestRule.getActivity().getActivityTab(), "chrome://net-/");

        // Test that autocompletion cancels if a non-matching character is input.
        NativeUiUtils.clickElementAndWaitForUiQuiescence(UserFriendlyElementName.URL, new PointF());
        // This should autocomplete to "chrome://version".
        NativeUiUtils.performActionAndWaitForVisibilityChange(
                UserFriendlyElementName.SUGGESTION_BOX,
                () -> { NativeUiUtils.inputString("chrome://v"); });
        NativeUiUtils.inputString("a");
        NativeUiUtils.waitNumFrames(NativeUiUtils.NUM_FRAMES_FOR_SUGGESTION_UPDATE);
        NativeUiUtils.inputEnter();
        ChromeTabUtils.waitForTabPageLoaded(
                mVrTestRule.getActivity().getActivityTab(), "chrome://va/");
    }
}
