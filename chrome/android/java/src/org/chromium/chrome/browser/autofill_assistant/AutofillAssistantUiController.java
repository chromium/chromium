// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.accounts.Account;
import android.os.Bundle;
import android.support.annotation.Nullable;

import com.google.android.libraries.feed.common.functional.Consumer;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.payments.AutofillAssistantPaymentRequest;
import org.chromium.chrome.browser.snackbar.SnackbarManager;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModel.TabSelectionType;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.variations.VariationsAssociatedData;
import org.chromium.content_public.browser.WebContents;
import org.chromium.payments.mojom.PaymentOptions;

import java.util.ArrayList;
import java.util.Calendar;
import java.util.Date;
import java.util.HashMap;
import java.util.Map;

/**
 * Bridge to native side autofill_assistant::UiControllerAndroid. It allows native side to control
 * Autofill Assistant related UIs and forward UI events to native side.
 */
@JNINamespace("autofill_assistant")
public class AutofillAssistantUiController implements AutofillAssistantUiDelegate.Client {
    /** Prefix for Intent extras relevant to this feature. */
    private static final String INTENT_EXTRA_PREFIX =
            "org.chromium.chrome.browser.autofill_assistant.";
    /** Autofill Assistant Study name. */
    private static final String STUDY_NAME = "AutofillAssistant";
    /** Variation url parameter name. */
    private static final String URL_PARAMETER_NAME = "url";

    /** Special parameter that enables the feature. */
    private static final String PARAMETER_ENABLED = "ENABLED";

    /** OAuth2 scope that RPCs require. */
    private static final String AUTH_TOKEN_TYPE =
            "oauth2:https://www.googleapis.com/auth/userinfo.profile";

    private final WebContents mWebContents;
    private final long mUiControllerAndroid;
    private final UiDelegateHolder mUiDelegateHolder;

    private AutofillAssistantPaymentRequest mAutofillAssistantPaymentRequest;

    /**
     * Indicates whether {@link mAccount} has been initialized.
     */
    private boolean mAccountInitialized;

    /**
     * Account to authenticate as when sending RPCs. Not relevant until the accounts have been
     * fetched, and mAccountInitialized set to true. Can still be null after the accounts are
     * fetched, in which case authentication is disabled.
     */
    @Nullable
    private Account mAccount;

    /** If set, fetch the access token once the accounts are fetched. */
    private boolean mShouldFetchAccessToken;

    /**
     * Returns true if all conditions are satisfied to construct an AutofillAssistantUiController.
     *
     * @return True if a controller can be constructed.
     */
    public static boolean isConfigured(Bundle intentExtras) {
        return getBooleanParameter(intentExtras, PARAMETER_ENABLED)
                && !VariationsAssociatedData.getVariationParamValue(STUDY_NAME, URL_PARAMETER_NAME)
                            .isEmpty();
    }

    /**
     * Construct Autofill Assistant UI controller.
     *
     * @param activity The CustomTabActivity of the controller associated with.
     */
    public AutofillAssistantUiController(CustomTabActivity activity) {
        // Set mUiDelegate before nativeInit, as it can be accessed through native methods from
        // nativeInit already.
        mUiDelegateHolder = new UiDelegateHolder(new AutofillAssistantUiDelegate(activity, this));
        chooseAccountAsync(activity.getInitialIntent().getExtras());

        Map<String, String> parameters = extractParameters(activity.getInitialIntent().getExtras());
        parameters.remove(PARAMETER_ENABLED);

        Tab activityTab = activity.getActivityTab();
        mWebContents = activityTab.getWebContents();
        mUiControllerAndroid =
                nativeInit(mWebContents, parameters.keySet().toArray(new String[parameters.size()]),
                        parameters.values().toArray(new String[parameters.size()]),
                        activity.getInitialIntent().getDataString());

        // Shut down Autofill Assistant when the tab is detached from the activity.
        activityTab.addObserver(new EmptyTabObserver() {
            @Override
            public void onActivityAttachmentChanged(Tab tab, boolean isAttached) {
                if (!isAttached) {
                    activityTab.removeObserver(this);
                    nativeDestroy(mUiControllerAndroid);
                }
            }
        });

        // Shut down Autofill Assistant when the selected tab (foreground tab) is changed.
        TabModel currentTabModel = activity.getTabModelSelector().getCurrentModel();
        currentTabModel.addObserver(new EmptyTabModelObserver() {
            @Override
            public void didSelectTab(Tab tab, @TabSelectionType int type, int lastId) {
                currentTabModel.removeObserver(this);

                // Assume newly selected tab is always different from the last one.
                nativeDestroy(mUiControllerAndroid);
                // TODO(crbug.com/806868): May start a new Autofill Assistant instance for the newly
                // selected Tab.
            }
        });
    }

