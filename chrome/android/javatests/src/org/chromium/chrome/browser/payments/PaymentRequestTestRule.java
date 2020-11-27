// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.os.Handler;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.inputmethod.EditorInfo;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.Spinner;
import android.widget.TextView;

import androidx.annotation.IntDef;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.CriteriaNotSatisfiedException;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.CardUnmaskPrompt;
import org.chromium.chrome.browser.autofill.CardUnmaskPrompt.CardUnmaskObserverForTest;
import org.chromium.chrome.browser.autofill.prefeditor.EditorObserverForTest;
import org.chromium.chrome.browser.autofill.prefeditor.EditorTextField;
import org.chromium.chrome.browser.payments.ChromePaymentRequestFactory.ChromePaymentRequestDelegateImpl;
import org.chromium.chrome.browser.payments.ChromePaymentRequestFactory.ChromePaymentRequestDelegateImplObserverForTest;
import org.chromium.chrome.browser.payments.ui.PaymentRequestSection.OptionSection;
import org.chromium.chrome.browser.payments.ui.PaymentRequestSection.OptionSection.OptionRow;
import org.chromium.chrome.browser.payments.ui.PaymentRequestUI;
import org.chromium.chrome.browser.payments.ui.PaymentRequestUI.PaymentRequestObserverForTest;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.payments.AbortReason;
import org.chromium.components.payments.PayerData;
import org.chromium.components.payments.PaymentApp;
import org.chromium.components.payments.PaymentAppFactoryDelegate;
import org.chromium.components.payments.PaymentAppFactoryInterface;
import org.chromium.components.payments.PaymentAppService;
import org.chromium.components.payments.PaymentFeatureList;
import org.chromium.components.payments.PaymentRequestService;
import org.chromium.components.payments.PaymentRequestService.PaymentRequestServiceObserverForTest;
import org.chromium.content_public.browser.UiThreadTaskTraits;
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
import java.util.Locale;
import java.util.Map;
import java.util.Set;
import java.util.UUID;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Custom ActivityTestRule for integration test for payments.
 */
