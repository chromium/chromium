// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments.ui;

import static org.chromium.chrome.browser.payments.ui.PaymentRequestSection.EDIT_BUTTON_GONE;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.ObjectAnimator;
import android.animation.ValueAnimator;
import android.animation.ValueAnimator.AnimatorUpdateListener;
import android.app.Activity;
import android.app.Dialog;
import android.content.Context;
import android.graphics.Bitmap;
import android.os.Handler;
import android.text.SpannableString;
import android.text.TextUtils;
import android.text.method.LinkMovementMethod;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnLayoutChangeListener;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.ResettersForTesting;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.editors.EditorDialogView;
import org.chromium.chrome.browser.autofill.editors.EditorObserverForTest;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.PauseResumeWithNativeObserver;
import org.chromium.chrome.browser.payments.ShippingStrings;
import org.chromium.chrome.browser.payments.ui.PaymentRequestSection.LineItemBreakdownSection;
import org.chromium.chrome.browser.payments.ui.PaymentRequestSection.OptionSection;
import org.chromium.chrome.browser.payments.ui.PaymentRequestSection.SectionSeparator;
import org.chromium.chrome.browser.payments.ui.PaymentUiService.PaymentUisShowStateReconciler;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.components.autofill.EditableOption;
import org.chromium.components.browser_ui.widget.FadingEdgeScrollView;
import org.chromium.components.browser_ui.widget.animation.FocusAnimator;
import org.chromium.components.payments.InputProtector;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.interpolators.Interpolators;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;
import org.chromium.ui.widget.TextViewWithClickableSpans;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.List;

