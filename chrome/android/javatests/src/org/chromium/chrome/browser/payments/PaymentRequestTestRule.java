// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.os.Handler;
import android.view.KeyEvent;
import android.view.View;
import android.view.inputmethod.EditorInfo;
import android.widget.EditText;
import android.widget.Spinner;
import android.widget.TextView;

import androidx.annotation.IntDef;

import org.hamcrest.Matchers;
import org.junit.Assert;

import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.CriteriaNotSatisfiedException;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.CardUnmaskPrompt;
import org.chromium.chrome.browser.autofill.CardUnmaskPrompt.CardUnmaskObserverForTest;
import org.chromium.chrome.browser.autofill.editors.EditorObserverForTest;
import org.chromium.chrome.browser.payments.ChromePaymentRequestFactory.ChromePaymentRequestDelegateImpl;
import org.chromium.chrome.browser.payments.ChromePaymentRequestFactory.ChromePaymentRequestDelegateImplObserverForTest;
import org.chromium.chrome.browser.payments.ui.PaymentRequestSection.OptionSection;
import org.chromium.chrome.browser.payments.ui.PaymentRequestSection.OptionSection.OptionRow;
import org.chromium.chrome.browser.payments.ui.PaymentRequestUI;
import org.chromium.chrome.browser.payments.ui.PaymentRequestUI.PaymentRequestObserverForTest;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.payments.InputProtector;
import org.chromium.components.payments.PayerData;
import org.chromium.components.payments.PaymentApp;
import org.chromium.components.payments.PaymentAppFactoryDelegate;
import org.chromium.components.payments.PaymentAppFactoryInterface;
import org.chromium.components.payments.PaymentAppService;
import org.chromium.components.payments.PaymentRequestService;
import org.chromium.components.payments.PaymentRequestService.PaymentRequestServiceObserverForTest;
import org.chromium.components.payments.test_support.FakeClock;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.payments.mojom.PaymentDetailsModifier;
import org.chromium.payments.mojom.PaymentItem;
import org.chromium.payments.mojom.PaymentMethodData;
import org.chromium.payments.mojom.PaymentOptions;
import org.chromium.payments.mojom.PaymentShippingOption;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.UUID;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicReference;