    @Override
    public void onDismiss() {
        mUiDelegateHolder.performUiOperation(uiDelegate -> {
            uiDelegate.hide();
            mUiDelegateHolder.pauseUiOperations();

            // Show the UI back when unpaused.
            mUiDelegateHolder.performUiOperation(AutofillAssistantUiDelegate::show);

            // We show a snackbar with "undo" button for a few seconds, and shutdown only if it is
            // not cancelled.
            uiDelegate.showAutofillAssistantStoppedSnackbar(
                    new SnackbarManager.SnackbarController() {
                        @Override
                        public void onAction(Object actionData) {
                            // Shutdown was cancelled.
                            mUiDelegateHolder.unpauseUiOperations();
                        }

                        @Override
                        public void onDismissNoAction(Object actionData) {
                            nativeDestroy(mUiControllerAndroid);
                        }
                    });
        });
    }

    @Override
    public void onScriptSelected(String scriptPath) {
        nativeOnScriptSelected(mUiControllerAndroid, scriptPath);
    }

    @Override
    public void onAddressSelected(String guid) {
        nativeOnAddressSelected(mUiControllerAndroid, guid);
    }

    @Override
    public void onCardSelected(String guid) {
        nativeOnCardSelected(mUiControllerAndroid, guid);
    }

    /** Return the value if the given boolean parameter from the extras. */
    private static boolean getBooleanParameter(Bundle extras, String parameterName) {
        return extras.getBoolean(INTENT_EXTRA_PREFIX + parameterName, false);
    }

    /** Returns a map containing the extras starting with {@link #INTENT_EXTRA_PREFIX}. */
    private static Map<String, String> extractParameters(Bundle extras) {
        Map<String, String> result = new HashMap<>();
        for (String key : extras.keySet()) {
            if (key.startsWith(INTENT_EXTRA_PREFIX)) {
                result.put(key.substring(INTENT_EXTRA_PREFIX.length()), extras.get(key).toString());
            }
        }
        return result;
    }

    @Override
    public void onClickOverlay() {
        // TODO(crbug.com/806868): Notify native side.
    }

    @CalledByNative
    private void onShowStatusMessage(String message) {
        mUiDelegateHolder.performUiOperation(uiDelegate -> uiDelegate.showStatusMessage(message));
    }

    @CalledByNative
    private void onShowOverlay() {
        mUiDelegateHolder.performUiOperation(AutofillAssistantUiDelegate::showOverlay);
    }

    @CalledByNative
    private void onHideOverlay() {
        mUiDelegateHolder.performUiOperation(AutofillAssistantUiDelegate::hideOverlay);
    }

    @CalledByNative
    private void onShutdown() {
        nativeDestroy(mUiControllerAndroid);
    }

    @CalledByNative
    private void onUpdateScripts(
            String[] scriptNames, String[] scriptPaths, boolean[] scriptsHighlightFlags) {
        assert scriptNames.length == scriptPaths.length;
        assert scriptNames.length == scriptsHighlightFlags.length;

        ArrayList<AutofillAssistantUiDelegate.ScriptHandle> scriptHandles = new ArrayList<>();
        // Note that scriptNames, scriptPaths and scriptsHighlightFlags are one-on-one matched by
        // index.
        for (int i = 0; i < scriptNames.length; i++) {
            scriptHandles.add(new AutofillAssistantUiDelegate.ScriptHandle(
                    scriptNames[i], scriptPaths[i], scriptsHighlightFlags[i]));
        }

        mUiDelegateHolder.performUiOperation(uiDelegate -> uiDelegate.updateScripts(scriptHandles));
    }

