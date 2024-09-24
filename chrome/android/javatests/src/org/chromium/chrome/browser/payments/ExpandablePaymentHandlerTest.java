// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withContentDescription;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import android.view.MotionEvent;
import android.view.View;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.uiautomator.UiDevice;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterProvider;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel.StateChangeReason;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.payments.handler.PaymentHandlerContentFrameLayout;
import org.chromium.chrome.browser.payments.handler.PaymentHandlerCoordinator;
import org.chromium.chrome.browser.payments.handler.PaymentHandlerCoordinator.PaymentHandlerUiObserver;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetTestSupport;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.payments.InputProtector;
import org.chromium.components.payments.test_support.FakeClock;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.TestTouchUtils;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.ServerCertificate;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.url.GURL;

import java.util.Arrays;
import java.util.List;

/** A test for the Expandable PaymentHandler {@link PaymentHandlerCoordinator}. */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class ExpandablePaymentHandlerTest {
    private static final long IGNORED_INPUT_DELAY =
            InputProtector.POTENTIALLY_UNINTENDED_INPUT_THRESHOLD - 100;
    private static final long SAFE_INPUT_DELAY =
            InputProtector.POTENTIALLY_UNINTENDED_INPUT_THRESHOLD;

    @Rule public ChromeTabbedActivityTestRule mRule = new ChromeTabbedActivityTestRule();

    // Host the tests on https://127.0.0.1, because file:// URLs cannot have service workers.
    private EmbeddedTestServer mServer;
    private boolean mUiShownCalled;
    private boolean mUiClosedCalled;
    private UiDevice mDevice;
    private ChromeActivity mDefaultActivity;
    private BottomSheetTestSupport mBottomSheetTestSupport;
    private FakeClock mClock;

    /** A list of bad server-certificates used for parameterized tests. */
    public static class BadCertParams implements ParameterProvider {
        @Override
        public List<ParameterSet> getParameters() {
            return Arrays.asList(
                    new ParameterSet()
                            .value(ServerCertificate.CERT_MISMATCHED_NAME)
                            .name("CERT_MISMATCHED_NAME"),
                    new ParameterSet().value(ServerCertificate.CERT_EXPIRED).name("CERT_EXPIRED"),
                    new ParameterSet()
                            .value(ServerCertificate.CERT_CHAIN_WRONG_ROOT)
                            .name("CERT_CHAIN_WRONG_ROOT"),
                    new ParameterSet()
                            .value(ServerCertificate.CERT_COMMON_NAME_ONLY)
                            .name("CERT_COMMON_NAME_ONLY"),
                    new ParameterSet()
                            .value(ServerCertificate.CERT_SHA1_LEAF)
                            .name("CERT_SHA1_LEAF"),
                    new ParameterSet()
                            .value(ServerCertificate.CERT_BAD_VALIDITY)
                            .name("CERT_BAD_VALIDITY"),
                    new ParameterSet()
                            .value(ServerCertificate.CERT_TEST_NAMES)
                            .name("CERT_TEST_NAMES"));
        }
    }

    /** A list of good server-certificates used for parameterized tests. */
    public static class GoodCertParams implements ParameterProvider {
        @Override
        public List<ParameterSet> getParameters() {
            return Arrays.asList(
                    new ParameterSet().value(ServerCertificate.CERT_OK).name("CERT_OK"),
                    new ParameterSet()
                            .value(ServerCertificate.CERT_COMMON_NAME_IS_DOMAIN)
                            .name("CERT_COMMON_NAME_IS_DOMAIN"),
                    new ParameterSet()
                            .value(ServerCertificate.CERT_OK_BY_INTERMEDIATE)
                            .name("CERT_OK_BY_INTERMEDIATE"),
                    new ParameterSet().value(ServerCertificate.CERT_AUTO).name("CERT_AUTO"));
        }
    }

    @Before
    public void setUp() throws Throwable {
        mRule.startMainActivityOnBlankPage();
        mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
        mDefaultActivity = mRule.getActivity();
        mBottomSheetTestSupport =
                new BottomSheetTestSupport(
                        mRule.getActivity()
                                .getRootUiCoordinatorForTesting()
                                .getBottomSheetController());
        mClock = new FakeClock();
    }

    private PaymentHandlerCoordinator createPaymentHandlerAndShow() throws Throwable {
        PaymentHandlerCoordinator paymentHandler = new PaymentHandlerCoordinator();
        paymentHandler.setInputProtectorForTest(new InputProtector(mClock));
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        paymentHandler.show(
                                mDefaultActivity.getCurrentWebContents(),
                                defaultPaymentAppUrl(),
                                defaultUiObserver()));
        return paymentHandler;
    }

    private String getOrigin(EmbeddedTestServer server) {
        String longOrigin = server.getURL("/");
        String begin = "https://";
        String end = "/";
        assert longOrigin.startsWith(begin);
        assert longOrigin.endsWith(end);
        return longOrigin.substring(begin.length(), longOrigin.length() - end.length());
    }

    private void startServer(@ServerCertificate int serverCertificate) {
        mServer =
                EmbeddedTestServer.createAndStartHTTPSServer(
                        ApplicationProvider.getApplicationContext(), serverCertificate);
    }

    private void startDefaultServer() {
        startServer(ServerCertificate.CERT_OK);
    }

    private GURL defaultPaymentAppUrl() {
        return new GURL(
                mServer.getURL(
                        "/components/test/data/payments/maxpay.test/payment_handler_window.html"));
    }

    private PaymentHandlerUiObserver defaultUiObserver() {
        return new PaymentHandlerUiObserver() {
            @Override
            public void onPaymentHandlerUiClosed() {
                mUiClosedCalled = true;
            }

            @Override
            public void onPaymentHandlerUiShown() {
                mUiShownCalled = true;
            }
        };
    }

    private void waitForUiShown() {
        CriteriaHelper.pollInstrumentationThread(() -> mUiShownCalled);
    }

    private void waitForTitleShown(WebContents paymentAppWebContents) {
        waitForTitleShown(paymentAppWebContents, "Max Pay");
    }

    private void waitForTitleShown(WebContents paymentAppWebContents, String title) {
        CriteriaHelper.pollInstrumentationThread(
                () -> paymentAppWebContents.getTitle().equals(title));
    }

    private void waitForUiClosed() {
        CriteriaHelper.pollInstrumentationThread(() -> mUiClosedCalled);
    }

    @Test
    @SmallTest
    @Feature({"Payments"})
    public void testOpenClose() throws Throwable {
        startDefaultServer();
        PaymentHandlerCoordinator paymentHandler = createPaymentHandlerAndShow();
        waitForUiShown();

        ThreadUtils.runOnUiThreadBlocking(() -> paymentHandler.hide());
        waitForUiClosed();
    }

    @Test
    @SmallTest
    @DisabledTest(message = "https://crbug.com/1191988")
    @Feature({"Payments"})
    public void testSwipeDownCloseUI() throws Throwable {
        startDefaultServer();
        createPaymentHandlerAndShow();

        waitForUiShown();

        View sheetControlContainer =
                mRule.getActivity().findViewById(R.id.bottom_sheet_control_container);
        int touchX = sheetControlContainer.getWidth() / 2;
        int startY = sheetControlContainer.getHeight() / 2;

        // Swipe past the end of the screen.
        int endY = mRule.getActivity().getResources().getDisplayMetrics().heightPixels + 100;

        TestTouchUtils.dragCompleteView(
                InstrumentationRegistry.getInstrumentation(),
                sheetControlContainer,
                touchX,
                touchX,
                startY,
                endY,
                20);

        waitForUiClosed();
    }

    @Test
    @SmallTest
    @Feature({"Payments"})
    public void testClickCloseButtonCloseUI() throws Throwable {
        startDefaultServer();
        createPaymentHandlerAndShow();
        waitForUiShown();

        mClock.advanceCurrentTimeMillis(SAFE_INPUT_DELAY);
        onView(withId(R.id.close)).perform(click());
        waitForUiClosed();
    }

    @Test
    @SmallTest
    @Feature({"Payments"})
    public void testCloseButtonInputProtection() throws Throwable {
        startDefaultServer();
        createPaymentHandlerAndShow();
        waitForUiShown();

        // Clicking close immediately is prevented.
        onView(withId(R.id.close)).perform(click());
        Assert.assertFalse(mUiClosedCalled);

        // Clicking close after an interval less than the threshold is still prevented.
        mClock.advanceCurrentTimeMillis(IGNORED_INPUT_DELAY);
        onView(withId(R.id.close)).perform(click());
        Assert.assertFalse(mUiClosedCalled);

        // Clicking close after the threshold is no longer prevented and closes the dialog.
        mClock.advanceCurrentTimeMillis(SAFE_INPUT_DELAY);
        onView(withId(R.id.close)).perform(click());
        waitForUiClosed();
    }

    @Test
    @SmallTest
    @Feature({"Payments"})
    public void testWebContentsInitializedCallbackInvoked() throws Throwable {
        startDefaultServer();
        PaymentHandlerCoordinator paymentHandler = createPaymentHandlerAndShow();
        waitForUiShown();

        ThreadUtils.runOnUiThreadBlocking(() -> paymentHandler.hide());
        waitForUiClosed();
    }

    @Test
    @SmallTest
    @Feature({"Payments"})
    public void testWebContentsDestroy() throws Throwable {
        startDefaultServer();
        PaymentHandlerCoordinator paymentHandler = createPaymentHandlerAndShow();
        waitForUiShown();

        Assert.assertFalse(paymentHandler.getWebContentsForTest().isDestroyed());
        ThreadUtils.runOnUiThreadBlocking(() -> paymentHandler.hide());
        waitForUiClosed();
        Assert.assertTrue(paymentHandler.getWebContentsForTest().isDestroyed());
    }

    @Test
    @SmallTest
    @Feature({"Payments"})
    public void testIncognitoTrue() throws Throwable {
        startDefaultServer();
        mRule.loadUrlInNewTab(UrlConstants.ABOUT_URL, true);
        PaymentHandlerCoordinator paymentHandler = createPaymentHandlerAndShow();
        waitForUiShown();

        Assert.assertTrue(paymentHandler.getWebContentsForTest().isIncognito());

        ThreadUtils.runOnUiThreadBlocking(() -> paymentHandler.hide());
        waitForUiClosed();
    }

    @Test
    @SmallTest
    @Feature({"Payments"})
    public void testIncognitoFalse() throws Throwable {
        startDefaultServer();
        PaymentHandlerCoordinator paymentHandler = createPaymentHandlerAndShow();
        waitForUiShown();

        Assert.assertFalse(paymentHandler.getWebContentsForTest().isIncognito());

        ThreadUtils.runOnUiThreadBlocking(() -> paymentHandler.hide());
        waitForUiClosed();
    }

    @Test
    @SmallTest
    @Feature({"Payments"})
    public void testUiElements() throws Throwable {
        startDefaultServer();
        PaymentHandlerCoordinator paymentHandler = createPaymentHandlerAndShow();
        waitForUiShown();

        onView(withId(R.id.bottom_sheet))
                .check(
                        matches(
                                withContentDescription(
                                        "Payment handler sheet. Swipe down to close.")));

        CriteriaHelper.pollInstrumentationThread(
                () -> paymentHandler.getWebContentsForTest().getTitle().equals("Max Pay"));

        onView(withId(R.id.title))
                .check(matches(isDisplayed()))
                .check(matches(withText("Max Pay")));
        onView(withId(R.id.bottom_sheet))
                .check(matches(isDisplayed()))
                .check(
                        matches(
                                withContentDescription(
                                        "Payment handler sheet. Swipe down to close.")));
        onView(withId(R.id.close))
                .check(matches(isDisplayed()))
                .check(matches(withContentDescription("Close")));
        onView(withId(R.id.security_icon))
                .check(matches(isDisplayed()))
                .check(matches(withContentDescription("Connection is secure")));
        onView(withId(R.id.origin))
                .check(matches(isDisplayed()))
                .check(matches(withText(getOrigin(mServer))));

        ThreadUtils.runOnUiThreadBlocking(() -> paymentHandler.hide());

        waitForUiClosed();
    }

    @Test
    @SmallTest
    @Feature({"Payments"})
    @DisabledTest(message = "https://crbug.com/1491094")
    public void testWebContentsInputProtection() throws Throwable {
        startDefaultServer();
        PaymentHandlerCoordinator paymentHandler = createPaymentHandlerAndShow();
        waitForUiShown();

        CallbackHelper callbackHelper = new CallbackHelper();
        WebContentsObserver observer =
                new WebContentsObserver() {
                    @Override
                    public void frameReceivedUserActivation() {
                        callbackHelper.notifyCalled();
                    }
                };
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    paymentHandler.getWebContentsForTest().addObserver(observer);
                });

        DOMUtils.waitForNonZeroNodeBounds(paymentHandler.getWebContentsForTest(), "confirmButton");
        // Before advancing the clock, input is intercepted from interacting with the page.
        PaymentHandlerContentFrameLayout contentLayout =
                (PaymentHandlerContentFrameLayout)
                        mRule.getActivity().findViewById(R.id.payment_handler_content);
        Assert.assertTrue(
                contentLayout.onInterceptTouchEvent(MotionEvent.obtain(0, 0, 0, 0, 0, 0)));
        Assert.assertTrue(
                DOMUtils.clickNode(paymentHandler.getWebContentsForTest(), "confirmButton"));
        Assert.assertEquals(0, callbackHelper.getCallCount());

        mClock.advanceCurrentTimeMillis(SAFE_INPUT_DELAY);
        Assert.assertFalse(
                contentLayout.onInterceptTouchEvent(MotionEvent.obtain(0, 0, 0, 0, 0, 0)));
        Assert.assertTrue(
                DOMUtils.clickNode(paymentHandler.getWebContentsForTest(), "confirmButton"));
        callbackHelper.waitForOnly();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    paymentHandler.getWebContentsForTest().removeObserver(observer);
                    paymentHandler.hide();
                });
        waitForUiClosed();
    }

    @Test
    @SmallTest
    @Feature({"Payments"})
    @DisabledTest(message = "https://crbug.com/1382925")
    public void testOpenPageInfoDialog() throws Throwable {
        startDefaultServer();
        PaymentHandlerCoordinator paymentHandler = createPaymentHandlerAndShow();
        waitForTitleShown(paymentHandler.getWebContentsForTest());

        onView(withId(R.id.security_icon)).perform(click());

        String paymentAppUrl =
                mServer.getURL(
                        "/components/test/data/payments/maxpay.test/payment_handler_window.html");

        // The UI only shows a hostname by default. Expand to full URL.
        onView(withId(R.id.page_info_url_wrapper)).perform(click());
        onView(withId(R.id.page_info_url))
                .check(matches(isDisplayed()))
                .check(matches(withText(paymentAppUrl)));

        mDevice.pressBack();

        onView(withId(R.id.page_info_url)).check(doesNotExist());

        ThreadUtils.runOnUiThreadBlocking(() -> paymentHandler.hide());
        waitForUiClosed();
    }

    @Test
    @SmallTest
    @Feature({"Payments"})
    public void testNavigateBackWithSystemBackButton() throws Throwable {
        startDefaultServer();

        PaymentHandlerCoordinator paymentHandler = createPaymentHandlerAndShow();

        waitForTitleShown(paymentHandler.getWebContentsForTest(), "Max Pay");
        onView(withId(R.id.origin)).check(matches(withText(getOrigin(mServer))));

        String anotherUrl =
                mServer.getURL("/components/test/data/payments/bobpay.test/app1/index.html");
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        paymentHandler
                                .getWebContentsForTest()
                                .getNavigationController()
                                .loadUrl(new LoadUrlParams(anotherUrl)));
        waitForTitleShown(paymentHandler.getWebContentsForTest(), "Bob Pay 1");
        onView(withId(R.id.origin)).check(matches(withText(getOrigin(mServer))));

        // Press back button would navigate back if it has previous pages.
        mDevice.pressBack();
        waitForTitleShown(paymentHandler.getWebContentsForTest(), "Max Pay");
        onView(withId(R.id.origin)).check(matches(withText(getOrigin(mServer))));

        // Press back button would be no-op if it does not have any previous page.
        mDevice.pressBack();
        waitForTitleShown(paymentHandler.getWebContentsForTest(), "Max Pay");
        onView(withId(R.id.origin)).check(matches(withText(getOrigin(mServer))));

        ThreadUtils.runOnUiThreadBlocking(() -> paymentHandler.hide());
        waitForUiClosed();
    }

    @Test
    @SmallTest
    @Feature({"Payments"})
    @ParameterAnnotations.UseMethodParameter(BadCertParams.class)
    public void testInsecureConnectionNotShowUi(int badCertificate) throws Throwable {
        startServer(badCertificate);
        PaymentHandlerCoordinator paymentHandler = createPaymentHandlerAndShow();

        CriteriaHelper.pollInstrumentationThread(
                () -> paymentHandler.getWebContentsForTest().isDestroyed());

        waitForUiClosed();
    }

    @Test
    @SmallTest
    @Feature({"Payments"})
    @DisableIf.Device(DeviceFormFactor.TABLET) // https://crbug.com/1135547
    @ParameterAnnotations.UseMethodParameter(GoodCertParams.class)
    public void testSecureConnectionShowUi(int goodCertificate) throws Throwable {
        startServer(goodCertificate);
        PaymentHandlerCoordinator paymentHandler = createPaymentHandlerAndShow();
        waitForTitleShown(paymentHandler.getWebContentsForTest());

        onView(withId(R.id.security_icon))
                .check(matches(isDisplayed()))
                .check(matches(withContentDescription("Connection is secure")));

        ThreadUtils.runOnUiThreadBlocking(() -> paymentHandler.hide());
        waitForUiClosed();
    }

    @Test
    @SmallTest
    @Feature({"Payments"})
    public void testBottomSheetSuppressedFailsShow() {
        startDefaultServer();
        PaymentHandlerCoordinator paymentHandler = new PaymentHandlerCoordinator();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mBottomSheetTestSupport.suppressSheet(StateChangeReason.UNKNOWN);
                });
        // When the return value is null, the caller needs to hide() manually.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertNull(
                            paymentHandler.show(
                                    mDefaultActivity.getCurrentWebContents(),
                                    defaultPaymentAppUrl(),
                                    defaultUiObserver()));
                    // When the return value is null, the caller needs to hide() manually.
                    paymentHandler.hide();
                });
        waitForUiClosed();
    }
}