/** Custom ActivityTestRule for integration test for payments. */
/*package*/ class PaymentRequestTestRule extends ChromeTabbedActivityTestRule
        implements PaymentRequestObserverForTest,
                PaymentRequestServiceObserverForTest,
                ChromePaymentRequestDelegateImplObserverForTest,
                CardUnmaskObserverForTest,
                EditorObserverForTest {
    private static final long SAFE_INPUT_DELAY =
            InputProtector.POTENTIALLY_UNINTENDED_INPUT_THRESHOLD;

    @IntDef({AppPresence.NO_APPS, AppPresence.HAVE_APPS})
    @Retention(RetentionPolicy.SOURCE)
    /* package */ @interface AppPresence {
        /** Flag for a factory without payment apps. */
        static final int NO_APPS = 0;

        /** Flag for a factory with payment apps. */
        static final int HAVE_APPS = 1;
    }

    @IntDef({AppSpeed.FAST_APP, AppSpeed.SLOW_APP})
    @Retention(RetentionPolicy.SOURCE)
    /* package */ @interface AppSpeed {
        /** Flag for installing a payment app that responds to its invocation fast. */
        static final int FAST_APP = 0;

        /** Flag for installing a payment app that responds to its invocation slowly. */
        static final int SLOW_APP = 1;
    }

    @IntDef({FactorySpeed.FAST_FACTORY, FactorySpeed.SLOW_FACTORY})
    @Retention(RetentionPolicy.SOURCE)
    /* package */ @interface FactorySpeed {
        /** Flag for a factory that immediately creates a payment app. */
        static final int FAST_FACTORY = 0;

        /** Flag for a factory that creates a payment app with a delay. */
        static final int SLOW_FACTORY = 1;
    }

    /** The expiration month dropdown index for December. */
    /* package */ static final int DECEMBER = 11;

    /** The expiration year dropdown index for the next year. */
    /* package */ static final int NEXT_YEAR = 1;

    /**
     * The billing address dropdown index for the first billing address. Index 0 is for the "Select"
     * hint.
     */
    /* package */ static final int FIRST_BILLING_ADDRESS = 1;

    /** Command line flag to enable experimental web platform features in tests. */
    /* package */ static final String ENABLE_EXPERIMENTAL_WEB_PLATFORM_FEATURES =
            "enable-experimental-web-platform-features";

    private final PaymentsCallbackHelper<PaymentRequestUI> mShowCalled;
    private final PaymentsCallbackHelper<PaymentRequestUI> mReadyForInput;
    private final PaymentsCallbackHelper<PaymentRequestUI> mReadyToPay;
    private final PaymentsCallbackHelper<PaymentRequestUI> mSelectionChecked;
    private final PaymentsCallbackHelper<PaymentRequestUI> mResultReady;
    private final PaymentsCallbackHelper<CardUnmaskPrompt> mReadyForUnmaskInput;
    private final PaymentsCallbackHelper<CardUnmaskPrompt> mReadyToUnmask;
    private final PaymentsCallbackHelper<CardUnmaskPrompt> mUnmaskValidationDone;
    private final PaymentsCallbackHelper<CardUnmaskPrompt> mSubmitRejected;
    private final CallbackHelper mReadyToEdit;
    private final CallbackHelper mEditorValidationError;
    private final CallbackHelper mEditorTextUpdate;
    private final CallbackHelper mDismissed;
    private final CallbackHelper mUnableToAbort;
    private final CallbackHelper mBillingAddressChangeProcessed;
    private final CallbackHelper mShowFailed;
    private final CallbackHelper mCanMakePaymentQueryResponded;
    private final CallbackHelper mHasEnrolledInstrumentQueryResponded;
    private final CallbackHelper mExpirationMonthChange;
    private final CallbackHelper mPaymentResponseReady;
    private final CallbackHelper mCompleteHandled;
    private final CallbackHelper mRendererClosedMojoConnection;
    private ChromePaymentRequestDelegateImpl mChromePaymentRequestDelegateImpl;
    private PaymentRequestUI mUI;
    private FakeClock mClock;
    private InputProtector mInputProtector;

    private final boolean mDelayStartActivity;
    private boolean mAutoAdvanceInputProtectorClock;

    private final AtomicReference<WebContents> mWebContentsRef;

    private final String mTestFilePath;

    private CardUnmaskPrompt mCardUnmaskPrompt;

    /**
     * Creates an instance of PaymentRequestTestRule.
     *
     * @param testFileName The file name of an test page in //components/test/data/payments,
     *     'about:blank', or a data url which starts with 'data:'.
     */
    /* package */ PaymentRequestTestRule(String testFileName) {
        this(testFileName, false);
    }

    /**
     * Creates an instance of PaymentRequestTestRule.
     *
     * @param testFileName The file name of an test page in //components/test/data/payments,
     *     'about:blank', or a data url which starts with 'data:'.
     * @param delayStartActivity Whether to delay the start of the main activity. When true, {@link
     *     #startMainActivityWithURL()} needs to be called to start the main activity; otherwise,
     *     the main activity would start automatically.
     */
    /* package */ PaymentRequestTestRule(String testFileName, boolean delayStartActivity) {
        this(testFileName, /* pathPrefix= */ "components/test/data/payments/", delayStartActivity);
    }

    /**
     * Creates an instance of PaymentRequestTestRule with a test page, which is specified by
     * pathPrefix and testFileName combined into a path relative to the repository root. For
     * example, if testFileName is "merchant.html", pathPrefix is "components/test/data/payments/",
     * the method would look for a test page at "components/test/data/payments/merchant.html".
     *
     * @param testFileName The file name of the test page.
     * @param pathPrefix The prefix path to testFileName.
     * @param delayStartActivity Whether to delay the start of the main activity.
     */
    private PaymentRequestTestRule(
            String testFilePath, String pathPrefix, boolean delayStartActivity) {
        super();
        mShowCalled = new PaymentsCallbackHelper<>();
        mReadyForInput = new PaymentsCallbackHelper<>();
        mReadyToPay = new PaymentsCallbackHelper<>();
        mSelectionChecked = new PaymentsCallbackHelper<>();
        mResultReady = new PaymentsCallbackHelper<>();
        mReadyForUnmaskInput = new PaymentsCallbackHelper<>();
        mReadyToUnmask = new PaymentsCallbackHelper<>();
        mUnmaskValidationDone = new PaymentsCallbackHelper<>();
        mSubmitRejected = new PaymentsCallbackHelper<>();
        mReadyToEdit = new CallbackHelper();
        mEditorValidationError = new CallbackHelper();
        mEditorTextUpdate = new CallbackHelper();
        mDismissed = new CallbackHelper();
        mUnableToAbort = new CallbackHelper();
        mBillingAddressChangeProcessed = new CallbackHelper();
        mExpirationMonthChange = new CallbackHelper();
        mPaymentResponseReady = new CallbackHelper();
        mShowFailed = new CallbackHelper();
        mCanMakePaymentQueryResponded = new CallbackHelper();
        mHasEnrolledInstrumentQueryResponded = new CallbackHelper();
        mCompleteHandled = new CallbackHelper();
        mRendererClosedMojoConnection = new CallbackHelper();
        mWebContentsRef = new AtomicReference<>();
        if (testFilePath.equals("about:blank") || testFilePath.startsWith("data:")) {
            mTestFilePath = testFilePath;
        } else {
            mTestFilePath = UrlUtils.getIsolatedTestFilePath(pathPrefix + testFilePath);
        }
        mDelayStartActivity = delayStartActivity;
        mAutoAdvanceInputProtectorClock = true;
        mClock = new FakeClock();
        mInputProtector = new InputProtector(mClock);
    }

    /* package */ void setObserversAndWaitForInitialPageLoad() throws TimeoutException {
        try {
            // TODO(crbug.com/40728764): Figure out what these tests need to wait on to not be flaky
            // instead of sleeping.
            Thread.sleep(2000);
        } catch (Exception ex) {
        }
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mWebContentsRef.set(getActivity().getCurrentWebContents());
                    PaymentRequestUI.setEditorObserverForTest(PaymentRequestTestRule.this);
                    PaymentRequestUI.setPaymentRequestObserverForTest(PaymentRequestTestRule.this);
                    PaymentRequestService.setObserverForTest(PaymentRequestTestRule.this);
                    ChromePaymentRequestFactory.setChromePaymentRequestDelegateImplObserverForTest(
                            PaymentRequestTestRule.this);
                    CardUnmaskPrompt.setObserverForTest(PaymentRequestTestRule.this);
                });
        assertWaitForPageScaleFactorMatch(0.5f);
    }

    /* package */ PaymentsCallbackHelper<PaymentRequestUI> getShowCalled() {
        return mShowCalled;
    }

    /* package */ PaymentsCallbackHelper<PaymentRequestUI> getReadyForInput() {
        return mReadyForInput;
    }

    /* package */ PaymentsCallbackHelper<PaymentRequestUI> getReadyToPay() {
        return mReadyToPay;
    }

    /* package */ PaymentsCallbackHelper<PaymentRequestUI> getSelectionChecked() {
        return mSelectionChecked;
    }

    /* package */ PaymentsCallbackHelper<PaymentRequestUI> getResultReady() {
        return mResultReady;
    }

    /* package */ PaymentsCallbackHelper<CardUnmaskPrompt> getReadyForUnmaskInput() {
        return mReadyForUnmaskInput;
    }

    /* package */ PaymentsCallbackHelper<CardUnmaskPrompt> getReadyToUnmask() {
        return mReadyToUnmask;
    }

    /* package */ PaymentsCallbackHelper<CardUnmaskPrompt> getUnmaskValidationDone() {
        return mUnmaskValidationDone;
    }

    /* package */ PaymentsCallbackHelper<CardUnmaskPrompt> getSubmitRejected() {
        return mSubmitRejected;
    }

    /* package */ CallbackHelper getReadyToEdit() {
        return mReadyToEdit;
    }

    /* package */ CallbackHelper getEditorValidationError() {
        return mEditorValidationError;
    }

    /* package */ CallbackHelper getEditorTextUpdate() {
        return mEditorTextUpdate;
    }

    /* package */ CallbackHelper getDismissed() {
        return mDismissed;
    }

    /* package */ CallbackHelper getUnableToAbort() {
        return mUnableToAbort;
    }

    /* package */ CallbackHelper getBillingAddressChangeProcessed() {
        return mBillingAddressChangeProcessed;
    }

    /* package */ CallbackHelper getShowFailed() {
        return mShowFailed;
    }

    /* package */ CallbackHelper getCanMakePaymentQueryResponded() {
        return mCanMakePaymentQueryResponded;
    }

    /* package */ CallbackHelper getHasEnrolledInstrumentQueryResponded() {
        return mHasEnrolledInstrumentQueryResponded;
    }

    /* package */ CallbackHelper getExpirationMonthChange() {
        return mExpirationMonthChange;
    }

    /* package */ CallbackHelper getPaymentResponseReady() {
        return mPaymentResponseReady;
    }

    /* package */ CallbackHelper getCompleteHandled() {
        return mCompleteHandled;
    }

    /* package */ CallbackHelper getRendererClosedMojoConnection() {
        return mRendererClosedMojoConnection;
    }

    /* package */ PaymentRequestUI getPaymentRequestUI() {
        return mUI;
    }

    /* package */ void triggerUIAndWait(
            String nodeId, PaymentsCallbackHelper<PaymentRequestUI> helper)
            throws TimeoutException {
        clickNodeAndWait(nodeId, helper);
    }

    /* package */ void retryPaymentRequest(String validationErrors, CallbackHelper helper)
            throws TimeoutException {
        int callCount = helper.getCallCount();
        JavaScriptUtils.executeJavaScriptAndWaitForResult(
                mWebContentsRef.get(), "retry(" + validationErrors + ");");
        helper.waitForCallback(callCount);
    }

    /**
     * Executes a snippet of JavaScript code in the current tab, and returns the result of the
     * snippet. The JavaScript code is run without a user gesture, and any async result (i.e.,
     * Promise) is not waited for.
     */
    /* package */ String executeJavaScriptAndWaitForResult(String script) throws TimeoutException {
        return JavaScriptUtils.executeJavaScriptAndWaitForResult(mWebContentsRef.get(), script);
    }

    /**
     * Executes a snippet of JavaScript code in the current tab, and waits for a given UI event to
     * occur. The JavaScript code is run with a user gesture present, and any async result (i.e.,
     * Promise) is not waited for.
     */
    /* package */ void runJavaScriptAndWaitForUIEvent(String code, CallbackHelper helper)
            throws TimeoutException {
        int callCount = helper.getCallCount();
        runJavaScriptCodeWithUserGestureInCurrentTab(code);
        helper.waitForCallback(callCount);
    }

    /**
     * Executes a snippet of JavaScript code in the current tab, and waits for the promise it
     * returns to settle with some value or to reject with an error message; returning the result as
     * a String in either case. The JavaScript code is run with a user gesture present.
     *
     * @param promiseCode a JavaScript snippet that will return a promise
     */
    /* package */ String runJavaScriptAndWaitForPromise(String promiseCode)
            throws TimeoutException {
        String code =
                promiseCode
                        + ".then(result => domAutomationController.send(result))"
                        + ".catch(error => domAutomationController.send(error));";
        return JavaScriptUtils.runJavascriptWithUserGestureAndAsyncResult(getWebContents(), code);
    }

    /** Clicks on an HTML node. */
    /* package */ void clickNodeAndWait(String nodeId, CallbackHelper helper)
            throws TimeoutException {
        int callCount = helper.getCallCount();
        clickNode(nodeId);
        helper.waitForCallback(callCount);
    }

    /** Clicks on an HTML node. */
    /* package */ void clickNode(String nodeId) throws TimeoutException {
        DOMUtils.waitForNonZeroNodeBounds(mWebContentsRef.get(), nodeId);
        DOMUtils.clickNode(mWebContentsRef.get(), nodeId);
    }

    /** Clicks on an element in the payments UI. */
    /* package */ void clickAndWait(int resourceId, CallbackHelper helper) throws TimeoutException {
        int callCount = helper.getCallCount();
        CriteriaHelper.pollUiThread(
                () -> {
                    boolean canClick = mUI.isAcceptingUserInput();
                    if (canClick) mUI.getDialogForTest().findViewById(resourceId).performClick();
                    Criteria.checkThat(canClick, Matchers.is(true));
                });
        helper.waitForCallback(callCount);
    }

    /** Clicks on an element in the "Order summary" section of the payments UI. */
    /* package */ void clickInOrderSummaryAndWait(CallbackHelper helper) throws TimeoutException {
        int callCount = helper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mUI.getOrderSummarySectionForTest()
                            .findViewById(R.id.payments_section)
                            .performClick();
                });
        helper.waitForCallback(callCount);
    }

    /** Clicks on an element in the "Shipping address" section of the payments UI. */
    /* package */ void clickInShippingAddressAndWait(final int resourceId, CallbackHelper helper)
            throws TimeoutException {
        int callCount = helper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mUI.getShippingAddressSectionForTest().findViewById(resourceId).performClick();
                });
        helper.waitForCallback(callCount);
    }

    /** Clicks on an element in the "Payment" section of the payments UI. */
    /* package */ void clickInPaymentMethodAndWait(final int resourceId, CallbackHelper helper)
            throws TimeoutException {
        int callCount = helper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mUI.getPaymentMethodSectionForTest().findViewById(resourceId).performClick();
                });
        helper.waitForCallback(callCount);
    }

    /** Clicks on an element in the "Contact Info" section of the payments UI. */
    /* package */ void clickInContactInfoAndWait(final int resourceId, CallbackHelper helper)
            throws TimeoutException {
        int callCount = helper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mUI.getContactDetailsSectionForTest().findViewById(resourceId).performClick();
                });
        helper.waitForCallback(callCount);
    }

    /** Clicks on an element in the editor UI. */
    /* package */ void clickInEditorAndWait(final int resourceId, CallbackHelper helper)
            throws TimeoutException {
        int callCount = helper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mUI.getEditorDialog().findViewById(resourceId).performClick();
                });
        helper.waitForCallback(callCount);
    }

    /* package */ void clickAndroidBackButtonInEditorAndWait(CallbackHelper helper)
            throws TimeoutException {
        int callCount = helper.getCallCount();
        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    mUI.getEditorDialog()
                            .dispatchKeyEvent(
                                    new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_BACK));
                    mUI.getEditorDialog()
                            .dispatchKeyEvent(
                                    new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_BACK));
                });
        helper.waitForCallback(callCount);
    }

    /** Clicks on a button in the card unmask UI. */
    /* package */ void clickCardUnmaskButtonAndWait(final int dialogButtonId, CallbackHelper helper)
            throws TimeoutException {
        int callCount = helper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PropertyModel model = mCardUnmaskPrompt.getDialogForTest();
                    model.get(ModalDialogProperties.CONTROLLER).onClick(model, dialogButtonId);
                });
        helper.waitForCallback(callCount);
    }

    /** Gets the retry error message. */
    /* package */ String getRetryErrorMessage() {
        return ThreadUtils.runOnUiThreadBlocking(
                () ->
                        ((TextView) mUI.getDialogForTest().findViewById(R.id.retry_error))
                                .getText()
                                .toString());
    }

    /** Gets the button state for the shipping summary section. */
    /* package */ int getShippingAddressSectionButtonState() {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> mUI.getShippingAddressSectionForTest().getEditButtonState());
    }

    /** Gets the button state for the contact details section. */
    /* package */ int getContactDetailsButtonState() {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> mUI.getContactDetailsSectionForTest().getEditButtonState());
    }

    /** Returns the label of the payment app at the specified |index|. */
    /* package */ String getPaymentAppLabel(final int index) {
        return ThreadUtils.runOnUiThreadBlocking(
                () ->
                        ((OptionSection) mUI.getPaymentMethodSectionForTest())
                                .getOptionLabelsForTest(index)
                                .getText()
                                .toString());
    }

    /** Returns the label of the selected payment app. */
    /* package */ String getSelectedPaymentAppLabel() {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    OptionSection section = ((OptionSection) mUI.getPaymentMethodSectionForTest());
                    int size = section.getNumberOfOptionLabelsForTest();
                    for (int i = 0; i < size; i++) {
                        if (section.getOptionRowAtIndex(i).isChecked()) {
                            return section.getOptionRowAtIndex(i).getLabelText().toString();
                        }
                    }
                    return null;
                });
    }

    /** Returns the total amount in order summary section. */
    /* package */ String getOrderSummaryTotal() {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> mUI.getOrderSummaryTotalTextViewForTest().getText().toString());
    }

    /** Returns the amount text corresponding to the line item at the specified |index|. */
    /* package */ String getLineItemAmount(int index) {
        return ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mUI.getOrderSummarySectionForTest()
                                .getLineItemAmountForTest(index)
                                .getText()
                                .toString()
                                .trim());
    }

    /** Returns the amount text corresponding to the line item at the specified |index|. */
    /* package */ int getNumberOfLineItems() {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> mUI.getOrderSummarySectionForTest().getNumberOfLineItemsForTest());
    }

    /**
     * Returns the label corresponding to the contact detail suggestion at the specified
     * |suggestionIndex|.
     */
    /* package */ String getContactDetailsSuggestionLabel(final int suggestionIndex) {
        return ThreadUtils.runOnUiThreadBlocking(
                () ->
                        ((OptionSection) mUI.getContactDetailsSectionForTest())
                                .getOptionLabelsForTest(suggestionIndex)
                                .getText()
                                .toString());
    }

    /** Returns the number of payment apps. */
    /* package */ int getNumberOfPaymentApps() {
        return ThreadUtils.runOnUiThreadBlocking(
                () ->
                        ((OptionSection) mUI.getPaymentMethodSectionForTest())
                                .getNumberOfOptionLabelsForTest());
    }

    /**
     * Returns the label corresponding to the payment method suggestion at the specified
     * |suggestionIndex|.
     */
    /* package */ String getPaymentMethodSuggestionLabel(final int suggestionIndex) {
        Assert.assertTrue(suggestionIndex < getNumberOfPaymentApps());

        return ThreadUtils.runOnUiThreadBlocking(
                () ->
                        ((OptionSection) mUI.getPaymentMethodSectionForTest())
                                .getOptionLabelsForTest(suggestionIndex)
                                .getText()
                                .toString());
    }

    /** Returns the number of contact detail suggestions. */
    /* package */ int getNumberOfContactDetailSuggestions() {
        return ThreadUtils.runOnUiThreadBlocking(
                () ->
                        ((OptionSection) mUI.getContactDetailsSectionForTest())
                                .getNumberOfOptionLabelsForTest());
    }

    /**
     * Returns the label corresponding to the shipping address suggestion at the specified
     * |suggestionIndex|.
     */
    /* package */ String getShippingAddressSuggestionLabel(final int suggestionIndex) {
        Assert.assertTrue(suggestionIndex < getNumberOfShippingAddressSuggestions());

        return ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mUI.getShippingAddressSectionForTest()
                                .getOptionLabelsForTest(suggestionIndex)
                                .getText()
                                .toString());
    }

    /* package */ String getShippingAddressSummary() {
        return ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mUI.getShippingAddressSectionForTest()
                                .getLeftSummaryLabelForTest()
                                .getText()
                                .toString());
    }

    /* package */ String getShippingOptionSummary() {
        return ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mUI.getShippingOptionSectionForTest()
                                .getLeftSummaryLabelForTest()
                                .getText()
                                .toString());
    }

    /* package */ String getShippingOptionCostSummaryOnBottomSheet() {
        return ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mUI.getShippingOptionSectionForTest()
                                .getRightSummaryLabelForTest()
                                .getText()
                                .toString());
    }

    /* package */ String getShippingAddressWarningLabel() {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    View view =
                            mUI.getShippingAddressSectionForTest()
                                    .findViewById(R.id.payments_warning_label);
                    return view != null && view instanceof TextView
                            ? ((TextView) view).getText().toString()
                            : null;
                });
    }

    /* package */ String getShippingAddressDescriptionLabel() {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    View view =
                            mUI.getShippingAddressSectionForTest()
                                    .findViewById(R.id.payments_description_label);
                    return view != null && view instanceof TextView
                            ? ((TextView) view).getText().toString()
                            : null;
                });
    }

    /**
     * Clicks on the label corresponding to the shipping address suggestion at the specified
     * |suggestionIndex|.
     */
    /* package */ void clickOnShippingAddressSuggestionOptionAndWait(
            final int suggestionIndex, CallbackHelper helper) throws TimeoutException {
        Assert.assertTrue(suggestionIndex < getNumberOfShippingAddressSuggestions());

        int callCount = helper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ((OptionSection) mUI.getShippingAddressSectionForTest())
                            .getOptionLabelsForTest(suggestionIndex)
                            .performClick();
                });
        helper.waitForCallback(callCount);
    }

    /**
     * Clicks on the label corresponding to the payment method suggestion at the specified
     * |suggestionIndex|.
     */
    /* package */ void clickOnPaymentMethodSuggestionOptionAndWait(
            final int suggestionIndex, CallbackHelper helper) throws TimeoutException {
        Assert.assertTrue(suggestionIndex < getNumberOfPaymentApps());

        int callCount = helper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ((OptionSection) mUI.getPaymentMethodSectionForTest())
                            .getOptionLabelsForTest(suggestionIndex)
                            .performClick();
                });
        helper.waitForCallback(callCount);
    }

    /**
     * Clicks on the label corresponding to the contact info suggestion at the specified
     * |suggestionIndex|.
     */
    /* package */ void clickOnContactInfoSuggestionOptionAndWait(
            final int suggestionIndex, CallbackHelper helper) throws TimeoutException {
        Assert.assertTrue(suggestionIndex < getNumberOfContactDetailSuggestions());

        int callCount = helper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ((OptionSection) mUI.getContactDetailsSectionForTest())
                            .getOptionLabelsForTest(suggestionIndex)
                            .performClick();
                });
        helper.waitForCallback(callCount);
    }

    /**
     * Clicks on the edit icon corresponding to the payment method suggestion at the specified
     * |suggestionIndex|.
     */
    /* package */ void clickOnPaymentMethodSuggestionEditIconAndWait(
            final int suggestionIndex, CallbackHelper helper) throws TimeoutException {
        Assert.assertTrue(suggestionIndex < getNumberOfPaymentApps());

        int callCount = helper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ((OptionSection) mUI.getPaymentMethodSectionForTest())
                            .getOptionRowAtIndex(suggestionIndex)
                            .getEditIconForTest()
                            .performClick();
                });
        helper.waitForCallback(callCount);
    }

    /** Returns the summary text of the shipping address section. */
    /* package */ String getShippingAddressSummaryLabel() {
        return getShippingAddressSummary();
    }

    /** Returns the summary text of the shipping option section. */
    /* package */ String getShippingOptionSummaryLabel() {
        return getShippingOptionSummary();
    }

    /** Returns the cost text of the shipping option section on the bottom sheet. */
    /* package */ String getShippingOptionCostSummaryLabelOnBottomSheet() {
        return getShippingOptionCostSummaryOnBottomSheet();
    }

    /** Returns the number of shipping address suggestions. */
    /* package */ int getNumberOfShippingAddressSuggestions() {
        return ThreadUtils.runOnUiThreadBlocking(
                () ->
                        ((OptionSection) mUI.getShippingAddressSectionForTest())
                                .getNumberOfOptionLabelsForTest());
    }

    /** Returns the {@link OptionRow} at the given index for the shipping address section. */
    /* package */ OptionRow getShippingAddressOptionRowAtIndex(final int index) {
        return ThreadUtils.runOnUiThreadBlocking(
                () ->
                        ((OptionSection) mUI.getShippingAddressSectionForTest())
                                .getOptionRowAtIndex(index));
    }

    /** Returns the error message visible to the user in the credit card unmask prompt. */
    /* package */ String getUnmaskPromptErrorMessage() {
        return mCardUnmaskPrompt.getErrorMessage();
    }

    /** Selects the spinner value in the editor UI. */
    /* package */ void setSpinnerSelectionInEditorAndWait(
            final int selection, CallbackHelper helper) throws TimeoutException {
        int callCount = helper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        ((Spinner) mUI.getEditorDialog().findViewById(R.id.spinner))
                                .setSelection(selection));
        helper.waitForCallback(callCount);
    }

    /** Directly sets the text in the editor UI. */
    /* package */ void setTextInEditorAndWait(final String[] values, CallbackHelper helper)
            throws TimeoutException {
        int callCount = helper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    List<EditText> fields = mUI.getEditorDialog().getEditableTextFieldsForTest();
                    for (int i = 0; i < values.length; i++) {
                        fields.get(i).requestFocus();
                        fields.get(i).setText(values[i]);
                    }
                });
        helper.waitForCallback(callCount);
    }

    /** Directly sets the text in the card unmask UI. */
    /* package */ void setTextInCardUnmaskDialogAndWait(
            final int resourceId, final String input, CallbackHelper helper)
            throws TimeoutException {
        int callCount = helper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    EditText editText =
                            mCardUnmaskPrompt
                                    .getDialogForTest()
                                    .get(ModalDialogProperties.CUSTOM_VIEW)
                                    .findViewById(resourceId);
                    editText.setText(input);
                    editText.getOnFocusChangeListener().onFocusChange(null, false);
                });
        helper.waitForCallback(callCount);
    }

    /** Directly sets the text in the expired card unmask UI. */
    /* package */ void setTextInExpiredCardUnmaskDialogAndWait(
            final int[] resourceIds, final String[] values, CallbackHelper helper)
            throws TimeoutException {
        assert resourceIds.length == values.length;
        int callCount = helper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    for (int i = 0; i < resourceIds.length; ++i) {
                        EditText editText =
                                mCardUnmaskPrompt
                                        .getDialogForTest()
                                        .get(ModalDialogProperties.CUSTOM_VIEW)
                                        .findViewById(resourceIds[i]);
                        editText.setText(values[i]);
                        editText.getOnFocusChangeListener().onFocusChange(null, false);
                    }
                });
        helper.waitForCallback(callCount);
    }

    /** Focues a view and hits the "submit" button on the software keyboard. */
    /* package */ void hitSoftwareKeyboardSubmitButtonAndWait(
            final int resourceId, CallbackHelper helper) throws TimeoutException {
        int callCount = helper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    EditText editText =
                            mCardUnmaskPrompt
                                    .getDialogForTest()
                                    .get(ModalDialogProperties.CUSTOM_VIEW)
                                    .findViewById(resourceId);
                    editText.requestFocus();
                    editText.onEditorAction(EditorInfo.IME_ACTION_DONE);
                });
        helper.waitForCallback(callCount);
    }

    /** Verifies the contents of the test webpage. */
    /* package */ void expectResultContains(final String[] contents) {
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    try {
                        String result = DOMUtils.getNodeContents(mWebContentsRef.get(), "result");
                        Criteria.checkThat(
                                "Cannot find 'result' node on test page",
                                result,
                                Matchers.notNullValue());
                        for (int i = 0; i < contents.length; i++) {
                            Criteria.checkThat(
                                    "Result '" + result + "' should contain '" + contents[i] + "'",
                                    result,
                                    Matchers.containsString(contents[i]));
                        }
                    } catch (TimeoutException e2) {
                        throw new CriteriaNotSatisfiedException(e2);
                    }
                });
    }

    /** Will fail if the OptionRow at |index| is not selected in Contact Details. */
    /* package */ void expectContactDetailsRowIsSelected(final int index) {
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    boolean isSelected =
                            ((OptionSection) mUI.getContactDetailsSectionForTest())
                                    .getOptionRowAtIndex(index)
                                    .isChecked();
                    Criteria.checkThat(
                            "Contact Details row at " + index + " was not selected.",
                            isSelected,
                            Matchers.is(true));
                });
    }

    /** Will fail if the OptionRow at |index| is not selected in Shipping Address section. */
    /* package */ void expectShippingAddressRowIsSelected(final int index) {
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    boolean isSelected =
                            ((OptionSection) mUI.getShippingAddressSectionForTest())
                                    .getOptionRowAtIndex(index)
                                    .isChecked();
                    Criteria.checkThat(
                            "Shipping Address row at " + index + " was not selected.",
                            isSelected,
                            Matchers.is(true));
                });
    }

    /** Will fail if the OptionRow at |index| is not selected in PaymentMethod section. */
    /* package */ void expectPaymentMethodRowIsSelected(final int index) {
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    boolean isSelected =
                            ((OptionSection) mUI.getPaymentMethodSectionForTest())
                                    .getOptionRowAtIndex(index)
                                    .isChecked();
                    Criteria.checkThat(
                            "Payment Method row at " + index + " was not selected.",
                            isSelected,
                            Matchers.is(true));
                });
    }

    /* package */ View getPaymentRequestView() {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> mUI.getDialogForTest().findViewById(R.id.payment_request));
    }

    /* package */ View getCardUnmaskView() throws Throwable {
        return ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mCardUnmaskPrompt
                                .getDialogForTest()
                                .get(ModalDialogProperties.CUSTOM_VIEW)
                                .findViewById(R.id.autofill_card_unmask_prompt));
    }

    /* package */ View getEditorDialogView() throws Throwable {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> mUI.getEditorDialog().findViewById(R.id.editor_container));
    }

    /* package */ void setAutoAdvanceInputProtectorClock(boolean autoAdvanceInputProtectorClock) {
        mAutoAdvanceInputProtectorClock = autoAdvanceInputProtectorClock;
    }

    /* package */ void advanceInputProtectorClock() {
        mClock.advanceCurrentTimeMillis(SAFE_INPUT_DELAY);
    }

    @Override
    public void onPaymentRequestUIShow(PaymentRequestUI ui) {
        ThreadUtils.assertOnUiThread();
        mUI = ui;
        mInputProtector.markShowTime();
        mUI.setInputProtectorForTest(mInputProtector);
        // By default, we advance the clock immediately as most tests just wait for ReadyForInput.
        if (mAutoAdvanceInputProtectorClock) {
            advanceInputProtectorClock();
        }
        mShowCalled.notifyCalled(ui);
    }

    @Override
    public void onPaymentRequestReadyForInput(PaymentRequestUI ui) {
        ThreadUtils.assertOnUiThread();
        mReadyForInput.notifyCalled(ui);
    }

    @Override
    public void onEditorReadyToEdit() {
        ThreadUtils.assertOnUiThread();
        mReadyToEdit.notifyCalled();
    }

    @Override
    public void onEditorValidationError() {
        ThreadUtils.assertOnUiThread();
        mEditorValidationError.notifyCalled();
    }

    @Override
    public void onEditorTextUpdate() {
        ThreadUtils.assertOnUiThread();
        mEditorTextUpdate.notifyCalled();
    }

    @Override
    public void onEditorConfirmationDialogShown() {
        // Not used.
    }

    @Override
    public void onPaymentRequestReadyToPay(PaymentRequestUI ui) {
        ThreadUtils.assertOnUiThread();
        mReadyToPay.notifyCalled(ui);
    }

    @Override
    public void onPaymentRequestSelectionChecked(PaymentRequestUI ui) {
        ThreadUtils.assertOnUiThread();
        mSelectionChecked.notifyCalled(ui);
    }

    @Override
    public void onPaymentRequestResultReady(PaymentRequestUI ui) {
        ThreadUtils.assertOnUiThread();
        mResultReady.notifyCalled(ui);
    }

    @Override
    public void onEditorDismiss() {
        ThreadUtils.assertOnUiThread();
        mDismissed.notifyCalled();
    }

    @Override
    public void onCreatedChromePaymentRequestDelegateImpl(
            ChromePaymentRequestDelegateImpl delegateImpl) {
        ThreadUtils.assertOnUiThread();
        mChromePaymentRequestDelegateImpl = delegateImpl;
    }

    @Override
    public void onPaymentRequestServiceUnableToAbort() {
        ThreadUtils.assertOnUiThread();
        mUnableToAbort.notifyCalled();
    }

    @Override
    public void onPaymentRequestServiceBillingAddressChangeProcessed() {
        ThreadUtils.assertOnUiThread();
        mBillingAddressChangeProcessed.notifyCalled();
    }

    @Override
    public void onPaymentRequestServiceExpirationMonthChange() {
        ThreadUtils.assertOnUiThread();
        mExpirationMonthChange.notifyCalled();
    }

    @Override
    public void onPaymentRequestServiceShowFailed() {
        ThreadUtils.assertOnUiThread();
        mShowFailed.notifyCalled();
    }

    @Override
    public void onPaymentRequestServiceCanMakePaymentQueryResponded() {
        ThreadUtils.assertOnUiThread();
        mCanMakePaymentQueryResponded.notifyCalled();
    }

    @Override
    public void onPaymentRequestServiceHasEnrolledInstrumentQueryResponded() {
        ThreadUtils.assertOnUiThread();
        mHasEnrolledInstrumentQueryResponded.notifyCalled();
    }

    @Override
    public void onCardUnmaskPromptReadyForInput(CardUnmaskPrompt prompt) {
        ThreadUtils.assertOnUiThread();
        mReadyForUnmaskInput.notifyCalled(prompt);
        mCardUnmaskPrompt = prompt;
    }

    @Override
    public void onCardUnmaskPromptReadyToUnmask(CardUnmaskPrompt prompt) {
        ThreadUtils.assertOnUiThread();
        mReadyToUnmask.notifyCalled(prompt);
    }

    @Override
    public void onCardUnmaskPromptValidationDone(CardUnmaskPrompt prompt) {
        ThreadUtils.assertOnUiThread();
        mUnmaskValidationDone.notifyCalled(prompt);
    }

    @Override
    public void onCardUnmaskPromptSubmitRejected(CardUnmaskPrompt prompt) {
        ThreadUtils.assertOnUiThread();
        mSubmitRejected.notifyCalled(prompt);
    }

    @Override
    public void onPaymentResponseReady() {
        ThreadUtils.assertOnUiThread();
        mPaymentResponseReady.notifyCalled();
    }

    @Override
    public void onCompletedHandled() {
        ThreadUtils.assertOnUiThread();
        mCompleteHandled.notifyCalled();
    }

    @Override
    public void onRendererClosedMojoConnection() {
        ThreadUtils.assertOnUiThread();
        mRendererClosedMojoConnection.notifyCalled();
    }

    /** Listens for UI notifications. */
    static class PaymentsCallbackHelper<T> extends CallbackHelper {
        private T mTarget;

        /**
         * Returns the UI that is ready for input.
         *
         * @return The UI that is ready for input.
         */
        /* package */ T getTarget() {
            return mTarget;
        }

        /**
         * Called when the UI is ready for input.
         *
         * @param target The UI that is ready for input.
         */
        /* package */ void notifyCalled(T target) {
            ThreadUtils.assertOnUiThread();
            mTarget = target;
            notifyCalled();
        }
    }

    /**
     * Adds a payment app factory for testing.
     *
     * @param appPresence Whether the factory has apps.
     * @param factorySpeed How quick the factory creates apps.
     * @return The test factory. Can be ignored.
     */
    /* package */ TestFactory addPaymentAppFactory(
            @AppPresence int appPresence, @FactorySpeed int factorySpeed) {
        return addPaymentAppFactory("https://bobpay.test", appPresence, factorySpeed);
    }

    /**
     * Adds a payment app factory for testing.
     *
     * @param methodName The name of the payment method used in the payment app.
     * @param appPresence Whether the factory has apps.
     * @param factorySpeed How quick the factory creates apps.
     * @return The test factory. Can be ignored.
     */
    /* package */ TestFactory addPaymentAppFactory(
            String methodName, @AppPresence int appPresence, @FactorySpeed int factorySpeed) {
        return addPaymentAppFactory(methodName, appPresence, factorySpeed, AppSpeed.FAST_APP);
    }

    /**
     * Adds a payment app factory for testing.
     *
     * @param methodName The name of the payment method used in the payment app.
     * @param appPresence Whether the factory has apps.
     * @param factorySpeed How quick the factory creates apps.
     * @param appSpeed How quick the app responds to "invoke".
     * @return The test factory. Can be ignored.
     */
    /* package */ TestFactory addPaymentAppFactory(
            String appMethodName,
            int appPresence,
            @FactorySpeed int factorySpeed,
            @AppSpeed int appSpeed) {
        TestFactory factory = new TestFactory(appMethodName, appPresence, factorySpeed, appSpeed);
        PaymentAppService.getInstance().addFactory(factory);
        return factory;
    }

    /** A payment app factory implementation for test. */
    /* package */ static final class TestFactory implements PaymentAppFactoryInterface {
        private final String mAppMethodName;
        private final @AppPresence int mAppPresence;
        private final @FactorySpeed int mFactorySpeed;
        private final @AppSpeed int mAppSpeed;
        private PaymentAppFactoryDelegate mDelegate;

        private TestFactory(
                String appMethodName,
                @AppPresence int appPresence,
                @FactorySpeed int factorySpeed,
                @AppSpeed int appSpeed) {
            mAppMethodName = appMethodName;
            mAppPresence = appPresence;
            mFactorySpeed = factorySpeed;
            mAppSpeed = appSpeed;
        }

        @Override
        public void create(PaymentAppFactoryDelegate delegate) {
            Runnable createApp =
                    () -> {
                        if (delegate.getParams().hasClosed()) return;
                        boolean canMakePayment =
                                delegate.getParams().getMethodData().containsKey(mAppMethodName);
                        delegate.onCanMakePaymentCalculated(canMakePayment);
                        if (canMakePayment && mAppPresence == AppPresence.HAVE_APPS) {
                            delegate.onPaymentAppCreated(new TestPay(mAppMethodName, mAppSpeed));
                        }
                        delegate.onDoneCreatingPaymentApps(this);
                    };
            if (mFactorySpeed == FactorySpeed.FAST_FACTORY) {
                createApp.run();
            } else {
                new Handler().postDelayed(createApp, 100);
            }
            mDelegate = delegate;
        }

        /* package */ PaymentAppFactoryDelegate getDelegateForTest() {
            return mDelegate;
        }
    }

    /** A payment app implementation for test. */
    /* package */ static final class TestPay extends PaymentApp {
        private final String mDefaultMethodName;
        private final @AppSpeed int mAppSpeed;

        TestPay(String defaultMethodName, @AppSpeed int appSpeed) {
            super(
                    /* id= */ UUID.randomUUID().toString(),
                    /* label= */ defaultMethodName,
                    /* sublabel= */ null,
                    /* icon= */ null);
            mDefaultMethodName = defaultMethodName;
            mAppSpeed = appSpeed;
        }

        @Override
        public Set<String> getInstrumentMethodNames() {
            Set<String> result = new HashSet<>();
            result.add(mDefaultMethodName);
            return result;
        }

        @Override
        public void invokePaymentApp(
                String id,
                String merchantName,
                String origin,
                String iframeOrigin,
                byte[][] certificateChain,
                Map<String, PaymentMethodData> methodData,
                PaymentItem total,
                List<PaymentItem> displayItems,
                Map<String, PaymentDetailsModifier> modifiers,
                PaymentOptions paymentOptions,
                List<PaymentShippingOption> shippingOptions,
                InstrumentDetailsCallback detailsCallback) {
            Runnable respond =
                    () -> {
                        detailsCallback.onInstrumentDetailsReady(
                                mDefaultMethodName,
                                "{\"transaction\": 1337, \"total\": \""
                                        + total.amount.value
                                        + "\"}",
                                new PayerData());
                    };
            if (mAppSpeed == AppSpeed.FAST_APP) {
                respond.run();
            } else {
                new Handler().postDelayed(respond, 100);
            }
        }

        @Override
        public void dismissInstrument() {}
    }

    public void startMainActivity() {
        assert mDelayStartActivity;
        startMainActivityWithURL(mTestFilePath);
    }

    @Override
    protected void before() throws Throwable {
        super.before();
        if (!mDelayStartActivity) {
            startMainActivityWithURL(mTestFilePath);
            setObserversAndWaitForInitialPageLoad();
        }
    }
}