public class PaymentRequestTestRule extends ChromeTabbedActivityTestRule
        implements PaymentRequestObserverForTest, PaymentRequestServiceObserverForTest,
                   ChromePaymentRequestDelegateImplObserverForTest, CardUnmaskObserverForTest,
                   EditorObserverForTest {
    @IntDef({AppPresence.NO_APPS, AppPresence.HAVE_APPS})
    @Retention(RetentionPolicy.SOURCE)
    public @interface AppPresence {
        /** Flag for a factory without payment apps. */
        public static final int NO_APPS = 0;

        /** Flag for a factory with payment apps. */
        public static final int HAVE_APPS = 1;
    }

    @IntDef({AppSpeed.FAST_APP, AppSpeed.SLOW_APP})
    @Retention(RetentionPolicy.SOURCE)
    public @interface AppSpeed {
        /** Flag for installing a payment app that responds to its invocation fast. */
        public static final int FAST_APP = 0;

        /** Flag for installing a payment app that responds to its invocation slowly. */
        public static final int SLOW_APP = 1;
    }

    @IntDef({FactorySpeed.FAST_FACTORY, FactorySpeed.SLOW_FACTORY})
    @Retention(RetentionPolicy.SOURCE)
    public @interface FactorySpeed {
        /** Flag for a factory that immediately creates a payment app. */
        public static final int FAST_FACTORY = 0;

        /** Flag for a factory that creates a payment app with a delay. */
        public static final int SLOW_FACTORY = 1;
    }

    /** The expiration month dropdown index for December. */
    public static final int DECEMBER = 11;

    /** The expiration year dropdown index for the next year. */
    public static final int NEXT_YEAR = 1;

    /**
     * The billing address dropdown index for the first billing address. Index 0 is for the
     * "Select" hint.
     */
    public static final int FIRST_BILLING_ADDRESS = 1;

    /** Command line flag to enable payment details modifiers in tests. */
    public static final String ENABLE_WEB_PAYMENTS_MODIFIERS =
            "enable-features=" + PaymentFeatureList.WEB_PAYMENTS_MODIFIERS;

    /** Command line flag to enable experimental web platform features in tests. */
    public static final String ENABLE_EXPERIMENTAL_WEB_PLATFORM_FEATURES =
            "enable-experimental-web-platform-features";

    final PaymentsCallbackHelper<PaymentRequestUI> mReadyForInput;
    final PaymentsCallbackHelper<PaymentRequestUI> mReadyToPay;
    final PaymentsCallbackHelper<PaymentRequestUI> mSelectionChecked;
    final PaymentsCallbackHelper<PaymentRequestUI> mResultReady;
    final PaymentsCallbackHelper<CardUnmaskPrompt> mReadyForUnmaskInput;
    final PaymentsCallbackHelper<CardUnmaskPrompt> mReadyToUnmask;
    final PaymentsCallbackHelper<CardUnmaskPrompt> mUnmaskValidationDone;
    final PaymentsCallbackHelper<CardUnmaskPrompt> mSubmitRejected;
    final CallbackHelper mReadyToEdit;
    final CallbackHelper mEditorValidationError;
    final CallbackHelper mEditorTextUpdate;
    final CallbackHelper mDismissed;
    final CallbackHelper mUnableToAbort;
    final CallbackHelper mBillingAddressChangeProcessed;
    final CallbackHelper mShowFailed;
    final CallbackHelper mCanMakePaymentQueryResponded;
    final CallbackHelper mHasEnrolledInstrumentQueryResponded;
    final CallbackHelper mExpirationMonthChange;
    final CallbackHelper mPaymentResponseReady;
    final CallbackHelper mCompleteHandled;
    final CallbackHelper mRendererClosedMojoConnection;
    private ChromePaymentRequestDelegateImpl mChromePaymentRequestDelegateImpl;
    PaymentRequestUI mUI;

    private final boolean mDelayStartActivity;

    private final AtomicReference<WebContents> mWebContentsRef;

    private final String mTestFilePath;

    private CardUnmaskPrompt mCardUnmaskPrompt;

    private final MainActivityStartCallback mCallback;

    /**
     * Creates an instance of PaymentRequestTestRule.
     * @param testFileName The file name of an test page in //components/test/data/payments,
     *         'about:blank', or a data url which starts with 'data:'.
     */
    public PaymentRequestTestRule(String testFileName) {
        this(testFileName, null);
    }

    /**
     * Creates an instance of PaymentRequestTestRule.
     * @param testFileName The file name of an test page in //components/test/data/payments,
     *         'about:blank', or a data url which starts with 'data:'.
     * @param callback A callback that is invoked on the start of the main activity.
     */
    public PaymentRequestTestRule(String testFileName, MainActivityStartCallback callback) {
        this(testFileName, callback, false);
    }

    /**
     * Creates an instance of PaymentRequestTestRule.
     * @param testFileName The file name of an test page in //components/test/data/payments,
     *         'about:blank', or a data url which starts with 'data:'.
     * @param callback A callback that is invoked on the start of the main activity.
     * @param delayStartActivity Whether to delay the start of the main activity. When true, {@link
     *         #startMainActivity()} needs to be called to start the main activity; otherwise, the
     *         main activity would start automatically.
     */
    public PaymentRequestTestRule(
            String testFileName, MainActivityStartCallback callback, boolean delayStartActivity) {
        this(testFileName, /*pathPrefix=*/"components/test/data/payments/", callback,
                delayStartActivity);
    }

    /**
     * Creates an instance of PaymentRequestTestRule with a test page, which is specified by
     * pathPrefix and testFileName combined into a path relative to the repository root. For
     * example, if testFileName is "merchant.html", pathPrefix is "components/test/data/payments/",
     * the method would look for a test page at "components/test/data/payments/merchant.html".
     * This method is used by the //clank tests.
     * @param testFileName The file name of the test page.
     * @param pathPrefix The prefix path to testFileName.
     * @param delayStartActivity Whether to delay the start of the main activity.
     * @return The created instance.
     */
    public static PaymentRequestTestRule createWithPathPrefix(
            String testFileName, String pathPrefix, boolean delayStartActivity) {
        assert pathPrefix.endsWith("/");
        return new PaymentRequestTestRule(testFileName, pathPrefix, null, delayStartActivity);
    }

    private PaymentRequestTestRule(String testFilePath, String pathPrefix,
            MainActivityStartCallback callback, boolean delayStartActivity) {
        super();
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
        mCallback = callback;
        mDelayStartActivity = delayStartActivity;
    }

    public void startMainActivity() {
        startMainActivityWithURL(mTestFilePath);
        try {
            // TODO(crbug.com/1144303): Figure out what these tests need to wait on to not be flaky
            // instead of sleeping.
            Thread.sleep(2000);
        } catch (Exception ex) {
        }
    }

    // public is used so as to be visible to the payment tests in //clank.
    public void openPage() throws TimeoutException {
        onMainActivityStarted();
        ThreadUtils.runOnUiThreadBlocking(() -> {
            mWebContentsRef.set(getActivity().getCurrentWebContents());
            PaymentRequestUI.setEditorObserverForTest(PaymentRequestTestRule.this);
            PaymentRequestUI.setPaymentRequestObserverForTest(PaymentRequestTestRule.this);
            PaymentRequestService.setObserverForTest(PaymentRequestTestRule.this);
            ChromePaymentRequestFactory.setChromePaymentRequestDelegateImplObserverForTest(
                    PaymentRequestTestRule.this);
            CardUnmaskPrompt.setObserverForTest(PaymentRequestTestRule.this);
        });
        assertWaitForPageScaleFactorMatch(1);
    }

    public PaymentsCallbackHelper<PaymentRequestUI> getReadyForInput() {
        return mReadyForInput;
    }
    public PaymentsCallbackHelper<PaymentRequestUI> getReadyToPay() {
        return mReadyToPay;
    }
    public PaymentsCallbackHelper<PaymentRequestUI> getSelectionChecked() {
        return mSelectionChecked;
    }
    public PaymentsCallbackHelper<PaymentRequestUI> getResultReady() {
        return mResultReady;
    }
    public PaymentsCallbackHelper<CardUnmaskPrompt> getReadyForUnmaskInput() {
        return mReadyForUnmaskInput;
    }
    public PaymentsCallbackHelper<CardUnmaskPrompt> getReadyToUnmask() {
        return mReadyToUnmask;
    }
    public PaymentsCallbackHelper<CardUnmaskPrompt> getUnmaskValidationDone() {
        return mUnmaskValidationDone;
    }
    public PaymentsCallbackHelper<CardUnmaskPrompt> getSubmitRejected() {
        return mSubmitRejected;
    }
    public CallbackHelper getReadyToEdit() {
        return mReadyToEdit;
    }
    public CallbackHelper getEditorValidationError() {
        return mEditorValidationError;
    }
    public CallbackHelper getEditorTextUpdate() {
        return mEditorTextUpdate;
    }
    public CallbackHelper getDismissed() {
        return mDismissed;
    }
    public CallbackHelper getUnableToAbort() {
        return mUnableToAbort;
    }
    public CallbackHelper getBillingAddressChangeProcessed() {
        return mBillingAddressChangeProcessed;
    }
    public CallbackHelper getShowFailed() {
        return mShowFailed;
    }
    public CallbackHelper getCanMakePaymentQueryResponded() {
        return mCanMakePaymentQueryResponded;
    }
    public CallbackHelper getHasEnrolledInstrumentQueryResponded() {
        return mHasEnrolledInstrumentQueryResponded;
    }
    public CallbackHelper getExpirationMonthChange() {
        return mExpirationMonthChange;
    }
    public CallbackHelper getPaymentResponseReady() {
        return mPaymentResponseReady;
    }
    public CallbackHelper getCompleteHandled() {
        return mCompleteHandled;
    }
    public CallbackHelper getRendererClosedMojoConnection() {
        return mRendererClosedMojoConnection;
    }
    public PaymentRequestUI getPaymentRequestUI() {
        return mUI;
    }

    protected void triggerUIAndWait(PaymentsCallbackHelper<PaymentRequestUI> helper)
            throws TimeoutException {
        openPageAndClickNodeAndWait("buy", helper);
        mUI = helper.getTarget();
    }

    protected void openPageAndClickNodeAndWait(String nodeId, CallbackHelper helper)
            throws TimeoutException {
        openPage();
        clickNodeAndWait(nodeId, helper);
    }

    protected void openPageAndClickBuyAndWait(CallbackHelper helper) throws TimeoutException {
        openPageAndClickNodeAndWait("buy", helper);
    }

    protected void openPageAndClickNode(String nodeId) throws TimeoutException {
        openPage();
        clickNode(nodeId);
    }

    protected void triggerUIAndWait(String nodeId, PaymentsCallbackHelper<PaymentRequestUI> helper)
            throws TimeoutException {
        openPageAndClickNodeAndWait(nodeId, helper);
        mUI = helper.getTarget();
    }

    protected void reTriggerUIAndWait(String nodeId,
            PaymentsCallbackHelper<PaymentRequestUI> helper) throws TimeoutException {
        clickNodeAndWait(nodeId, helper);
        mUI = helper.getTarget();
    }

    protected void retryPaymentRequest(String validationErrors, CallbackHelper helper)
            throws TimeoutException {
        int callCount = helper.getCallCount();
        JavaScriptUtils.executeJavaScriptAndWaitForResult(
                mWebContentsRef.get(), "retry(" + validationErrors + ");");
        helper.waitForCallback(callCount);
    }

    protected String executeJavaScriptAndWaitForResult(String script) throws TimeoutException {
        return JavaScriptUtils.executeJavaScriptAndWaitForResult(mWebContentsRef.get(), script);
    }

    // public is used so as to be visible to the payment tests in //clank.
    public String runJavascriptWithAsyncResult(String script) throws TimeoutException {
        return JavaScriptUtils.runJavascriptWithAsyncResult(mWebContentsRef.get(), script);
    }

    /** Clicks on an HTML node. */
    protected void clickNodeAndWait(String nodeId, CallbackHelper helper) throws TimeoutException {
        int callCount = helper.getCallCount();
        clickNode(nodeId);
        helper.waitForCallback(callCount);
    }

    /** Clicks on an HTML node. */
    protected void clickNode(String nodeId) throws TimeoutException {
        DOMUtils.clickNode(mWebContentsRef.get(), nodeId);
    }

    /** Clicks on an element in the payments UI. */
    protected void clickAndWait(int resourceId, CallbackHelper helper) throws TimeoutException {
        int callCount = helper.getCallCount();
        CriteriaHelper.pollUiThread(() -> {
            boolean canClick = mUI.isAcceptingUserInput();
            if (canClick) mUI.getDialogForTest().findViewById(resourceId).performClick();
            Criteria.checkThat(canClick, Matchers.is(true));
        });
        helper.waitForCallback(callCount);
    }

    /** Clicks on an element in the "Order summary" section of the payments UI. */
    protected void clickInOrderSummaryAndWait(CallbackHelper helper) throws TimeoutException {
        int callCount = helper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(() -> {
            mUI.getOrderSummarySectionForTest().findViewById(R.id.payments_section).performClick();
        });
        helper.waitForCallback(callCount);
    }

    /** Clicks on an element in the "Shipping address" section of the payments UI. */
    protected void clickInShippingAddressAndWait(final int resourceId, CallbackHelper helper)
            throws TimeoutException {
        int callCount = helper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(() -> {
            mUI.getShippingAddressSectionForTest().findViewById(resourceId).performClick();
        });
        helper.waitForCallback(callCount);
    }

    /** Clicks on an element in the "Payment" section of the payments UI. */
    protected void clickInPaymentMethodAndWait(final int resourceId, CallbackHelper helper)
            throws TimeoutException {
        int callCount = helper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(() -> {
            mUI.getPaymentMethodSectionForTest().findViewById(resourceId).performClick();
        });
        helper.waitForCallback(callCount);
    }

    /** Clicks on an element in the "Contact Info" section of the payments UI. */
    protected void clickInContactInfoAndWait(final int resourceId, CallbackHelper helper)
            throws TimeoutException {
        int callCount = helper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(() -> {
            mUI.getContactDetailsSectionForTest().findViewById(resourceId).performClick();
        });
        helper.waitForCallback(callCount);
    }

    /** Clicks on an element in the editor UI for credit cards. */
    protected void clickInCardEditorAndWait(final int resourceId, CallbackHelper helper)
            throws TimeoutException {
        int callCount = helper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(
                () -> { mUI.getCardEditorDialog().findViewById(resourceId).performClick(); });
        helper.waitForCallback(callCount);
    }

    /** Clicks on an element in the editor UI. */
    protected void clickInEditorAndWait(final int resourceId, CallbackHelper helper)
            throws TimeoutException {
        int callCount = helper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(
                () -> { mUI.getEditorDialog().findViewById(resourceId).performClick(); });
        helper.waitForCallback(callCount);
    }

    protected void clickAndroidBackButtonInEditorAndWait(CallbackHelper helper)
            throws TimeoutException {
        int callCount = helper.getCallCount();
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> {
            mUI.getEditorDialog().dispatchKeyEvent(
                    new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_BACK));
            mUI.getEditorDialog().dispatchKeyEvent(
                    new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_BACK));
        });
        helper.waitForCallback(callCount);
    }

    /** Clicks on a button in the card unmask UI. */
    protected void clickCardUnmaskButtonAndWait(final int dialogButtonId, CallbackHelper helper)
            throws TimeoutException {
        int callCount = helper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(() -> {
            PropertyModel model = mCardUnmaskPrompt.getDialogForTest();
            model.get(ModalDialogProperties.CONTROLLER).onClick(model, dialogButtonId);
        });
        helper.waitForCallback(callCount);
    }

    /** Gets the retry error message. */
    protected String getRetryErrorMessage() {
        return ThreadUtils.runOnUiThreadBlockingNoException(
                ()
                        -> ((TextView) mUI.getDialogForTest().findViewById(R.id.retry_error))
                                   .getText()
                                   .toString());
    }

    /** Gets the button state for the shipping summary section. */
    protected int getShippingAddressSectionButtonState() {
        return ThreadUtils.runOnUiThreadBlockingNoException(
                () -> mUI.getShippingAddressSectionForTest().getEditButtonState());
    }

    /** Gets the button state for the contact details section. */
    protected int getContactDetailsButtonState() {
        return ThreadUtils.runOnUiThreadBlockingNoException(
                () -> mUI.getContactDetailsSectionForTest().getEditButtonState());
    }

    /** Returns the label of the payment app at the specified |index|. */
    protected String getPaymentAppLabel(final int index) {
        return ThreadUtils.runOnUiThreadBlockingNoException(
                ()
                        -> ((OptionSection) mUI.getPaymentMethodSectionForTest())
                                   .getOptionLabelsForTest(index)
                                   .getText()
                                   .toString());
    }

    /** Returns the label of the selected payment app. */
    protected String getSelectedPaymentAppLabel() {
        return ThreadUtils.runOnUiThreadBlockingNoException(() -> {
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
    protected String getOrderSummaryTotal() {
        return ThreadUtils.runOnUiThreadBlockingNoException(
                () -> mUI.getOrderSummaryTotalTextViewForTest().getText().toString());
    }

    /** Returns the amount text corresponding to the line item at the specified |index|. */
    protected String getLineItemAmount(int index) {
        return ThreadUtils.runOnUiThreadBlockingNoException(
                ()
                        -> mUI.getOrderSummarySectionForTest()
                                   .getLineItemAmountForTest(index)
                                   .getText()
                                   .toString()
                                   .trim());
    }

    /** Returns the amount text corresponding to the line item at the specified |index|. */
    protected int getNumberOfLineItems() {
        return ThreadUtils.runOnUiThreadBlockingNoException(
                () -> mUI.getOrderSummarySectionForTest().getNumberOfLineItemsForTest());
    }

    /**
     * Returns the label corresponding to the contact detail suggestion at the specified
     * |suggestionIndex|.
     */
    protected String getContactDetailsSuggestionLabel(final int suggestionIndex) {
        return ThreadUtils.runOnUiThreadBlockingNoException(
                ()
                        -> ((OptionSection) mUI.getContactDetailsSectionForTest())
                                   .getOptionLabelsForTest(suggestionIndex)
                                   .getText()
                                   .toString());
    }

    /** Returns the number of payment apps. */
    protected int getNumberOfPaymentApps() {
        return ThreadUtils.runOnUiThreadBlockingNoException(
                ()
                        -> ((OptionSection) mUI.getPaymentMethodSectionForTest())
                                   .getNumberOfOptionLabelsForTest());
    }

    /**
     * Returns the label corresponding to the payment method suggestion at the specified
     * |suggestionIndex|.
     */
    protected String getPaymentMethodSuggestionLabel(final int suggestionIndex) {
        Assert.assertTrue(suggestionIndex < getNumberOfPaymentApps());

        return ThreadUtils.runOnUiThreadBlockingNoException(
                ()
                        -> ((OptionSection) mUI.getPaymentMethodSectionForTest())
                                   .getOptionLabelsForTest(suggestionIndex)
                                   .getText()
                                   .toString());
    }

    /** Returns the number of contact detail suggestions. */
    protected int getNumberOfContactDetailSuggestions() {
        return ThreadUtils.runOnUiThreadBlockingNoException(
                ()
                        -> ((OptionSection) mUI.getContactDetailsSectionForTest())
                                   .getNumberOfOptionLabelsForTest());
    }

    /**
     * Returns the label corresponding to the shipping address suggestion at the specified
     * |suggestionIndex|.
     */
    protected String getShippingAddressSuggestionLabel(final int suggestionIndex) {
        Assert.assertTrue(suggestionIndex < getNumberOfShippingAddressSuggestions());

        return ThreadUtils.runOnUiThreadBlockingNoException(
                ()
                        -> mUI.getShippingAddressSectionForTest()
                                   .getOptionLabelsForTest(suggestionIndex)
                                   .getText()
                                   .toString());
    }

    protected String getShippingAddressSummary() {
        return ThreadUtils.runOnUiThreadBlockingNoException(
                ()
                        -> mUI.getShippingAddressSectionForTest()
                                   .getLeftSummaryLabelForTest()
                                   .getText()
                                   .toString());
    }

    protected String getShippingOptionSummary() {
        return ThreadUtils.runOnUiThreadBlockingNoException(
                ()
                        -> mUI.getShippingOptionSectionForTest()
                                   .getLeftSummaryLabelForTest()
                                   .getText()
                                   .toString());
    }

    protected String getShippingOptionCostSummaryOnBottomSheet() {
        return ThreadUtils.runOnUiThreadBlockingNoException(
                ()
                        -> mUI.getShippingOptionSectionForTest()
                                   .getRightSummaryLabelForTest()
                                   .getText()
                                   .toString());
    }

    protected String getShippingAddressWarningLabel() {
        return ThreadUtils.runOnUiThreadBlockingNoException(() -> {
            View view = mUI.getShippingAddressSectionForTest().findViewById(
                    R.id.payments_warning_label);
            return view != null && view instanceof TextView ? ((TextView) view).getText().toString()
                                                            : null;
        });
    }

    protected String getShippingAddressDescriptionLabel() {
        return ThreadUtils.runOnUiThreadBlockingNoException(() -> {
            View view = mUI.getShippingAddressSectionForTest().findViewById(
                    R.id.payments_description_label);
            return view != null && view instanceof TextView ? ((TextView) view).getText().toString()
                                                            : null;
        });
    }

    /** Returns the focused view in the card editor view. */
    protected View getCardEditorFocusedView() {
        return mUI.getCardEditorDialog().getCurrentFocus();
    }

    /**
     * Clicks on the label corresponding to the shipping address suggestion at the specified
     * |suggestionIndex|.
     */
    protected void clickOnShippingAddressSuggestionOptionAndWait(
            final int suggestionIndex, CallbackHelper helper) throws TimeoutException {
        Assert.assertTrue(suggestionIndex < getNumberOfShippingAddressSuggestions());

        int callCount = helper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(() -> {
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
    protected void clickOnPaymentMethodSuggestionOptionAndWait(
            final int suggestionIndex, CallbackHelper helper) throws TimeoutException {
        Assert.assertTrue(suggestionIndex < getNumberOfPaymentApps());

        int callCount = helper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(() -> {
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
    protected void clickOnContactInfoSuggestionOptionAndWait(
            final int suggestionIndex, CallbackHelper helper) throws TimeoutException {
        Assert.assertTrue(suggestionIndex < getNumberOfContactDetailSuggestions());

        int callCount = helper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(() -> {
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
    protected void clickOnPaymentMethodSuggestionEditIconAndWait(
            final int suggestionIndex, CallbackHelper helper) throws TimeoutException {
        Assert.assertTrue(suggestionIndex < getNumberOfPaymentApps());

        int callCount = helper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(() -> {
            ((OptionSection) mUI.getPaymentMethodSectionForTest())
                    .getOptionRowAtIndex(suggestionIndex)
                    .getEditIconForTest()
                    .performClick();
        });
        helper.waitForCallback(callCount);
    }

    /**
     * Returns the summary text of the shipping address section.
     */
    protected String getShippingAddressSummaryLabel() {
        return getShippingAddressSummary();
    }

    /**
     * Returns the summary text of the shipping option section.
     */
    protected String getShippingOptionSummaryLabel() {
        return getShippingOptionSummary();
    }

    /**
     * Returns the cost text of the shipping option section on the bottom sheet.
     */
    protected String getShippingOptionCostSummaryLabelOnBottomSheet() {
        return getShippingOptionCostSummaryOnBottomSheet();
    }

    /**
     * Returns the number of shipping address suggestions.
     */
    protected int getNumberOfShippingAddressSuggestions() {
        return ThreadUtils.runOnUiThreadBlockingNoException(
                ()
                        -> ((OptionSection) mUI.getShippingAddressSectionForTest())
                                   .getNumberOfOptionLabelsForTest());
    }

    /** Returns the {@link OptionRow} at the given index for the shipping address section. */
    protected OptionRow getShippingAddressOptionRowAtIndex(final int index) {
        return ThreadUtils.runOnUiThreadBlockingNoException(
                ()
                        -> ((OptionSection) mUI.getShippingAddressSectionForTest())
                                   .getOptionRowAtIndex(index));
    }

    /** Returns the selected spinner value in the editor UI for credit cards. */
    protected String getSpinnerSelectionTextInCardEditor(final int dropdownIndex) {
        return ThreadUtils.runOnUiThreadBlockingNoException(
                ()
                        -> mUI.getCardEditorDialog()
                                   .getDropdownFieldsForTest()
                                   .get(dropdownIndex)
                                   .getSelectedItem()
                                   .toString());
    }

    /** Returns the spinner value at the specified position in the editor UI for credit cards. */
    protected String getSpinnerTextAtPositionInCardEditor(
            final int dropdownIndex, final int itemPosition) {
        return ThreadUtils.runOnUiThreadBlockingNoException(
                ()
                        -> mUI.getCardEditorDialog()
                                   .getDropdownFieldsForTest()
                                   .get(dropdownIndex)
                                   .getItemAtPosition(itemPosition)
                                   .toString());
    }

    /** Returns the number of items offered by the spinner in the editor UI for credit cards. */
    protected int getSpinnerItemCountInCardEditor(final int dropdownIndex) {
        return ThreadUtils.runOnUiThreadBlockingNoException(
                ()
                        -> mUI.getCardEditorDialog()
                                   .getDropdownFieldsForTest()
                                   .get(dropdownIndex)
                                   .getCount());
    }

    /** Returns the error message visible to the user in the credit card unmask prompt. */
    protected String getUnmaskPromptErrorMessage() {
        return mCardUnmaskPrompt.getErrorMessage();
    }

    /** Selects the spinner value in the editor UI for credit cards. */
    protected void setSpinnerSelectionsInCardEditorAndWait(
            final int[] selections, CallbackHelper helper) throws TimeoutException {
        int callCount = helper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(() -> {
            List<Spinner> fields = mUI.getCardEditorDialog().getDropdownFieldsForTest();
            for (int i = 0; i < selections.length && i < fields.size(); i++) {
                fields.get(i).setSelection(selections[i]);
            }
        });
        helper.waitForCallback(callCount);
    }

    /** Selects the spinner value in the editor UI. */
    protected void setSpinnerSelectionInEditorAndWait(final int selection, CallbackHelper helper)
            throws TimeoutException {
        int callCount = helper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(
                ()
                        -> ((Spinner) mUI.getEditorDialog().findViewById(R.id.spinner))
                                   .setSelection(selection));
        helper.waitForCallback(callCount);
    }

    /** Directly sets the text in the editor UI for credit cards. */
    protected void setTextInCardEditorAndWait(final String[] values, CallbackHelper helper)
            throws TimeoutException {
        int callCount = helper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(() -> {
            ViewGroup contents = (ViewGroup) mUI.getCardEditorDialog().findViewById(R.id.contents);
            Assert.assertNotNull(contents);
            for (int i = 0, j = 0; i < contents.getChildCount() && j < values.length; i++) {
                View view = contents.getChildAt(i);
                if (view instanceof EditorTextField) {
                    ((EditorTextField) view).getEditText().setText(values[j++]);
                }
            }
        });
        helper.waitForCallback(callCount);
    }

    /** Directly sets the text in the editor UI. */
    protected void setTextInEditorAndWait(final String[] values, CallbackHelper helper)
            throws TimeoutException {
        int callCount = helper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(() -> {
            List<EditText> fields = mUI.getEditorDialog().getEditableTextFieldsForTest();
            for (int i = 0; i < values.length; i++) {
                fields.get(i).requestFocus();
                fields.get(i).setText(values[i]);
            }
        });
        helper.waitForCallback(callCount);
    }

    /** Directly sets the checkbox selection in the editor UI for credit cards. */
    protected void selectCheckboxAndWait(final int resourceId, final boolean isChecked,
            CallbackHelper helper) throws TimeoutException {
        int callCount = helper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(
                ()
                        -> ((CheckBox) mUI.getCardEditorDialog().findViewById(resourceId))
                                   .setChecked(isChecked));
        helper.waitForCallback(callCount);
    }

    /** Directly sets the text in the card unmask UI. */
    protected void setTextInCardUnmaskDialogAndWait(final int resourceId, final String input,
            CallbackHelper helper) throws TimeoutException {
        int callCount = helper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(() -> {
            EditText editText = mCardUnmaskPrompt.getDialogForTest()
                                        .get(ModalDialogProperties.CUSTOM_VIEW)
                                        .findViewById(resourceId);
            editText.setText(input);
            editText.getOnFocusChangeListener().onFocusChange(null, false);
        });
        helper.waitForCallback(callCount);
    }

    /** Directly sets the text in the expired card unmask UI. */
    protected void setTextInExpiredCardUnmaskDialogAndWait(final int[] resourceIds,
            final String[] values, CallbackHelper helper) throws TimeoutException {
        assert resourceIds.length == values.length;
        int callCount = helper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(() -> {
            for (int i = 0; i < resourceIds.length; ++i) {
                EditText editText = mCardUnmaskPrompt.getDialogForTest()
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
        ThreadUtils.runOnUiThreadBlocking(() -> {
            EditText editText = mCardUnmaskPrompt.getDialogForTest()
                                        .get(ModalDialogProperties.CUSTOM_VIEW)
                                        .findViewById(resourceId);
            editText.requestFocus();
            editText.onEditorAction(EditorInfo.IME_ACTION_DONE);
        });
        helper.waitForCallback(callCount);
    }

    /** Verifies the contents of the test webpage. */
    protected void expectResultContains(final String[] contents) {
        CriteriaHelper.pollInstrumentationThread(() -> {
            try {
                String result = DOMUtils.getNodeContents(mWebContentsRef.get(), "result");
                Criteria.checkThat(
                        "Cannot find 'result' node on test page", result, Matchers.notNullValue());
                for (int i = 0; i < contents.length; i++) {
                    Criteria.checkThat(
                            "Result '" + result + "' should contain '" + contents[i] + "'", result,
                            Matchers.containsString(contents[i]));
                }
            } catch (TimeoutException e2) {
                throw new CriteriaNotSatisfiedException(e2);
            }
        });
    }

    /** Will fail if the OptionRow at |index| is not selected in Contact Details.*/
    protected void expectContactDetailsRowIsSelected(final int index) {
        CriteriaHelper.pollInstrumentationThread(() -> {
            boolean isSelected = ((OptionSection) mUI.getContactDetailsSectionForTest())
                                         .getOptionRowAtIndex(index)
                                         .isChecked();
            Criteria.checkThat("Contact Details row at " + index + " was not selected.", isSelected,
                    Matchers.is(true));
        });
    }

    /** Will fail if the OptionRow at |index| is not selected in Shipping Address section.*/
    protected void expectShippingAddressRowIsSelected(final int index) {
        CriteriaHelper.pollInstrumentationThread(() -> {
            boolean isSelected = ((OptionSection) mUI.getShippingAddressSectionForTest())
                                         .getOptionRowAtIndex(index)
                                         .isChecked();
            Criteria.checkThat("Shipping Address row at " + index + " was not selected.",
                    isSelected, Matchers.is(true));
        });
    }

    /** Will fail if the OptionRow at |index| is not selected in PaymentMethod section.*/
    protected void expectPaymentMethodRowIsSelected(final int index) {
        CriteriaHelper.pollInstrumentationThread(() -> {
            boolean isSelected = ((OptionSection) mUI.getPaymentMethodSectionForTest())
                                         .getOptionRowAtIndex(index)
                                         .isChecked();
            Criteria.checkThat("Payment Method row at " + index + " was not selected.", isSelected,
                    Matchers.is(true));
        });
    }

    /**
     * Asserts that only the specified reason for abort is logged.
     *
     * @param abortReason The only bucket in the abort histogram that should have a record.
     */
    protected void assertOnlySpecificAbortMetricLogged(int abortReason) {
        for (int i = 0; i < AbortReason.MAX; ++i) {
            Assert.assertEquals(
                    String.format(Locale.getDefault(), "Found %d instead of %d", i, abortReason),
                    (i == abortReason ? 1 : 0),
                    RecordHistogram.getHistogramValueCountForTesting(
                            "PaymentRequest.CheckoutFunnel.Aborted", i));
        }
    }

    /* package */ View getPaymentRequestView() {
        return ThreadUtils.runOnUiThreadBlockingNoException(
                () -> mUI.getDialogForTest().findViewById(R.id.payment_request));
    }

    /* package */ View getCardUnmaskView() throws Throwable {
        return ThreadUtils.runOnUiThreadBlocking(
                ()
                        -> mCardUnmaskPrompt.getDialogForTest()
                                   .get(ModalDialogProperties.CUSTOM_VIEW)
                                   .findViewById(R.id.autofill_card_unmask_prompt));
    }

    /* package */ View getEditorDialogView() throws Throwable {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> mUI.getEditorDialog().findViewById(R.id.editor_container));
    }

    /** Allows to skip UI into paymenthandler for"basic-card". */
    protected void enableSkipUIForBasicCard() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> mChromePaymentRequestDelegateImpl.setSkipUiForBasicCard());
    }

    @Override
    public void onPaymentRequestReadyForInput(PaymentRequestUI ui) {
        ThreadUtils.assertOnUiThread();
        // This happens when the payment request is created by a direct js function call rather than
        // calling the js function via triggerUIAndWait() which sets the mUI.
        if (mUI == null) mUI = ui;
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
    public void onPaymentRequestReadyToPay(PaymentRequestUI ui) {
        ThreadUtils.assertOnUiThread();
        // This happens when the payment request is created by a direct js function call rather than
        // calling the js function via triggerUIAndWait() which sets the mUI.
        if (mUI == null) mUI = ui;
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

    /**
     * Listens for UI notifications.
     */
    static class PaymentsCallbackHelper<T> extends CallbackHelper {
        private T mTarget;

        /**
         * Returns the UI that is ready for input.
         *
         * @return The UI that is ready for input.
         */
        public T getTarget() {
            return mTarget;
        }

        /**
         * Called when the UI is ready for input.
         *
         * @param target The UI that is ready for input.
         */
        public void notifyCalled(T target) {
            ThreadUtils.assertOnUiThread();
            mTarget = target;
            notifyCalled();
        }
    }

    /**
     * Adds a payment app factory for testing.
     *
     * @param appPresence  Whether the factory has apps.
     * @param factorySpeed How quick the factory creates apps.
     * @return The test factory. Can be ignored.
     */
    /* package */ TestFactory addPaymentAppFactory(
            @AppPresence int appPresence, @FactorySpeed int factorySpeed) {
        return addPaymentAppFactory("https://bobpay.com", appPresence, factorySpeed);
    }

    /**
     * Adds a payment app factory for testing.
     *
     * @param methodName   The name of the payment method used in the payment app.
     * @param appPresence  Whether the factory has apps.
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
     * @param methodName   The name of the payment method used in the payment app.
     * @param appPresence  Whether the factory has apps.
     * @param factorySpeed How quick the factory creates apps.
     * @param appSpeed     How quick the app responds to "invoke".
     * @return The test factory. Can be ignored.
     */
    /* package */ TestFactory addPaymentAppFactory(String appMethodName, int appPresence,
            @FactorySpeed int factorySpeed, @AppSpeed int appSpeed) {
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

        private TestFactory(String appMethodName, @AppPresence int appPresence,
                @FactorySpeed int factorySpeed, @AppSpeed int appSpeed) {
            mAppMethodName = appMethodName;
            mAppPresence = appPresence;
            mFactorySpeed = factorySpeed;
            mAppSpeed = appSpeed;
        }

        @Override
        public void create(PaymentAppFactoryDelegate delegate) {
            Runnable createApp = () -> {
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
            super(/*id=*/UUID.randomUUID().toString(), /*label=*/defaultMethodName,
                    /*sublabel=*/null, /*icon=*/null);
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
        public void invokePaymentApp(String id, String merchantName, String origin,
                String iframeOrigin, byte[][] certificateChain,
                Map<String, PaymentMethodData> methodData, PaymentItem total,
                List<PaymentItem> displayItems, Map<String, PaymentDetailsModifier> modifiers,
                PaymentOptions paymentOptions, List<PaymentShippingOption> shippingOptions,
                InstrumentDetailsCallback detailsCallback) {
            Runnable respond = () -> {
                detailsCallback.onInstrumentDetailsReady(mDefaultMethodName,
                        "{\"transaction\": 1337, \"total\": \"" + total.amount.value + "\"}",
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

    public void onMainActivityStarted() throws TimeoutException {
        if (mCallback != null) {
            mCallback.onMainActivityStarted();
        }
    }

    @Override
    public Statement apply(final Statement base, Description description) {
        return super.apply(new Statement() {
            @Override
            public void evaluate() throws Throwable {
                if (!mDelayStartActivity) startMainActivity();
                base.evaluate();
            }
        }, description);
    }

    /** The interface for being notified of the main activity startup. */
    public interface MainActivityStartCallback {
        /** Called when the main activity has started up. */
        void onMainActivityStarted() throws TimeoutException;
    }
}