    @CalledByNative
    private void onChooseAddress() {
        // TODO(crbug.com/806868): Remove this method once all scripts use payment request.
        mUiDelegateHolder.performUiOperation(uiDelegate
                -> uiDelegate.showProfiles(PersonalDataManager.getInstance().getProfilesToSuggest(
                        true /* includeNameInLabel */)));
    }

    @CalledByNative
    private void onChooseCard() {
        // TODO(crbug.com/806868): Remove this method once all scripts use payment request.
        mUiDelegateHolder.performUiOperation(uiDelegate
                -> uiDelegate.showCards(PersonalDataManager.getInstance().getCreditCardsToSuggest(
                        true /* includeServerCards */)));
    }

    @CalledByNative
    private void onRequestPaymentInformation(boolean requestShipping, boolean requestPayerName,
            boolean requestPayerPhone, boolean requestPayerEmail, int shippingType, String title) {
        PaymentOptions paymentOtions = new PaymentOptions();
        paymentOtions.requestShipping = requestShipping;
        paymentOtions.requestPayerName = requestPayerName;
        paymentOtions.requestPayerPhone = requestPayerPhone;
        paymentOtions.requestPayerEmail = requestPayerEmail;
        paymentOtions.shippingType = shippingType;
        mAutofillAssistantPaymentRequest =
                new AutofillAssistantPaymentRequest(mWebContents, paymentOtions, title);

        mUiDelegateHolder.performUiOperation(
                uiDelegate -> mAutofillAssistantPaymentRequest.show(selectedPaymentInformation -> {
                    nativeOnGetPaymentInformation(mUiControllerAndroid,
                            selectedPaymentInformation.succeed, selectedPaymentInformation.cardGuid,
                            selectedPaymentInformation.cardIssuerNetwork,
                            selectedPaymentInformation.addressGuid,
                            selectedPaymentInformation.payerName,
                            selectedPaymentInformation.payerPhone,
                            selectedPaymentInformation.payerEmail);
                    mAutofillAssistantPaymentRequest.close();
                    mAutofillAssistantPaymentRequest = null;
                }));
    }

    @CalledByNative
    private void onHideDetails() {
        mUiDelegateHolder.performUiOperation(AutofillAssistantUiDelegate::hideDetails);
    }

    @CalledByNative
    private void onShowDetails(String title, String url, String description, int year, int month,
            int day, int hour, int minute, int second) {
        Date date;
        if (year > 0 && month > 0 && day > 0 && hour >= 0 && minute >= 0 && second >= 0) {
            Calendar calendar = Calendar.getInstance();
            // Month in Java Date is 0-based, but the one we receive from the server is 1-based.
            calendar.set(year, month - 1, day, hour, minute, second);
            date = calendar.getTime();
        } else {
            date = null;
        }

        mUiDelegateHolder.performUiOperation(uiDelegate
                -> uiDelegate.showDetails(
                        new AutofillAssistantUiDelegate.Details(title, url, date, description)));
    }

    @CalledByNative
    private void onShowProgressBar(int progress, String message) {
        mUiDelegateHolder.performUiOperation(
                uiDelegate -> uiDelegate.showProgressBar(progress, message));
    }

    @CalledByNative
    private void onHideProgressBar() {
        mUiDelegateHolder.performUiOperation(AutofillAssistantUiDelegate::hideProgressBar);
    }

    /**
     * Class holder for the AutofillAssistantUiDelegate to make sure we don't make UI changes when
     * we are in a pause state (i.e. few seconds before stopping completely).
     */
    private static class UiDelegateHolder {
        private final AutofillAssistantUiDelegate mUiDelegate;

        private boolean mShouldQueueUiOperations = false;
        private final ArrayList<Consumer<AutofillAssistantUiDelegate>> mPendingUiOperations =
                new ArrayList<>();

        private UiDelegateHolder(AutofillAssistantUiDelegate uiDelegate) {
            mUiDelegate = uiDelegate;
        }

        /**
         * Pause all UI operations such that they can potentially be ran later using {@link
         * #unpauseUiOperations()}.
         */
        public void pauseUiOperations() {
            mShouldQueueUiOperations = true;
        }