/** The PaymentRequest UI. */
public class PaymentRequestUI
        implements DimmingDialog.OnDismissListener,
                View.OnClickListener,
                PaymentRequestSection.SectionDelegate,
                PauseResumeWithNativeObserver {
    @IntDef({
        DataType.SHIPPING_ADDRESSES,
        DataType.SHIPPING_OPTIONS,
        DataType.CONTACT_DETAILS,
        DataType.PAYMENT_METHODS
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface DataType {
        int SHIPPING_ADDRESSES = 1;
        int SHIPPING_OPTIONS = 2;
        int CONTACT_DETAILS = 3;
        int PAYMENT_METHODS = 4;
    }

    @IntDef({
        SelectionResult.ASYNCHRONOUS_VALIDATION,
        SelectionResult.EDITOR_LAUNCH,
        SelectionResult.NONE
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface SelectionResult {
        int ASYNCHRONOUS_VALIDATION = 1;
        int EDITOR_LAUNCH = 2;
        int NONE = 3;
    }

    /** The interface to be implemented by the consumer of the PaymentRequest UI. */
    public interface Client {
        /**
         * Asynchronously returns the default payment information.
         * @param waitForUpdatedDetails Whether the payment details is pending for updating.
         * @param callback Retrieves the data to show in the initial PaymentRequest UI.
         */
        void getDefaultPaymentInformation(
                boolean waitForUpdatedDetails, Callback<PaymentInformation> callback);

        /**
         * Asynchronously returns the full bill. Includes the total price and its breakdown into
         * individual line items.
         */
        void getShoppingCart(Callback<ShoppingCart> callback);

        /**
         * Asynchronously returns the full list of options for the given type.
         *
         * @param optionType Data being updated.
         * @param callback   Callback to run when the data has been fetched.
         */
        void getSectionInformation(@DataType int optionType, Callback<SectionInformation> callback);

        /**
         * Called when the user changes one of their payment options.
         *
         * If this method returns {@link SelectionResult.ASYNCHRONOUS_VALIDATION}, then:
         * + The added option should be asynchronously verified.
         * + The section should be disabled and a progress spinny should be shown while the option
         *   is being verified.
         * + The checkedCallback will be invoked with the results of the check and updated
         *   information.
         *
         * If this method returns {@link SelectionResult.EDITOR_LAUNCH}, then:
         * + Interaction with UI should be disabled until updateSection() is called.
         *
         * For example, if the website needs a shipping address to calculate shipping options, then
         * calling onSectionOptionSelected(DataType.SHIPPING_ADDRESS, option, checkedCallback) will
         * return true. When the website updates the shipping options, the checkedCallback will be
         * invoked.
         *
         * @param optionType        Data being updated.
         * @param option            Value of the data being updated.
         * @param checkedCallback   The callback after an asynchronous check has completed.
         * @return The result of the selection.
         */
        @SelectionResult
        int onSectionOptionSelected(
                @DataType int optionType,
                EditableOption option,
                Callback<PaymentInformation> checkedCallback);

        /**
         * Called when the user clicks edit icon (pencil icon) on the payment option in a section.
         *
         * If this method returns {@link SelectionResult.ASYNCHRONOUS_VALIDATION}, then:
         * + The edited option should be asynchronously verified.
         * + The section should be disabled and a progress spinny should be shown while the option
         *   is being verified.
         * + The checkedCallback will be invoked with the results of the check and updated
         *   information.
         *
         * If this method returns {@link SelectionResult.EDITOR_LAUNCH}, then:
         * + Interaction with UI should be disabled until updateSection() is called.
         *
         * @param optionType      Data being updated.
         * @param option          The option to be edited.
         * @param checkedCallback The callback after an asynchronous check has completed.
         * @return The result of the edit request.
         */
        @SelectionResult
        int onSectionEditOption(
                @DataType int optionType,
                EditableOption option,
                Callback<PaymentInformation> checkedCallback);

        /**
         * Called when the user clicks on the "Add" button for a section.
         *
         * If this method returns {@link SelectionResult.ASYNCHRONOUS_VALIDATION}, then:
         * + The added option should be asynchronously verified.
         * + The section should be disabled and a progress spinny should be shown while the option
         *   is being verified.
         * + The checkedCallback will be invoked with the results of the check and updated
         *   information.
         *
         * If this method returns {@link SelectionResult.EDITOR_LAUNCH}, then:
         * + Interaction with UI should be disabled until updateSection() is called.
         *
         * @param optionType      Data being updated.
         * @param checkedCallback The callback after an asynchronous check has completed.
         * @return The result of the selection.
         */
        @SelectionResult
        int onSectionAddOption(
                @DataType int optionType, Callback<PaymentInformation> checkedCallback);

        /**
         * Called when the user clicks on the “Pay” button. If this method returns true, the UI is
         * disabled and is showing a spinner. Otherwise, the UI is hidden.
         */
        boolean onPayClicked(
                EditableOption selectedShippingAddress,
                EditableOption selectedShippingOption,
                EditableOption selectedPaymentMethod);

        /**
         * Called when the user dismisses the UI via the “back” button on their phone
         * or the “X” button in UI.
         */
        void onDismiss();

        /** Called when the user clicks on 'Settings' to control card and address options. */
        void onCardAndAddressSettingsClicked();

        /**
         * Returns true when shipping address is requested and the selected payment method cannot
         * provide it.
         */
        boolean shouldShowShippingSection();

        /**
         * Returns true when payer's contact details is requested and the selected payment method
         * cannot provide it.
         */
        boolean shouldShowContactSection();
    }

    /** A test-only observer for PaymentRequest UI. */
    public interface PaymentRequestObserverForTest {
        /** Called immediately when PaymentRequestUI#show() is called. */
        void onPaymentRequestUIShow(PaymentRequestUI ui);

        /** Called when clicks on the UI are possible. */
        void onPaymentRequestReadyForInput(PaymentRequestUI ui);

        /** Called when clicks on the PAY button are possible. */
        void onPaymentRequestReadyToPay(PaymentRequestUI ui);

        /** Called when the UI has been updated to reflect checking a selected option. */
        void onPaymentRequestSelectionChecked(PaymentRequestUI ui);

        /** Called when the result UI is showing. */
        void onPaymentRequestResultReady(PaymentRequestUI ui);
    }

    /** Helper to notify tests of an event only once. */
    private static class NotifierForTest {
        private final Handler mHandler;
        private final Runnable mNotification;
        private boolean mNotificationPending;

        /**
         * Constructs the helper to notify tests for an event.
         *
         * @param notification The callback that notifies the test of an event.
         */
        public NotifierForTest(final Runnable notification) {
            mHandler = new Handler();
            mNotification =
                    new Runnable() {
                        @Override
                        public void run() {
                            notification.run();
                            mNotificationPending = false;
                        }
                    };
        }

        /** Schedules a single notification for test, even if called only once. */
        public void run() {
            if (mNotificationPending) return;
            mNotificationPending = true;
            mHandler.post(mNotification);
        }
    }

    /**
     * Length of the animation to either show the UI or expand it to full height.
     * Note that click of 'Pay' button is not accepted until the animation is done, so this duration
     * also serves the function of preventing the user from accidentally double-clicking on the
     * screen when triggering payment and thus authorizing unwanted transaction.
     */
    private static final int DIALOG_ENTER_ANIMATION_MS = 225;

    private static PaymentRequestObserverForTest sPaymentRequestObserverForTest;
    private static EditorObserverForTest sEditorObserverForTest;

    /** Notifies tests that the [PAY] button can be clicked. */
    private final NotifierForTest mReadyToPayNotifierForTest;

    private final Context mContext;
    private final Client mClient;
    private final boolean mShowDataSource;
    private final PaymentUisShowStateReconciler mPaymentUisShowStateReconciler;
    private final Profile mProfile;

    /**
     * The top level container of this UI. When needing to call show() or hide(), use {@link
     * PaymentUisShowStateReconciler}'s showPaymentRequestDialogWhenNoBottomSheet() and
     * hidePaymentRequestDialog() instead.
     */
    private final DimmingDialog mDialog;

    private final EditorDialogView mEditorDialog;
    private final ViewGroup mRequestView;
    private final Callback<PaymentInformation> mUpdateSectionsCallback;
    private final ShippingStrings mShippingStrings;
    private final int mAnimatorTranslation;

    private FadingEdgeScrollView mPaymentContainer;
    private LinearLayout mPaymentContainerLayout;
    private TextView mRetryErrorView;
    private ViewGroup mBottomBar;
    private Button mEditButton;
    private Button mPayButton;
    private View mCloseButton;
    private View mSpinnyLayout;

    private LineItemBreakdownSection mOrderSummarySection;
    private OptionSection mShippingAddressSection;
    private OptionSection mShippingOptionSection;
    private OptionSection mContactDetailsSection;
    private OptionSection mPaymentMethodSection;
    private List<SectionSeparator> mSectionSeparators;

    private PaymentRequestSection mSelectedSection;
    private boolean mIsExpandedToFullHeight;
    private boolean mIsProcessingPayClicked;
    private boolean mIsClientClosing;
    private boolean mIsClientCheckingSelection;
    private boolean mIsShowingSpinner;
    private boolean mIsEditingPaymentItem;
    private boolean mIsClosing;

    private SectionInformation mPaymentMethodSectionInformation;
    private SectionInformation mShippingAddressSectionInformation;
    private SectionInformation mShippingOptionsSectionInformation;
    private SectionInformation mContactDetailsSectionInformation;

    private Animator mSheetAnimator;
    private FocusAnimator mSectionAnimator;

    private InputProtector mInputProtector = new InputProtector();

    /**
     * Builds the UI for PaymentRequest.
     *
     * @param activity              The activity on top of which the UI should be displayed.
     * @param client                The consumer of the PaymentRequest UI.
     * @param showDataSource        Whether the UI should describe the source of Autofill data.
     * @param title                 The title to show at the top of the UI. This can be, for
     *                              example, the &lt;title&gt; of the merchant website. If the
     *                              string is too long for UI, it elides at the end.
     * @param origin                The origin (https://tools.ietf.org/html/rfc6454) to show under
     *                              the title. For example, "https://shop.momandpop.com". If the
     *                              origin is too long for the UI, it should elide according to:
     * https://www.chromium.org/Home/chromium-security/enamel#TOC-Eliding-Origin-Names-And-Hostnames
     * @param securityLevel   The security level of the page that invoked PaymentRequest.
     * @param shippingStrings The string resource identifiers to use in the shipping sections.
     * @param profile         The current profile that creates the PaymentRequestUI.
     */
    public PaymentRequestUI(
            Activity activity,
            Client client,
            boolean showDataSource,
            String title,
            String origin,
            int securityLevel,
            ShippingStrings shippingStrings,
            PaymentUisShowStateReconciler paymentUisShowStateReconciler,
            Profile profile) {
        mContext = activity;
        mClient = client;
        mShowDataSource = showDataSource;
        mAnimatorTranslation =
                mContext.getResources().getDimensionPixelSize(R.dimen.payments_ui_translation);
        mProfile = profile;

        mReadyToPayNotifierForTest =
                new NotifierForTest(
                        new Runnable() {
                            @Override
                            public void run() {
                                if (sPaymentRequestObserverForTest != null
                                        && isAcceptingUserInput()
                                        && mPayButton.isEnabled()) {
                                    sPaymentRequestObserverForTest.onPaymentRequestReadyToPay(
                                            PaymentRequestUI.this);
                                }
                            }
                        });

        // This callback will be fired if mIsClientCheckingSelection is true.
        mUpdateSectionsCallback =
                new Callback<PaymentInformation>() {
                    @Override
                    public void onResult(PaymentInformation result) {
                        mIsClientCheckingSelection = false;
                        updateOrderSummarySection(result.getShoppingCart());
                        if (mClient.shouldShowShippingSection()) {
                            updateSection(
                                    DataType.SHIPPING_ADDRESSES, result.getShippingAddresses());
                            updateSection(DataType.SHIPPING_OPTIONS, result.getShippingOptions());
                        }
                        if (mClient.shouldShowContactSection()) {
                            updateSection(DataType.CONTACT_DETAILS, result.getContactDetails());
                        }
                        updateSection(DataType.PAYMENT_METHODS, result.getPaymentMethods());
                        if (mShippingAddressSectionInformation != null
                                && mShippingAddressSectionInformation.getSelectedItem() == null) {
                            expand(mShippingAddressSection);
                        } else {
                            expand(null);
                        }
                        updatePayButtonEnabled();
                        notifySelectionChecked();
                    }
                };

        mShippingStrings = shippingStrings;

        mRequestView =
                (ViewGroup) LayoutInflater.from(mContext).inflate(R.layout.payment_request, null);
        prepareRequestView(mContext, title, origin, securityLevel, profile);

        mEditorDialog = new EditorDialogView(activity, profile);
        DimmingDialog.setVisibleStatusBarIconColor(mEditorDialog.getWindow());

        mDialog = new DimmingDialog(activity, this);
        mPaymentUisShowStateReconciler = paymentUisShowStateReconciler;
    }

    /**
     * Shows the PaymentRequest UI. This will dim the background behind the PaymentRequest UI.
     * @param waitForUpdatedDetails Whether the payment details is pending to be updated.
     */
    public void show(boolean waitForUpdatedDetails) {
        mInputProtector.markShowTime();
        mDialog.addBottomSheetView(mRequestView);
        mPaymentUisShowStateReconciler.showPaymentRequestDialogWhenNoBottomSheet();
        mClient.getDefaultPaymentInformation(
                waitForUpdatedDetails,
                new Callback<PaymentInformation>() {
                    @Override
                    public void onResult(PaymentInformation result) {
                        updateOrderSummarySection(result.getShoppingCart());

                        if (mClient.shouldShowShippingSection()) {
                            updateSection(
                                    DataType.SHIPPING_ADDRESSES, result.getShippingAddresses());
                            updateSection(DataType.SHIPPING_OPTIONS, result.getShippingOptions());
                        }

                        if (mClient.shouldShowContactSection()) {
                            updateSection(DataType.CONTACT_DETAILS, result.getContactDetails());
                        }

                        mPaymentMethodSection.setDisplaySummaryInSingleLineInNormalMode(
                                result.getPaymentMethods()
                                        .getDisplaySelectedItemSummaryInSingleLineInNormalMode());
                        updateSection(DataType.PAYMENT_METHODS, result.getPaymentMethods());
                        updatePayButtonEnabled();

                        // Hide the loading indicators and show the real sections.
                        changeSpinnerVisibility(false);
                        mRequestView.addOnLayoutChangeListener(new SheetEnlargingAnimator(false));
                    }
                });
        if (sPaymentRequestObserverForTest != null) {
            sPaymentRequestObserverForTest.onPaymentRequestUIShow(PaymentRequestUI.this);
        }
    }

    /**
     * Dim the background without showing any UI. No UI will be interactive. The dimming stops when
     * close() is called. This is useful for the skip-ui scenario, i.e., launching a payment handler
     * directly without showing a PaymentRequest UI first in cases where only one payment handler is
     * available.
     */
    public void dimBackground() {
        // Intentionally do not add the bottom sheet view to mDialog so that only the scrim part of
        // the dialog will be shown.
        mPaymentUisShowStateReconciler.showPaymentRequestDialogWhenNoBottomSheet();
    }

    /**
     * Prepares the PaymentRequestUI for initial display.
     *
     * TODO(dfalcantara): Ideally, everything related to the request and its views would just be put
     *                    into its own class but that'll require yanking out a lot of this class.
     *
     * @param context       The application context.
     * @param title         Title of the page.
     * @param origin        The RFC6454 origin of the page.
     * @param securityLevel The security level of the page that invoked PaymentRequest.
     * @param profile       The current profile to pass PaymentRequestHeader.
     */
    private void prepareRequestView(
            Context context, String title, String origin, int securityLevel, Profile profile) {
        mSpinnyLayout = mRequestView.findViewById(R.id.payment_request_spinny);
        assert mSpinnyLayout.getVisibility() == View.VISIBLE;
        mIsShowingSpinner = true;

        // Indicate that we're preparing the dialog for display.
        TextView messageView = mRequestView.findViewById(R.id.message);
        messageView.setText(R.string.payments_loading_message);

        ((PaymentRequestHeader) mRequestView.findViewById(R.id.header))
                .setTitleAndOrigin(title, origin, securityLevel, profile);

        // Set up the buttons.
        mCloseButton = mRequestView.findViewById(R.id.close_button);
        mCloseButton.setOnClickListener(this);
        mBottomBar = mRequestView.findViewById(R.id.bottom_bar);
        mPayButton = mBottomBar.findViewById(R.id.button_primary);
        mPayButton.setOnClickListener(this);
        mPayButton.setText(R.string.payments_continue_button);
        mEditButton = mBottomBar.findViewById(R.id.button_secondary);
        mEditButton.setOnClickListener(this);

        // Create all the possible sections.
        mSectionSeparators = new ArrayList<>();
        mPaymentContainer = mRequestView.findViewById(R.id.option_container);
        mPaymentContainerLayout = mRequestView.findViewById(R.id.payment_container_layout);
        mRetryErrorView = mRequestView.findViewById(R.id.retry_error);
        mOrderSummarySection =
                new LineItemBreakdownSection(
                        context,
                        context.getString(R.string.payments_order_summary_label),
                        this,
                        context.getString(R.string.payments_updated_label));
        mShippingAddressSection =
                new OptionSection(
                        context, context.getString(mShippingStrings.getAddressLabel()), this);
        mShippingOptionSection =
                new OptionSection(
                        context, context.getString(mShippingStrings.getOptionLabel()), this);
        mContactDetailsSection =
                new OptionSection(
                        context, context.getString(R.string.payments_contact_details_label), this);
        mPaymentMethodSection =
                new OptionSection(
                        context,
                        context.getString(R.string.payments_method_of_payment_label),
                        this);

        // Display the summary of the selected address in multiple lines on bottom sheet.
        mShippingAddressSection.setDisplaySummaryInSingleLineInNormalMode(false);

        // Display selected shipping option name in the left summary text view and
        // the cost in the right summary text view on bottom sheet.
        mShippingOptionSection.setSplitSummaryInDisplayModeNormal(true);

        // The user cannot add new shipping options or payment methods.
        mShippingOptionSection.setCanAddItems(false);
        mPaymentMethodSection.setCanAddItems(false);

        // Add the necessary sections to the layout.
        mPaymentContainerLayout.addView(
                mOrderSummarySection,
                new LinearLayout.LayoutParams(
                        LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));

        SectionSeparator shippingSectionSeparator = new SectionSeparator(mPaymentContainerLayout);
        mSectionSeparators.add(shippingSectionSeparator);
        mPaymentContainerLayout.addView(
                mShippingAddressSection,
                new LinearLayout.LayoutParams(
                        LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));

        // The shipping breakout sections are visible only if they are needed.
        if (!mClient.shouldShowShippingSection()) {
            mShippingAddressSection.setVisibility(View.GONE);
            shippingSectionSeparator.setVisibility(View.GONE);
        }

        mSectionSeparators.add(new SectionSeparator(mPaymentContainerLayout));
        mPaymentContainerLayout.addView(
                mPaymentMethodSection,
                new LinearLayout.LayoutParams(
                        LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));

        SectionSeparator contactSectionSeparator = new SectionSeparator(mPaymentContainerLayout);
        mSectionSeparators.add(contactSectionSeparator);
        mPaymentContainerLayout.addView(
                mContactDetailsSection,
                new LinearLayout.LayoutParams(
                        LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));

        // Contact details are optional, depending on the merchant website, and whether or not the
        // selected payment app can provide them.
        if (!mClient.shouldShowContactSection()) {
            mContactDetailsSection.setVisibility(View.GONE);
            contactSectionSeparator.setVisibility(View.GONE);
        }

        mRequestView.addOnLayoutChangeListener(new PeekingAnimator());

        // Enabled in updatePayButtonEnabled() when the user has selected all payment options.
        mPayButton.setEnabled(false);
    }

    /**
     * Closes the UI. Can be invoked in response to, for example:
     * <ul>
     *  <li>Successfully processing the payment.</li>
     *  <li>Failure to process the payment.</li>
     *  <li>The JavaScript calling the abort() method in PaymentRequest API.</li>
     *  <li>The PaymentRequest JavaScript object being destroyed.</li>
     * </ul>
     *
     * Does not call Client.onDismissed().
     *
     * Should not be called multiple times.
     */
    public void close() {
        mIsClientClosing = true;

        dismissDialog(false);

        if (sPaymentRequestObserverForTest != null) {
            sPaymentRequestObserverForTest.onPaymentRequestResultReady(this);
        }
    }

    /**
     * Sets the icon in the top left of the UI. This can be, for example, the favicon of the
     * merchant website. This is not a part of the constructor because favicon retrieval is
     * asynchronous.
     *
     * @param bitmap The bitmap to show next to the title.
     */
    public void setTitleBitmap(Bitmap bitmap) {
        ((PaymentRequestHeader) mRequestView.findViewById(R.id.header)).setTitleBitmap(bitmap);
    }

    /**
     * Sets the retry error message. This is used to display error message on the header UI when
     * retry() is called on merchant side. The error message may be reset when users click 'Pay'
     * button or expand any section.
     *
     * @param error The error message to display on the header.
     */
    public void setRetryErrorMessage(String error) {
        if (mRetryErrorView == null) return;

        mRetryErrorView.setText(error);
        if (TextUtils.isEmpty(error)) {
            mRetryErrorView.setVisibility(View.GONE);
        } else {
            if (mIsExpandedToFullHeight) {
                // Add padding instead of margin to let getMeasuredHeight return correct value for
                // section resize animation.
                int paddingSize =
                        mContext.getResources()
                                .getDimensionPixelSize(R.dimen.editor_dialog_section_large_spacing);
                mRetryErrorView.setPaddingRelative(0, paddingSize, 0, paddingSize);
            } else {
                mRetryErrorView.setPaddingRelative(0, 0, 0, 0);
            }
            mRetryErrorView.setVisibility(View.VISIBLE);
        }
    }

    /**
     * Updates the line items in response to a changed shipping address or option.
     *
     * @param cart The shopping cart, including the line items and the total.
     */
    public void updateOrderSummarySection(ShoppingCart cart) {
        if (cart == null || cart.getTotal() == null) {
            mOrderSummarySection.setVisibility(View.GONE);
        } else {
            mOrderSummarySection.setVisibility(View.VISIBLE);
            mOrderSummarySection.update(cart);
        }
    }

    /**
     * Updates the UI to account for changes in different sections information.
     *
     * @param whichSection The type of the updated section.
     * @param section The updated section information.
     */
    public void updateSection(@DataType int whichSection, SectionInformation section) {
        if (whichSection == DataType.SHIPPING_ADDRESSES) {
            mShippingAddressSectionInformation = section;
            mShippingAddressSection.update(section);
        } else if (whichSection == DataType.SHIPPING_OPTIONS) {
            mShippingOptionsSectionInformation = section;
            mShippingOptionSection.update(section);
            addShippingOptionSectionIfNecessary();
        } else if (whichSection == DataType.CONTACT_DETAILS) {
            mContactDetailsSectionInformation = section;
            mContactDetailsSection.update(section);
        } else if (whichSection == DataType.PAYMENT_METHODS) {
            mPaymentMethodSectionInformation = section;
            mPaymentMethodSection.update(section);
        }

        boolean isFinishingEditItem = mIsEditingPaymentItem;
        mIsEditingPaymentItem = false;
        updateSectionButtons();
        updatePayButtonEnabled();

        // Notify ready for input for test if this is finishing editing item.
        if (isFinishingEditItem) notifyReadyForInput();
    }

    // Only add shipping option section once there are shipping options.
    private void addShippingOptionSectionIfNecessary() {
        if (!mClient.shouldShowShippingSection()
                || mShippingOptionsSectionInformation.isEmpty()
                || mPaymentContainerLayout.indexOfChild(mShippingOptionSection) != -1) {
            return;
        }

        // Shipping option section is added below shipping address section.
        int addressSectionIndex = mPaymentContainerLayout.indexOfChild(mShippingAddressSection);
        SectionSeparator sectionSeparator =
                new SectionSeparator(mPaymentContainerLayout, addressSectionIndex + 1);
        mSectionSeparators.add(sectionSeparator);
        if (mIsExpandedToFullHeight) sectionSeparator.expand();
        mPaymentContainerLayout.addView(
                mShippingOptionSection,
                addressSectionIndex + 2,
                new LinearLayout.LayoutParams(
                        LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));
        ViewUtils.requestLayout(
                mPaymentContainerLayout, "PaymentRequestUI.addShippingOptionSectionIfNecessary");
    }

    /**
     * Notifies the UI about the changes in selected payment method.
     *
     * @param paymentInformation The updated payment information.
     */
    public void selectedPaymentMethodUpdated(PaymentInformation paymentInformation) {
        if (mClient.shouldShowShippingSection()
                && mShippingAddressSection.getVisibility() == View.GONE) {
            updateSection(DataType.SHIPPING_ADDRESSES, paymentInformation.getShippingAddresses());
            updateSection(DataType.SHIPPING_OPTIONS, paymentInformation.getShippingOptions());

            // Show shipping address section and its separator.
            mShippingAddressSection.setVisibility(View.VISIBLE);
            int addressSectionIndex = mPaymentContainerLayout.indexOfChild(mShippingAddressSection);
            mPaymentContainerLayout.getChildAt(addressSectionIndex - 1).setVisibility(View.VISIBLE);

            // Show shipping option section (if it exists) and its separator.
            int shippingOptionSectionIndex =
                    mPaymentContainerLayout.indexOfChild(mShippingOptionSection);
            if (shippingOptionSectionIndex != -1) {
                mShippingOptionSection.setVisibility(View.VISIBLE);
                mPaymentContainerLayout
                        .getChildAt(shippingOptionSectionIndex - 1)
                        .setVisibility(View.VISIBLE);
            }

        } else if (!mClient.shouldShowShippingSection()
                && mShippingAddressSection.getVisibility() == View.VISIBLE) {
            // Hide shipping address section and its separator.
            mShippingAddressSection.setVisibility(View.GONE);
            int addressSectionIndex = mPaymentContainerLayout.indexOfChild(mShippingAddressSection);
            mPaymentContainerLayout.getChildAt(addressSectionIndex - 1).setVisibility(View.GONE);

            // Hide shipping option section (if exists) and its separator.
            int shippingOptionSectionIndex =
                    mPaymentContainerLayout.indexOfChild(mShippingOptionSection);
            if (shippingOptionSectionIndex != -1) {
                mShippingOptionSection.setVisibility(View.GONE);
                mPaymentContainerLayout
                        .getChildAt(shippingOptionSectionIndex - 1)
                        .setVisibility(View.GONE);
            }
        }
        if (mClient.shouldShowContactSection()
                && mContactDetailsSection.getVisibility() == View.GONE) {
            updateSection(DataType.CONTACT_DETAILS, paymentInformation.getContactDetails());

            // Show contact details section and its separator.
            mContactDetailsSection.setVisibility(View.VISIBLE);
            int contactSectionIndex = mPaymentContainerLayout.indexOfChild(mContactDetailsSection);
            mPaymentContainerLayout.getChildAt(contactSectionIndex - 1).setVisibility(View.VISIBLE);
        } else if (!mClient.shouldShowContactSection()
                && mContactDetailsSection.getVisibility() == View.VISIBLE) {
            // Hide contact details section and its separator.
            mContactDetailsSection.setVisibility(View.GONE);
            int contactSectionIndex = mPaymentContainerLayout.indexOfChild(mContactDetailsSection);
            mPaymentContainerLayout.getChildAt(contactSectionIndex - 1).setVisibility(View.GONE);
        }

        ViewUtils.requestLayout(
                mPaymentContainerLayout, "PaymentRequestUI.selectedPaymentMethodUpdated");
    }

    @Override
    public void onEditableOptionChanged(
            final PaymentRequestSection section, EditableOption option) {
        @SelectionResult int result = SelectionResult.NONE;
        if (section == mShippingAddressSection
                && mShippingAddressSectionInformation.getSelectedItem() != option) {
            mShippingAddressSectionInformation.setSelectedItem(option);
            result =
                    mClient.onSectionOptionSelected(
                            DataType.SHIPPING_ADDRESSES, option, mUpdateSectionsCallback);
        } else if (section == mShippingOptionSection
                && mShippingOptionsSectionInformation.getSelectedItem() != option) {
            mShippingOptionsSectionInformation.setSelectedItem(option);
            result =
                    mClient.onSectionOptionSelected(
                            DataType.SHIPPING_OPTIONS, option, mUpdateSectionsCallback);
        } else if (section == mContactDetailsSection) {
            mContactDetailsSectionInformation.setSelectedItem(option);
            result =
                    mClient.onSectionOptionSelected(
                            DataType.CONTACT_DETAILS, option, mUpdateSectionsCallback);
        } else if (section == mPaymentMethodSection) {
            mPaymentMethodSectionInformation.setSelectedItem(option);
            result = mClient.onSectionOptionSelected(DataType.PAYMENT_METHODS, option, null);
        }

        updateStateFromResult(section, result);
    }

    @Override
    public void onEditEditableOption(final PaymentRequestSection section, EditableOption option) {
        @SelectionResult int result = SelectionResult.NONE;

        assert section != mOrderSummarySection;
        assert section != mShippingOptionSection;
        if (section == mShippingAddressSection) {
            assert mShippingAddressSectionInformation.getSelectedItem() == option;
            result =
                    mClient.onSectionEditOption(
                            DataType.SHIPPING_ADDRESSES, option, mUpdateSectionsCallback);
        }

        if (section == mContactDetailsSection) {
            assert mContactDetailsSectionInformation.getSelectedItem() == option;
            result = mClient.onSectionEditOption(DataType.CONTACT_DETAILS, option, null);
        }

        if (section == mPaymentMethodSection) {
            assert mPaymentMethodSectionInformation.getSelectedItem() == option;
            result = mClient.onSectionEditOption(DataType.PAYMENT_METHODS, option, null);
        }

        updateStateFromResult(section, result);
    }

    @Override
    public void onAddEditableOption(PaymentRequestSection section) {
        assert section != mShippingOptionSection;

        @SelectionResult int result = SelectionResult.NONE;
        if (section == mShippingAddressSection) {
            result =
                    mClient.onSectionAddOption(
                            DataType.SHIPPING_ADDRESSES, mUpdateSectionsCallback);
        } else if (section == mContactDetailsSection) {
            result = mClient.onSectionAddOption(DataType.CONTACT_DETAILS, null);
        } else if (section == mPaymentMethodSection) {
            result = mClient.onSectionAddOption(DataType.PAYMENT_METHODS, null);
        }

        updateStateFromResult(section, result);
    }

    void updateStateFromResult(PaymentRequestSection section, @SelectionResult int result) {
        mIsClientCheckingSelection = result == SelectionResult.ASYNCHRONOUS_VALIDATION;
        mIsEditingPaymentItem = result == SelectionResult.EDITOR_LAUNCH;

        if (mIsClientCheckingSelection) {
            mSelectedSection = section;
            updateSectionVisibility();
            section.setDisplayMode(PaymentRequestSection.DISPLAY_MODE_CHECKING);
        } else {
            expand(null);
        }

        updatePayButtonEnabled();
    }

    @Override
    public boolean isBoldLabelNeeded(PaymentRequestSection section) {
        return section == mShippingAddressSection;
    }

    /** @return The common editor user interface. */
    public EditorDialogView getEditorDialog() {
        return mEditorDialog;
    }

    /** Called when user clicks anything in the dialog. */
    // View.OnClickListener implementation.
    @Override
    public void onClick(View v) {
        if (!isAcceptingCloseButton()) return;

        if (v == mCloseButton) {
            dismissDialog(true);
            return;
        }

        if (!isAcceptingUserInput()) return;

        // Users can only expand incomplete sections by clicking on their edit buttons.
        if (v instanceof PaymentRequestSection) {
            PaymentRequestSection section = (PaymentRequestSection) v;
            if (section.getEditButtonState() != EDIT_BUTTON_GONE) return;
        }

        if (v == mOrderSummarySection) {
            expand(mOrderSummarySection);
        } else if (v == mShippingAddressSection) {
            expand(mShippingAddressSection);
        } else if (v == mShippingOptionSection) {
            expand(mShippingOptionSection);
        } else if (v == mContactDetailsSection) {
            expand(mContactDetailsSection);
        } else if (v == mPaymentMethodSection) {
            expand(mPaymentMethodSection);
        } else if (v == mPayButton) {
            processPayButton();
        } else if (v == mEditButton) {
            if (mIsExpandedToFullHeight) {
                dismissDialog(true);
            } else {
                expand(mOrderSummarySection);
            }
        }

        setRetryErrorMessage(null);

        updatePayButtonEnabled();
    }

    /**
     * Dismiss the dialog.
     *
     * @param isAnimated If true, the dialog dismissal is animated.
     */
    private void dismissDialog(boolean isAnimated) {
        mIsClosing = true;
        mDialog.dismiss(isAnimated);
    }

    private void processPayButton() {
        assert !mIsShowingSpinner;
        mIsProcessingPayClicked = true;

        boolean shouldShowSpinner =
                mClient.onPayClicked(
                        mShippingAddressSectionInformation == null
                                ? null
                                : mShippingAddressSectionInformation.getSelectedItem(),
                        mShippingOptionsSectionInformation == null
                                ? null
                                : mShippingOptionsSectionInformation.getSelectedItem(),
                        mPaymentMethodSectionInformation.getSelectedItem());

        if (shouldShowSpinner) {
            changeSpinnerVisibility(true);
        } else {
            mPaymentUisShowStateReconciler.hidePaymentRequestDialog();
        }
    }

    /** Called when user cancelled out of the UI that was shown after they clicked [PAY] button. */
    public void onPayButtonProcessingCancelled() {
        assert mIsProcessingPayClicked;
        mIsProcessingPayClicked = false;
        changeSpinnerVisibility(false);
        mPaymentUisShowStateReconciler.showPaymentRequestDialogWhenNoBottomSheet();
        updatePayButtonEnabled();
    }

    /**
     *  Called to show the processing message after payment details have been loaded in the case the
     *  payment request UI has been skipped.
     */
    public void showProcessingMessageAfterUiSkip() {
        // Button was clicked before but not marked as clicked because we skipped the UI.
        mIsProcessingPayClicked = true;
        showProcessingMessage();
    }

    /**
     * Called when the user has clicked on pay. The message is shown while the payment information
     * is processed right until a confirmation from the merchant is received.
     */
    public void showProcessingMessage() {
        assert mIsProcessingPayClicked;

        changeSpinnerVisibility(true);
        mPaymentUisShowStateReconciler.showPaymentRequestDialogWhenNoBottomSheet();
    }

    private void changeSpinnerVisibility(boolean showSpinner) {
        if (mIsShowingSpinner == showSpinner) return;
        mIsShowingSpinner = showSpinner;

        if (showSpinner) {
            mPaymentContainer.setVisibility(View.GONE);
            mBottomBar.setVisibility(View.GONE);
            mCloseButton.setVisibility(View.GONE);
            mSpinnyLayout.setVisibility(View.VISIBLE);

            // Turn the bottom sheet back into a collapsed bottom sheet showing only the spinner.
            // TODO(dfalcantara): Animate this: https://crbug.com/621955
            ((FrameLayout.LayoutParams) mRequestView.getLayoutParams()).height =
                    LayoutParams.WRAP_CONTENT;
            ViewUtils.requestLayout(mRequestView, "PaymentRequestUI.changeSpinnerVisibility show");
        } else {
            mPaymentContainer.setVisibility(View.VISIBLE);
            mBottomBar.setVisibility(View.VISIBLE);
            mCloseButton.setVisibility(View.VISIBLE);
            mSpinnyLayout.setVisibility(View.GONE);

            if (mIsExpandedToFullHeight) {
                ((FrameLayout.LayoutParams) mRequestView.getLayoutParams()).height =
                        LayoutParams.MATCH_PARENT;
                ViewUtils.requestLayout(
                        mRequestView,
                        "PaymentRequestUI.changeSpinnerVisibility expanded to full height");
            }
        }
    }

    private void updatePayButtonEnabled() {
        boolean contactInfoOk =
                !mClient.shouldShowContactSection()
                        || (mContactDetailsSectionInformation != null
                                && mContactDetailsSectionInformation.getSelectedItem() != null);
        boolean shippingInfoOk =
                !mClient.shouldShowShippingSection()
                        || (mShippingAddressSectionInformation != null
                                && mShippingAddressSectionInformation.getSelectedItem() != null);
        boolean shippingOptionInfoOk =
                !mClient.shouldShowShippingSection()
                        || (mShippingOptionsSectionInformation != null
                                && mShippingOptionsSectionInformation.getSelectedItem() != null);
        mPayButton.setEnabled(
                contactInfoOk
                        && shippingInfoOk
                        && shippingOptionInfoOk
                        && mPaymentMethodSectionInformation != null
                        && mPaymentMethodSectionInformation.getSelectedItem() != null
                        && !mIsClientCheckingSelection
                        && !mIsEditingPaymentItem
                        && !mIsClosing);

        mReadyToPayNotifierForTest.run();
    }

    /** @return Whether or not the dialog can be closed via the X close button. */
    private boolean isAcceptingCloseButton() {
        assert mInputProtector != null;
        return !mDialog.isAnimatingDisappearance()
                && mSheetAnimator == null
                && mSectionAnimator == null
                && !mIsProcessingPayClicked
                && !mIsEditingPaymentItem
                && !mIsClosing
                && mInputProtector.shouldInputBeProcessed();
    }

    /** @return Whether or not the dialog is accepting user input. */
    @Override
    public boolean isAcceptingUserInput() {
        return isAcceptingCloseButton()
                && mPaymentMethodSectionInformation != null
                && !mIsClientCheckingSelection;
    }

    /**
     * Sets the observer to be called when the shipping address section gains or loses focus.
     *
     * @param observer The observer to notify.
     */
    public void setShippingAddressSectionFocusChangedObserver(
            OptionSection.FocusChangedObserver observer) {
        mShippingAddressSection.setOptionSectionFocusChangedObserver(observer);
    }

    private void expand(PaymentRequestSection section) {
        if (!mIsExpandedToFullHeight) {
            // Container now takes the full height of the screen, animating towards it.
            mRequestView.getLayoutParams().height = LayoutParams.MATCH_PARENT;
            mRequestView.addOnLayoutChangeListener(new SheetEnlargingAnimator(true));

            // New separators appear at the top and bottom of the list.
            mPaymentContainer.setEdgeVisibility(
                    FadingEdgeScrollView.EdgeType.HARD, FadingEdgeScrollView.EdgeType.FADING);
            mSectionSeparators.add(new SectionSeparator(mPaymentContainerLayout, -1));

            // Add a link to Autofill settings.
            addCardAndAddressOptionsSettingsView(mPaymentContainerLayout);

            // Expand all the dividers.
            for (int i = 0; i < mSectionSeparators.size(); i++) mSectionSeparators.get(i).expand();
            ViewUtils.requestLayout(mPaymentContainerLayout, "PaymentRequestUI.expand");

            // Switch the 'edit' button to a 'cancel' button.
            mEditButton.setText(mContext.getString(R.string.cancel));

            // Disable all but the first button.
            updateSectionButtons();

            mIsExpandedToFullHeight = true;
        }

        // Update the section contents when they're selected.
        mSelectedSection = section;
        if (mSelectedSection == mOrderSummarySection) {
            mClient.getShoppingCart(
                    new Callback<ShoppingCart>() {
                        @Override
                        public void onResult(ShoppingCart result) {
                            updateOrderSummarySection(result);
                            updateSectionVisibility();
                        }
                    });
        } else if (mSelectedSection == mShippingAddressSection) {
            mClient.getSectionInformation(
                    DataType.SHIPPING_ADDRESSES,
                    createUpdateSectionCallback(DataType.SHIPPING_ADDRESSES));
        } else if (mSelectedSection == mShippingOptionSection) {
            mClient.getSectionInformation(
                    DataType.SHIPPING_OPTIONS,
                    createUpdateSectionCallback(DataType.SHIPPING_OPTIONS));
        } else if (mSelectedSection == mContactDetailsSection) {
            mClient.getSectionInformation(
                    DataType.CONTACT_DETAILS,
                    createUpdateSectionCallback(DataType.CONTACT_DETAILS));
        } else if (mSelectedSection == mPaymentMethodSection) {
            mClient.getSectionInformation(
                    DataType.PAYMENT_METHODS,
                    createUpdateSectionCallback(DataType.PAYMENT_METHODS));
        } else {
            updateSectionVisibility();
        }
    }

    private void addCardAndAddressOptionsSettingsView(LinearLayout parent) {
        String message;
        if (!mShowDataSource) {
            message = mContext.getString(R.string.payments_card_and_address_settings);
        } else {
            String email = getSignedInUsersEmail();
            if (email != null) {
                message =
                        mContext.getString(
                                R.string.payments_card_and_address_settings_signed_in, email);
            } else {
                message =
                        mContext.getString(R.string.payments_card_and_address_settings_signed_out);
            }
        }

        NoUnderlineClickableSpan settingsSpan =
                new NoUnderlineClickableSpan(
                        mContext, (widget) -> mClient.onCardAndAddressSettingsClicked());
        SpannableString spannableMessage =
                SpanApplier.applySpans(
                        message, new SpanInfo("BEGIN_LINK", "END_LINK", settingsSpan));

        TextView view = new TextViewWithClickableSpans(mContext);
        view.setText(spannableMessage);
        view.setMovementMethod(LinkMovementMethod.getInstance());
        view.setTextAppearance(R.style.TextAppearance_TextMedium_Secondary);

        // Add padding instead of margin to let getMeasuredHeight return correct value for section
        // resize animation.
        int paddingSize =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.editor_dialog_section_large_spacing);
        view.setPaddingRelative(paddingSize, paddingSize, paddingSize, paddingSize);
        parent.addView(view);
    }

    /**
     * Get the email of the signed-in user, if possible. This is not necessarily the email shown or
     * being used for contact details (if they were requested), but is the email that
     * cards/addresses are being synced to.
     *
     * @return The email of signed in user or null.
     */
    private @Nullable String getSignedInUsersEmail() {
        if (mProfile.isOffTheRecord()) {
            return null;
        }

        IdentityManager identityManager =
                IdentityServicesProvider.get().getIdentityManager(mProfile);
        if (identityManager == null) return null;
        @ConsentLevel
        int consentLevel =
                ChromeFeatureList.isEnabled(
                                ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
                        ? ConsentLevel.SIGNIN
                        : ConsentLevel.SYNC;
        CoreAccountInfo info = identityManager.getPrimaryAccountInfo(consentLevel);
        return CoreAccountInfo.getEmailFrom(info);
    }

    private Callback<SectionInformation> createUpdateSectionCallback(@DataType final int type) {
        return new Callback<SectionInformation>() {
            @Override
            public void onResult(SectionInformation result) {
                updateSection(type, result);
                updateSectionVisibility();
            }
        };
    }

    /** Update the display status of each expandable section in the full dialog. */
    private void updateSectionVisibility() {
        startSectionResizeAnimation();
        mOrderSummarySection.focusSection(mSelectedSection == mOrderSummarySection);
        if (mClient.shouldShowShippingSection()) {
            mShippingAddressSection.focusSection(mSelectedSection == mShippingAddressSection);
            mShippingOptionSection.focusSection(mSelectedSection == mShippingOptionSection);
        }
        if (mClient.shouldShowContactSection()) {
            mContactDetailsSection.focusSection(mSelectedSection == mContactDetailsSection);
        }
        mPaymentMethodSection.focusSection(mSelectedSection == mPaymentMethodSection);
        updateSectionButtons();
    }

    /**
     * Updates the enabled/disabled state of each section's edit button.
     *
     * Only the top-most button is enabled -- the others are disabled so the user is directed
     * through the form from top to bottom.
     */
    private void updateSectionButtons() {
        // Disable edit buttons when the client is checking a selection.
        boolean mayEnableButton = !mIsClientCheckingSelection;
        for (int i = 0; i < mPaymentContainerLayout.getChildCount(); i++) {
            View child = mPaymentContainerLayout.getChildAt(i);
            if (!(child instanceof PaymentRequestSection)) continue;

            PaymentRequestSection section = (PaymentRequestSection) child;
            section.setIsEditButtonEnabled(mayEnableButton);
            if (section.getEditButtonState() != EDIT_BUTTON_GONE) mayEnableButton = false;
        }
    }

    /**
     * Called when the dialog is dismissed. Can be caused by:
     * <ul>
     *  <li>User click on the "back" button on the phone.</li>
     *  <li>User click on the "X" button in the top-right corner of the dialog.</li>
     *  <li>User click on the "CANCEL" button on the bottom of the dialog.</li>
     *  <li>Successfully processing the payment.</li>
     *  <li>Failure to process the payment.</li>
     *  <li>The JavaScript calling the abort() method in PaymentRequest API.</li>
     *  <li>The PaymentRequest JavaScript object being destroyed.</li>
     *  <li>User closing all incognito windows with PaymentRequest UI open in an incognito
     *      window.</li>
     * </ul>
     */
    // DimmingDialog.OnDismissListener implementation.
    @Override
    public void onDismiss() {
        mIsClosing = true;
        if (mEditorDialog.isShowing()) mEditorDialog.dismiss();
        if (sEditorObserverForTest != null) sEditorObserverForTest.onEditorDismiss();
        if (!mIsClientClosing) mClient.onDismiss();
    }

    @Override
    public String getAdditionalText(PaymentRequestSection section) {
        if (section == mShippingAddressSection) {
            int selectedItemIndex = mShippingAddressSectionInformation.getSelectedItemIndex();
            if (selectedItemIndex != SectionInformation.NO_SELECTION
                    && selectedItemIndex != SectionInformation.INVALID_SELECTION) {
                return null;
            }

            String customErrorMessage = mShippingAddressSectionInformation.getErrorMessage();
            if (selectedItemIndex == SectionInformation.INVALID_SELECTION
                    && !TextUtils.isEmpty(customErrorMessage)) {
                return customErrorMessage;
            }

            return mContext.getString(
                    selectedItemIndex == SectionInformation.NO_SELECTION
                            ? mShippingStrings.getSelectPrompt()
                            : mShippingStrings.getUnsupported());
        } else if (section == mPaymentMethodSection) {
            return mPaymentMethodSectionInformation.getAdditionalText();
        } else {
            return null;
        }
    }

    @Override
    public boolean isAdditionalTextDisplayingWarning(PaymentRequestSection section) {
        return section == mShippingAddressSection
                && mShippingAddressSectionInformation != null
                && mShippingAddressSectionInformation.getSelectedItemIndex()
                        == SectionInformation.INVALID_SELECTION;
    }

    @Override
    public void onSectionClicked(PaymentRequestSection section) {
        expand(section);
    }

    /**
     * Animates the different sections of the dialog expanding and contracting into their final
     * positions.
     */
    private void startSectionResizeAnimation() {
        Runnable animationEndRunnable =
                new Runnable() {
                    @Override
                    public void run() {
                        mSectionAnimator = null;
                        notifyReadyForInput();
                        mReadyToPayNotifierForTest.run();
                    }
                };

        mSectionAnimator =
                new FocusAnimator(mPaymentContainerLayout, mSelectedSection, animationEndRunnable);
    }

    /**
     * Animates the bottom sheet UI translating upwards from the bottom of the screen.
     * Can be canceled when a {@link SheetEnlargingAnimator} starts and expands the dialog.
     */
    private class PeekingAnimator extends AnimatorListenerAdapter
            implements OnLayoutChangeListener {
        @Override
        public void onLayoutChange(
                View v,
                int left,
                int top,
                int right,
                int bottom,
                int oldLeft,
                int oldTop,
                int oldRight,
                int oldBottom) {
            mRequestView.removeOnLayoutChangeListener(this);

            mSheetAnimator =
                    ObjectAnimator.ofFloat(
                            mRequestView, View.TRANSLATION_Y, mAnimatorTranslation, 0);
            mSheetAnimator.setDuration(DIALOG_ENTER_ANIMATION_MS);
            mSheetAnimator.setInterpolator(Interpolators.LINEAR_OUT_SLOW_IN_INTERPOLATOR);
            mSheetAnimator.addListener(this);
            mSheetAnimator.start();
        }

        @Override
        public void onAnimationEnd(Animator animation) {
            mSheetAnimator = null;
        }
    }

    /** Animates the bottom sheet expanding to a larger sheet. */
    private class SheetEnlargingAnimator extends AnimatorListenerAdapter
            implements OnLayoutChangeListener {
        private final boolean mIsBottomBarLockedInPlace;
        private int mContainerHeightDifference;

        public SheetEnlargingAnimator(boolean isBottomBarLockedInPlace) {
            mIsBottomBarLockedInPlace = isBottomBarLockedInPlace;
        }

        /**
         * Updates the animation.
         *
         * @param progress How far along the animation is.  In the range [0,1], with 1 being done.
         */
        private void update(float progress) {
            // The dialog container initially starts off translated downward, gradually decreasing
            // the translation until it is in the right place on screen.
            float containerTranslation = mContainerHeightDifference * progress;
            mRequestView.setTranslationY(containerTranslation);

            if (mIsBottomBarLockedInPlace) {
                // The bottom bar is translated along the dialog so that is looks like it stays in
                // place at the bottom while the entire bottom sheet is translating upwards.
                mBottomBar.setTranslationY(-containerTranslation);

                // The payment container is sandwiched between the header and the bottom bar.
                // Expansion animates by changing where its "bottom" is, letting its shadows appear
                // and disappear as it changes size.
                int paymentContainerBottom =
                        Math.min(
                                mPaymentContainer.getTop() + mPaymentContainer.getMeasuredHeight(),
                                mBottomBar.getTop());
                mPaymentContainer.setBottom(paymentContainerBottom);
            }
        }

        @Override
        public void onLayoutChange(
                View v,
                int left,
                int top,
                int right,
                int bottom,
                int oldLeft,
                int oldTop,
                int oldRight,
                int oldBottom) {
            if (mSheetAnimator != null) mSheetAnimator.cancel();

            mRequestView.removeOnLayoutChangeListener(this);
            mContainerHeightDifference = (bottom - top) - (oldBottom - oldTop);

            ValueAnimator containerAnimator = ValueAnimator.ofFloat(1f, 0f);
            containerAnimator.addUpdateListener(
                    new AnimatorUpdateListener() {
                        @Override
                        public void onAnimationUpdate(ValueAnimator animation) {
                            float alpha = (Float) animation.getAnimatedValue();
                            update(alpha);
                        }
                    });

            mSheetAnimator = containerAnimator;
            mSheetAnimator.setDuration(DIALOG_ENTER_ANIMATION_MS);
            mSheetAnimator.setInterpolator(Interpolators.LINEAR_OUT_SLOW_IN_INTERPOLATOR);
            mSheetAnimator.addListener(this);
            mSheetAnimator.start();
        }

        @Override
        public void onAnimationEnd(Animator animation) {
            // Reset the layout so that everything is in the expected place.
            mRequestView.setTranslationY(0);
            mBottomBar.setTranslationY(0);
            ViewUtils.requestLayout(
                    mRequestView, "PaymentRequestUI.SheetEnlargingAnimator.onAnimationEnd");

            // Indicate that the dialog is ready to use.
            mSheetAnimator = null;
            notifyReadyForInput();
            mReadyToPayNotifierForTest.run();
        }
    }

    public static void setEditorObserverForTest(EditorObserverForTest editorObserverForTest) {
        sEditorObserverForTest = editorObserverForTest;
        EditorDialogView.setEditorObserverForTest(sEditorObserverForTest);
    }

    public static void setPaymentRequestObserverForTest(
            PaymentRequestObserverForTest paymentRequestObserverForTest) {
        sPaymentRequestObserverForTest = paymentRequestObserverForTest;
        ResettersForTesting.register(() -> sPaymentRequestObserverForTest = null);
    }

    public void setInputProtectorForTest(InputProtector inputProtector) {
        mInputProtector = inputProtector;
    }

    public Dialog getDialogForTest() {
        return mDialog.getDialogForTest();
    }

    public TextView getOrderSummaryTotalTextViewForTest() {
        return mOrderSummarySection.getSummaryRightTextView();
    }

    public LineItemBreakdownSection getOrderSummarySectionForTest() {
        return mOrderSummarySection;
    }

    public OptionSection getShippingAddressSectionForTest() {
        return mShippingAddressSection;
    }

    public OptionSection getShippingOptionSectionForTest() {
        return mShippingOptionSection;
    }

    public ViewGroup getPaymentMethodSectionForTest() {
        return mPaymentMethodSection;
    }

    public PaymentRequestSection getContactDetailsSectionForTest() {
        return mContactDetailsSection;
    }

    private void notifyReadyForInput() {
        if (sPaymentRequestObserverForTest != null && isAcceptingUserInput()) {
            sPaymentRequestObserverForTest.onPaymentRequestReadyForInput(this);
        }
    }

    private void notifySelectionChecked() {
        if (sPaymentRequestObserverForTest != null) {
            sPaymentRequestObserverForTest.onPaymentRequestSelectionChecked(this);
        }
    }

    /**
     * Set the visibility state of the dialog. Use {@link PaymentUisShowStateReconciler}'s
     * showPaymentRequestDialogWhenNoBottomSheet() and hidePaymentRequestDialog() instead of calling
     * this method directly.
     * @param visible True to show the dialog, false to hide the dialog.
     * @return Whether setting visibility is successful.
     */
    public boolean setVisible(boolean visible) {
        if (visible) {
            return mDialog.show();
        } else {
            mDialog.hide();
            return true;
        }
    }

    // Implement PauseResumeWithNativeObserver:
    @Override
    public void onResumeWithNative() {
        // When users come back from an external activity (e.g., app-picker/webauthn), the PR UI
        // somehow shows up even though it's set to GONE (crbug.com/1030416 and
        // crbug.com/1051786). Here we use a workaround to fix it - refresh the dialog window
        // from time to time to force the visual state to respect its visibility attribute.
        mDialog.refresh();
    }

    // Implement PauseResumeWithNativeObserver:
    @Override
    public void onPauseWithNative() {}
}