        /**
         * Unpause and trigger all UI operations received by {@link #performUiOperation(Consumer)}
         * since the last {@link #pauseUiOperations()}.
         */
        public void unpauseUiOperations() {
            mShouldQueueUiOperations = false;
            for (int i = 0; i < mPendingUiOperations.size(); i++) {
                mPendingUiOperations.get(i).accept(mUiDelegate);
            }
            mPendingUiOperations.clear();
        }

        /**
         * Perform a UI operation:
         *  - directly if we are not in a pause state.
         *  - later if the shutdown is cancelled.
         *  - never if Autofill Assistant is shut down.
         */
        public void performUiOperation(Consumer<AutofillAssistantUiDelegate> operation) {
            if (mShouldQueueUiOperations) {
                mPendingUiOperations.add(operation);
                return;
            }

            operation.accept(mUiDelegate);
        }
    }

    @CalledByNative
    private void fetchAccessToken() {
        if (!mAccountInitialized) {
            // Still getting the account list. Fetch the token as soon as an account is available.
            mShouldFetchAccessToken = true;
            return;
        }
        if (mAccount == null) {
            nativeOnAccessToken(mUiControllerAndroid, true, "");
            return;
        }

        AccountManagerFacade.get().getAuthToken(
                mAccount, AUTH_TOKEN_TYPE, new AccountManagerFacade.GetAuthTokenCallback() {
                    @Override
                    public void tokenAvailable(String token) {
                        nativeOnAccessToken(mUiControllerAndroid, true, token);
                    }

                    @Override
                    public void tokenUnavailable(boolean isTransientError) {
                        if (!isTransientError) {
                            nativeOnAccessToken(mUiControllerAndroid, false, "");
                        }
                    }
                });
    }

    @CalledByNative
    private void invalidateAccessToken(String accessToken) {
        if (mAccount == null) {
            return;
        }

        AccountManagerFacade.get().invalidateAuthToken(accessToken);
    }

    /** Choose an account to authenticate as for making RPCs to the backend. */
    private void chooseAccountAsync(Bundle extras) {
        AccountManagerFacade.get().tryGetGoogleAccounts(accounts -> {
            if (accounts.length == 1) {
                // If there's only one account, there aren't any doubts.
                onAccountChosen(accounts[0]);
                return;
            }
            Account signedIn =
                    findAccountByName(accounts, nativeGetPrimaryAccountName(mUiControllerAndroid));
            if (signedIn != null) {
                // TODO(crbug.com/806868): Compare against account name from extras and complain if
                // they don't match.
                onAccountChosen(signedIn);
                return;
            }
            for (String extra : extras.keySet()) {
                if (extra.endsWith("ACCOUNT_NAME")) {
                    Account account = findAccountByName(accounts, extras.getString(extra));
                    if (account != null) {
                        onAccountChosen(account);
                        return;
                    }
                }
            }
            onAccountChosen(null);
        });
    }

    private void onAccountChosen(@Nullable Account account) {
        mAccount = account;
        mAccountInitialized = true;
        // TODO(crbug.com/806868): Consider providing a way of signing in this case, to enforce
        // that all calls are authenticated.

        if (mShouldFetchAccessToken) {
            mShouldFetchAccessToken = false;
            fetchAccessToken();
        }
    }

    private static Account findAccountByName(Account[] accounts, String name) {
        for (Account account : accounts) {
            if (account.name.equals(name)) {
                return account;
            }
        }
        return null;
    }

    // native methods.
    private native long nativeInit(WebContents webContents, String[] parameterNames,
            String[] parameterValues, String initialUrl);
    private native void nativeDestroy(long nativeUiControllerAndroid);
    private native void nativeOnScriptSelected(long nativeUiControllerAndroid, String scriptPath);
    private native void nativeOnAddressSelected(long nativeUiControllerAndroid, String guid);
    private native void nativeOnCardSelected(long nativeUiControllerAndroid, String guid);
    private native void nativeOnGetPaymentInformation(long nativeUiControllerAndroid,
            boolean succeed, @Nullable String cardGuid, @Nullable String cardIssuerNetwork,
            @Nullable String addressGuid, @Nullable String payerName, @Nullable String payerPhone,
            @Nullable String payerEmail);
    private native void nativeOnAccessToken(
            long nativeUiControllerAndroid, boolean success, String accessToken);
    private native String nativeGetPrimaryAccountName(long nativeUiControllerAndroid);
}
