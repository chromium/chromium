// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.app.Activity;
import android.content.Context;
import android.os.Bundle;
import android.os.SystemClock;
import android.support.annotation.IntDef;
import android.support.annotation.StringRes;
import android.support.v4.app.FragmentManager;
import android.support.v4.view.ViewCompat;
import android.support.v7.app.AlertDialog;
import android.text.method.LinkMovementMethod;
import android.util.AttributeSet;
import android.view.View;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.task.AsyncTask;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.consent_auditor.ConsentAuditorFeature;
import org.chromium.chrome.browser.externalauth.UserRecoverableErrorHandler;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.signin.AccountTrackerService.OnSystemAccountsSeededListener;
import org.chromium.chrome.browser.signin.ConfirmImportSyncDataDialog.ImportSyncType;
import org.chromium.components.signin.AccountIdProvider;
import org.chromium.components.signin.AccountManagerDelegateException;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerResult;
import org.chromium.components.signin.AccountsChangeObserver;
import org.chromium.components.signin.ChildAccountStatus;
import org.chromium.components.signin.GmsAvailabilityException;
import org.chromium.components.signin.GmsJustUpdatedException;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;
import org.chromium.ui.widget.ButtonCompat;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.TimeUnit;

/**
 * This view allows the user to select an account to log in to, add an account, cancel account
 * selection, etc. Users of this class should call {@link #init} after the view has been inflated.
 */
public class AccountSigninView extends FrameLayout {
    /**
     * Callbacks for various account selection events.
     */
    public interface Listener {
        /**
         * The user canceled account selection.
         */
        void onAccountSelectionCanceled();

        /**
         * The user wants to make a new account.
         */
        void onNewAccount();

        /**
         * The user completed the View and selected an account.
         * @param accountName The name of the account
         * @param isDefaultAccount Whether selected account is a default one (first of all accounts)
         * @param settingsClicked If true, user requested to see their sync settings, if false
         *                        they just clicked Done.
         */
        void onAccountSelected(
                String accountName, boolean isDefaultAccount, boolean settingsClicked);

        /**
         * Failed to set the forced account because it wasn't found.
         * @param forcedAccountName The name of the forced-sign-in account
         */
        void onFailedToSetForcedAccount(String forcedAccountName);
    }

    /**
     * Provides UI objects for new UI component creation.
     */
    public interface Delegate {
        /**
         * Provides an Activity for the View to check GMSCore version.
         */
        Activity getActivity();

        /**
         * Provides a FragmentManager for the View to create dialogs. This is done through a
         * different mechanism than getActivity().getSupportFragmentManager() as a potential fix to
         * https://crbug.com/646978 on the theory that getActivity() and getSupportFragmentManager()
         * return null at different times.
         */
        FragmentManager getSupportFragmentManager();
    }

    private static final String TAG = "AccountSigninView";

    private static final String SETTINGS_LINK_OPEN = "<LINK1>";
    private static final String SETTINGS_LINK_CLOSE = "</LINK1>";

    private static final String ARGUMENT_ACCESS_POINT = "AccountSigninView.AccessPoint";
    private static final String ARGUMENT_SIGNIN_FLOW_TYPE = "AccountSigninView.FlowType";
    private static final String ARGUMENT_ACCOUNT_NAME = "AccountSigninView.AccountName";
    private static final String ARGUMENT_IS_DEFAULT_ACCOUNT = "AccountSigninView.IsDefaultAccount";
    private static final String ARGUMENT_CHILD_ACCOUNT_STATUS =
            "AccountSigninView.ChildAccountStatus";
    private static final String ARGUMENT_UNDO_BEHAVIOR = "AccountSigninView.UndoBehavior";

    @IntDef({SigninFlowType.DEFAULT, SigninFlowType.CONFIRMATION_ONLY,
            SigninFlowType.ADD_NEW_ACCOUNT})
    @Retention(RetentionPolicy.SOURCE)
    public @interface SigninFlowType {
        int DEFAULT = 0;
        int CONFIRMATION_ONLY = 1;
        int ADD_NEW_ACCOUNT = 2;
    }

    /** Specifies different behaviors for "Undo" button on signin confirmation page. */
    @IntDef({UndoBehavior.INVISIBLE, UndoBehavior.BACK_TO_SELECTION, UndoBehavior.ABORT})
    @Retention(RetentionPolicy.SOURCE)
    public @interface UndoBehavior {
        /** "Undo" button is invisible. */
        int INVISIBLE = 0;
        /** "Undo" button opens account selection page. */
        int BACK_TO_SELECTION = 1;
        /** "Undo" button calls {@link Listener#onAccountSelectionCanceled()}. */
        int ABORT = 2;
    }

    private final AccountsChangeObserver mAccountsChangedObserver;
    private final ProfileDataCache.Observer mProfileDataCacheObserver;
    private final ProfileDataCache mProfileDataCache;
    private List<String> mAccountNames;
    private AccountSigninChooseView mSigninChooseView;
    private ButtonCompat mPositiveButton;
    private Button mNegativeButton;
    private Button mMoreButton;
    private Listener mListener;
    private Delegate mDelegate;
    private @SigninAccessPoint int mSigninAccessPoint;
    private @SigninFlowType int mSigninFlowType;
    private @UndoBehavior int mUndoBehavior;
    private String mSelectedAccountName;
    private boolean mIsDefaultAccountSelected;
    private @StringRes int mCancelButtonTextId = R.string.cancel;
    private @ChildAccountStatus.Status int mChildAccountStatus;
    private UserRecoverableErrorHandler.ModalDialog mGooglePlayServicesUpdateErrorHandler;
    private AlertDialog mGmsIsUpdatingDialog;
    private long mGmsIsUpdatingDialogShowTime;
    private boolean mShouldShowConfirmationPageWhenAttachedToWindow;

    private AccountSigninConfirmationView mSigninConfirmationView;
    private ImageView mSigninAccountImage;
    private TextView mSigninAccountName;
    private TextView mSigninAccountEmail;
    private TextView mSigninSyncTitle;
    private TextView mSigninSyncDescription;
    private TextView mSigninPersonalizeServiceTitle;
    private TextView mSigninPersonalizeServiceDescription;
    private TextView mSigninSettingsControl;
    private ConfirmSyncDataStateMachine mConfirmSyncDataStateMachine;
    private ConsentTextTracker mConsentTextTracker;

    public AccountSigninView(Context context, AttributeSet attrs) {
        super(context, attrs);
        mAccountsChangedObserver = this::triggerUpdateAccounts;
        mProfileDataCacheObserver = (String accountId) -> updateProfileData();
        mProfileDataCache = new ProfileDataCache(context,
                context.getResources().getDimensionPixelSize(R.dimen.signin_account_image_size));
        mConsentTextTracker = new ConsentTextTracker(context.getResources());
    }

    /**
     * Creates an argument bundle to start AccountSigninView from the account selection page.
     *
     * @param accessPoint The access point for starting signin flow.
     * @param childAccountStatus Whether this view is for a child account and of what type.
     */
    public static Bundle createArgumentsForDefaultFlow(
            @SigninAccessPoint int accessPoint, @ChildAccountStatus.Status int childAccountStatus) {
        Bundle result = new Bundle();
        result.putInt(ARGUMENT_SIGNIN_FLOW_TYPE, SigninFlowType.DEFAULT);
        result.putInt(ARGUMENT_ACCESS_POINT, accessPoint);
        result.putInt(ARGUMENT_CHILD_ACCOUNT_STATUS, childAccountStatus);
        result.putInt(ARGUMENT_UNDO_BEHAVIOR, UndoBehavior.BACK_TO_SELECTION);
        return result;
    }

    /**
     * Creates an argument bundle to start AccountSigninView from the new account creation screen.
     *
     * @param accessPoint The access point for starting signin flow.
     */
    public static Bundle createArgumentsForAddAccountFlow(@SigninAccessPoint int accessPoint) {
        Bundle result = new Bundle();
        result.putInt(ARGUMENT_SIGNIN_FLOW_TYPE, SigninFlowType.ADD_NEW_ACCOUNT);
        result.putInt(ARGUMENT_ACCESS_POINT, accessPoint);
        result.putInt(ARGUMENT_CHILD_ACCOUNT_STATUS,
                ChildAccountStatus.NOT_CHILD); // Children profiles can't add accounts
        result.putInt(ARGUMENT_UNDO_BEHAVIOR, UndoBehavior.ABORT);
        return result;
    }

    /**
     * Creates an argument bundle to start AccountSigninView from the signin confirmation page.
     *
     * @param accessPoint The access point for starting signin flow.
     * @param childAccountStatus Whether this view is for a child account and of what type.
     * @param accountName An account that should be used for confirmation page and signin.
     * @param isDefaultAccount Whether {@param accountName} is a default account, used for metrics.
     * @param undoBehavior "Undo" button behavior (see {@link UndoBehavior}).
     */
    public static Bundle createArgumentsForConfirmationFlow(@SigninAccessPoint int accessPoint,
            @ChildAccountStatus.Status int childAccountStatus, String accountName,
            boolean isDefaultAccount, @UndoBehavior int undoBehavior) {
        Bundle result = new Bundle();
        result.putInt(ARGUMENT_SIGNIN_FLOW_TYPE, SigninFlowType.CONFIRMATION_ONLY);
        result.putInt(ARGUMENT_ACCESS_POINT, accessPoint);
        result.putInt(ARGUMENT_CHILD_ACCOUNT_STATUS, childAccountStatus);
        result.putString(ARGUMENT_ACCOUNT_NAME, accountName);
        result.putBoolean(ARGUMENT_IS_DEFAULT_ACCOUNT, isDefaultAccount);
        result.putInt(ARGUMENT_UNDO_BEHAVIOR, undoBehavior);
        return result;
    }

    /**
     * Initializes the view.
     *
     * @param arguments The argument bundle created by {@link #createArgumentsForDefaultFlow},
     *         {@link #createArgumentsForAddAccountFlow} or
     *         {@link #createArgumentsForConfirmationFlow}.
     * @param delegate The UI object creation delegate.
     * @param listener The account selection event listener.
     */
    public void init(Bundle arguments, Delegate delegate, Listener listener) {
        @SigninAccessPoint int accessPoint = arguments.getInt(ARGUMENT_ACCESS_POINT, -1);
        assert accessPoint != -1;

        initAccessPoint(accessPoint);
        mChildAccountStatus =
                arguments.getInt(ARGUMENT_CHILD_ACCOUNT_STATUS, ChildAccountStatus.NOT_CHILD);
        mUndoBehavior = arguments.getInt(ARGUMENT_UNDO_BEHAVIOR, -1);
        mSigninFlowType = arguments.getInt(ARGUMENT_SIGNIN_FLOW_TYPE, -1);
        mDelegate = delegate;
        mListener = listener;

        updateConsentText();

        switch (mSigninFlowType) {
            case SigninFlowType.DEFAULT:
                showSigninPage();
                break;
            case SigninFlowType.CONFIRMATION_ONLY: {
                String accountName = arguments.getString(ARGUMENT_ACCOUNT_NAME);
                assert accountName != null;
                boolean isDefaultAccount = arguments.getBoolean(ARGUMENT_IS_DEFAULT_ACCOUNT, false);
                showConfirmationPageForAccount(accountName, isDefaultAccount);
                triggerUpdateAccounts();
                break;
            }
            case SigninFlowType.ADD_NEW_ACCOUNT:
                showSigninPage();
                RecordUserAction.record("Signin_AddAccountToDevice");
                mListener.onNewAccount();
                break;
            default:
                assert false : "Unknown or missing signin flow type: " + mSigninFlowType;
                return;
        }
    }

    public @SigninFlowType int getSigninFlowType() {
        return mSigninFlowType;
    }

    public @SigninAccessPoint int getSigninAccessPoint() {
        return mSigninAccessPoint;
    }

    private void initAccessPoint(@SigninAccessPoint int accessPoint) {
        mSigninAccessPoint = accessPoint;
        if (accessPoint == SigninAccessPoint.START_PAGE
                || accessPoint == SigninAccessPoint.SIGNIN_PROMO) {
            mCancelButtonTextId = R.string.no_thanks;
        }
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mSigninChooseView = findViewById(R.id.account_signin_choose_view);
        mSigninChooseView.setAddNewAccountObserver(() -> {
            mListener.onNewAccount();
            RecordUserAction.record("Signin_AddAccountToDevice");
        });

        mPositiveButton = findViewById(R.id.positive_button);
        mNegativeButton = findViewById(R.id.negative_button);
        mMoreButton = findViewById(R.id.more_button);
        mSigninConfirmationView = findViewById(R.id.signin_confirmation_view);
        mSigninAccountImage = findViewById(R.id.signin_account_image);
        mSigninAccountName = findViewById(R.id.signin_account_name);
        mSigninAccountEmail = findViewById(R.id.signin_account_email);
        mSigninSyncTitle = findViewById(R.id.signin_sync_title);
        mSigninSyncDescription = findViewById(R.id.signin_sync_description);
        mSigninPersonalizeServiceTitle = findViewById(R.id.signin_personalize_service_title);
        mSigninPersonalizeServiceDescription =
                findViewById(R.id.signin_personalize_service_description);
        mSigninSettingsControl = findViewById(R.id.signin_settings_control);
        // For the spans to be clickable.
        mSigninSettingsControl.setMovementMethod(LinkMovementMethod.getInstance());
    }

    private void updateConsentText() {
        // Static strings.
        mConsentTextTracker.setText(mSigninSyncTitle, R.string.sync_confirmation_chrome_sync_title);
        mConsentTextTracker.setText(
                mSigninSyncDescription, R.string.sync_confirmation_chrome_sync_message);
        mConsentTextTracker.setText(mSigninPersonalizeServiceTitle,
                R.string.sync_confirmation_personalize_services_title);
        mConsentTextTracker.setText(mSigninPersonalizeServiceDescription,
                mChildAccountStatus == ChildAccountStatus.REGULAR_CHILD
                        ? R.string.sync_confirmation_personalize_services_body_child_account
                        : R.string.sync_confirmation_personalize_services_body);
        mConsentTextTracker.setText(
                mSigninSettingsControl, R.string.signin_signed_in_settings_description);
        mConsentTextTracker.setText(mNegativeButton, mCancelButtonTextId);
        mConsentTextTracker.setText(mPositiveButton, R.string.choose_account_sign_in);
        mConsentTextTracker.setText(mMoreButton, R.string.more);

        // The clickable "Settings" link.
        NoUnderlineClickableSpan settingsSpan = new NoUnderlineClickableSpan((widget) -> {
            mListener.onAccountSelected(mSelectedAccountName, mIsDefaultAccountSelected, true);
            RecordUserAction.record("Signin_Signin_WithAdvancedSyncSettings");

            // Record the fact that the user consented to the consent text by clicking
            // on |mSigninSettingsControl|.
            recordConsent((TextView) widget);
        });
        mConsentTextTracker.setText(mSigninSettingsControl,
                getSettingsControlDescription(mChildAccountStatus), input -> {
                    return SpanApplier.applySpans(input.toString(),
                            new SpanInfo(SETTINGS_LINK_OPEN, SETTINGS_LINK_CLOSE, settingsSpan));
                });
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        triggerUpdateAccounts();
        AccountManagerFacade.get().addObserver(mAccountsChangedObserver);
        mProfileDataCache.addObserver(mProfileDataCacheObserver);
        if (mShouldShowConfirmationPageWhenAttachedToWindow) {
            // Can happen if init is invoked before attaching to window (https://crbug.com/800665).
            seedAccountsAndShowConfirmationPage();
        }
    }

    @Override
    protected void onDetachedFromWindow() {
        if (mConfirmSyncDataStateMachine != null) {
            mConfirmSyncDataStateMachine.cancel(/* isBeingDestroyed = */ true);
            mConfirmSyncDataStateMachine = null;
        }
        mProfileDataCache.removeObserver(mProfileDataCacheObserver);
        AccountManagerFacade.get().removeObserver(mAccountsChangedObserver);
        super.onDetachedFromWindow();
    }

    @Override
    public void onWindowVisibilityChanged(int visibility) {
        super.onWindowVisibilityChanged(visibility);
        if (visibility == View.VISIBLE) {
            triggerUpdateAccounts();
            return;
        }
        if (visibility == View.INVISIBLE && mGooglePlayServicesUpdateErrorHandler != null) {
            mGooglePlayServicesUpdateErrorHandler.cancelDialog();
            mGooglePlayServicesUpdateErrorHandler = null;
        }
    }

    /**
     * @return Whether the view is in signed in mode.
     */
    public boolean isInConfirmationScreen() {
        return mSelectedAccountName != null;
    }

    /**
     * Cancels signin confirmation and shows account selection page.
     */
    public void cancelConfirmationScreen() {
        assert isInConfirmationScreen();
        mUndoBehavior = UndoBehavior.BACK_TO_SELECTION;
        showSigninPage();
    }

    private void setButtonsEnabled(boolean enabled) {
        mPositiveButton.setEnabled(enabled);
        mNegativeButton.setEnabled(enabled);
    }

    /**
     * Refresh the list of available system accounts asynchronously.
     */
    private void triggerUpdateAccounts() {
        AccountManagerFacade.get().getGoogleAccountNames(this::updateAccounts);
    }

    private void updateAccounts(AccountManagerResult<List<String>> result) {
        if (!ViewCompat.isAttachedToWindow(AccountSigninView.this)) {
            // This callback is invoked after AccountSigninView is detached from window
            // (e.g., Chrome is minimized). Updating view now is redundant and dangerous
            // (getSupportFragmentManager() can return null, etc.). See https://crbug.com/733117.
            return;
        }

        final List<String> accountNames;
        try {
            accountNames = result.get();
        } catch (GmsAvailabilityException e) {
            dismissGmsUpdatingDialog();
            if (e.isUserResolvableError()) {
                showGmsErrorDialog(e.getGmsAvailabilityReturnCode());
            } else {
                Log.e(TAG, "Unresolvable GmsAvailabilityException.", e);
            }
            return;
        } catch (GmsJustUpdatedException e) {
            dismissGmsErrorDialog();
            showGmsUpdatingDialog();
            return;
        } catch (AccountManagerDelegateException e) {
            Log.e(TAG, "Unknown exception from AccountManagerFacade.", e);
            dismissGmsErrorDialog();
            dismissGmsUpdatingDialog();
            return;
        }
        dismissGmsErrorDialog();
        dismissGmsUpdatingDialog();

        if (mSelectedAccountName != null) {
            if (accountNames.contains(mSelectedAccountName)) return;

            if (mUndoBehavior == UndoBehavior.BACK_TO_SELECTION) {
                RecordUserAction.record("Signin_Undo_Signin");
                showSigninPage();
            } else {
                mListener.onFailedToSetForcedAccount(mSelectedAccountName);
            }
            return;
        }

        List<String> oldAccountNames = mAccountNames;
        mAccountNames = accountNames;

        int oldSelectedAccount = mSigninChooseView.getSelectedAccountPosition();
        AccountSelectionResult selection = selectAccountAfterAccountsUpdate(
                oldAccountNames, mAccountNames, oldSelectedAccount);
        int accountToSelect = selection.getSelectedAccountIndex();
        boolean shouldJumpToConfirmationScreen = selection.shouldJumpToConfirmationScreen();

        mSigninChooseView.updateAccounts(mAccountNames, accountToSelect, mProfileDataCache);
        setUpSigninButton(!mAccountNames.isEmpty());
        mProfileDataCache.update(mAccountNames);

        boolean selectedAccountChanged = oldAccountNames != null && !oldAccountNames.isEmpty()
                && (mAccountNames.isEmpty()
                           || !mAccountNames.get(accountToSelect)
                                       .equals(oldAccountNames.get(oldSelectedAccount)));
        if (selectedAccountChanged && mConfirmSyncDataStateMachine != null) {
            // Any dialogs that may have been showing are now invalid (they were created
            // for the previously selected account).
            mConfirmSyncDataStateMachine.cancel(/* isBeingDestroyed = */ false);
            mConfirmSyncDataStateMachine = null;
        }

        if (shouldJumpToConfirmationScreen) {
            showConfirmationPageForSelectedAccount();
        }
    }

    private boolean hasGmsError() {
        return mGooglePlayServicesUpdateErrorHandler != null || mGmsIsUpdatingDialog != null;
    }

    private void showGmsErrorDialog(int gmsErrorCode) {
        if (mGooglePlayServicesUpdateErrorHandler != null
                && mGooglePlayServicesUpdateErrorHandler.isShowing()) {
            return;
        }
        boolean cancelable = !SigninManager.get().isForceSigninEnabled();
        mGooglePlayServicesUpdateErrorHandler =
                new UserRecoverableErrorHandler.ModalDialog(mDelegate.getActivity(), cancelable);
        mGooglePlayServicesUpdateErrorHandler.handleError(getContext(), gmsErrorCode);
    }

    private void showGmsUpdatingDialog() {
        if (mGmsIsUpdatingDialog != null) {
            return;
        }
        mGmsIsUpdatingDialog = new AlertDialog.Builder(getContext())
                .setCancelable(false)
                .setView(R.layout.updating_gms_progress_view)
                .create();
        mGmsIsUpdatingDialog.show();
        mGmsIsUpdatingDialogShowTime = SystemClock.elapsedRealtime();
    }

    private void dismissGmsErrorDialog() {
        if (mGooglePlayServicesUpdateErrorHandler == null) {
            return;
        }
        mGooglePlayServicesUpdateErrorHandler.cancelDialog();
        mGooglePlayServicesUpdateErrorHandler = null;
    }

    private void dismissGmsUpdatingDialog() {
        if (mGmsIsUpdatingDialog == null) {
            return;
        }
        mGmsIsUpdatingDialog.dismiss();
        mGmsIsUpdatingDialog = null;
        RecordHistogram.recordTimesHistogram("Signin.AndroidGmsUpdatingDialogShownTime",
                SystemClock.elapsedRealtime() - mGmsIsUpdatingDialogShowTime,
                TimeUnit.MILLISECONDS);
    }

    private static class AccountSelectionResult {
        private final int mSelectedAccountIndex;
        private final boolean mShouldJumpToConfirmationScreen;

        AccountSelectionResult(int selectedAccountIndex, boolean shouldJumpToConfirmationScreen) {
            mSelectedAccountIndex = selectedAccountIndex;
            mShouldJumpToConfirmationScreen = shouldJumpToConfirmationScreen;
        }

        int getSelectedAccountIndex() {
            return mSelectedAccountIndex;
        }

        boolean shouldJumpToConfirmationScreen() {
            return mShouldJumpToConfirmationScreen;
        }
    }

    /**
     * Determine what account should be selected after account list update. This function also
     * decides whether AccountSigninView should jump to confirmation screen.
     *
     * @param oldList Old list of user accounts.
     * @param newList New list of user accounts.
     * @param oldIndex Index of the selected account in the old list.
     * @return {@link AccountSelectionResult} that encapsulates new index and jump/no jump flag.
     */
    private static AccountSelectionResult selectAccountAfterAccountsUpdate(
            List<String> oldList, List<String> newList, int oldIndex) {
        if (oldList == null || newList == null) return new AccountSelectionResult(0, false);
        // Return the old index if nothing changed
        if (oldList.size() == newList.size() && oldList.containsAll(newList)) {
            return new AccountSelectionResult(oldIndex, false);
        }
        if (newList.containsAll(oldList)) {
            // A new account(s) has been added and no accounts have been deleted. Select new account
            // and jump to the confirmation screen if only one account was added.
            boolean shouldJumpToConfirmationScreen = newList.size() == oldList.size() + 1;
            for (int i = 0; i < newList.size(); i++) {
                if (!oldList.contains(newList.get(i))) {
                    return new AccountSelectionResult(i, shouldJumpToConfirmationScreen);
                }
            }
        }
        return new AccountSelectionResult(0, false);
    }

    private void updateProfileData() {
        mSigninChooseView.updateAccountProfileImages(mProfileDataCache);

        if (mSelectedAccountName != null) updateSignedInAccountInfo();
    }

    private void updateSignedInAccountInfo() {
        DisplayableProfileData profileData =
                mProfileDataCache.getProfileDataOrDefault(mSelectedAccountName);
        mSigninAccountImage.setImageDrawable(profileData.getImage());
        String name = null;
        if (mChildAccountStatus == ChildAccountStatus.REGULAR_CHILD) {
            name = profileData.getGivenName();
        }
        if (name == null) name = profileData.getFullNameOrEmail();
        mConsentTextTracker.setTextNonRecordable(
                mSigninAccountName, getResources().getString(R.string.signin_hi_name, name));
        mConsentTextTracker.setTextNonRecordable(mSigninAccountEmail, mSelectedAccountName);
    }

    private void showSigninPage() {
        mSelectedAccountName = null;

        mSigninConfirmationView.setVisibility(View.GONE);
        mSigninChooseView.setVisibility(View.VISIBLE);

        setUpCancelButton();
        triggerUpdateAccounts();
    }

    private void showConfirmationPage() {
        updateSignedInAccountInfo();
        mProfileDataCache.update(Collections.singletonList(mSelectedAccountName));

        mSigninChooseView.setVisibility(View.GONE);
        mSigninConfirmationView.setVisibility(View.VISIBLE);

        setButtonsEnabled(true);
        setUpConfirmButton();
        setUpUndoButton();
    }

    private void showConfirmationPageForSelectedAccount() {
        int index = mSigninChooseView.getSelectedAccountPosition();
        showConfirmationPageForAccount(mAccountNames.get(index), index == 0);
    }

    private void showConfirmationPageForAccount(String accountName, boolean isDefaultAccount) {
        assert accountName != null;

        // Disable the buttons to prevent them being clicked again while waiting for the callbacks.
        setButtonsEnabled(false);

        mSelectedAccountName = accountName;
        mIsDefaultAccountSelected = isDefaultAccount;
        seedAccountsAndShowConfirmationPage();
    }

    private void seedAccountsAndShowConfirmationPage() {
        // Ensure that the AccountTrackerService has a fully up to date GAIA id <-> email mapping,
        // as this is needed for the previous account check.
        final long seedingStartTime = SystemClock.elapsedRealtime();
        if (AccountTrackerService.get().checkAndSeedSystemAccounts()) {
            recordAccountTrackerServiceSeedingTime(seedingStartTime);
            runStateMachineAndShowConfirmationPage();
        } else {
            AccountTrackerService.get().addSystemAccountsSeededListener(
                    new OnSystemAccountsSeededListener() {
                        @Override
                        public void onSystemAccountsSeedingComplete() {
                            AccountTrackerService.get().removeSystemAccountsSeededListener(this);
                            recordAccountTrackerServiceSeedingTime(seedingStartTime);
                            // Don't show dialogs and confirmation page if activity was destroyed.
                            if (ViewCompat.isAttachedToWindow(AccountSigninView.this)) {
                                runStateMachineAndShowConfirmationPage();
                            } else {
                                mShouldShowConfirmationPageWhenAttachedToWindow = true;
                            }
                        }

                        @Override
                        public void onSystemAccountsChanged() {}
                    });
        }
    }

    private void runStateMachineAndShowConfirmationPage() {
        mConfirmSyncDataStateMachine = new ConfirmSyncDataStateMachine(getContext(),
                mDelegate.getSupportFragmentManager(), ImportSyncType.PREVIOUS_DATA_FOUND,
                PrefServiceBridge.getInstance().getSyncLastAccountName(), mSelectedAccountName,
                new ConfirmImportSyncDataDialog.Listener() {
                    @Override
                    public void onConfirm(boolean wipeData) {
                        mConfirmSyncDataStateMachine = null;
                        SigninManager.wipeSyncUserDataIfRequired(wipeData).then(
                                (Void v) -> showConfirmationPage());
                    }

                    @Override
                    public void onCancel() {
                        mConfirmSyncDataStateMachine = null;
                        setButtonsEnabled(true);
                        onSigninConfirmationCancel();
                    }
                });
    }

    private static void recordAccountTrackerServiceSeedingTime(long seedingStartTime) {
        RecordHistogram.recordTimesHistogram("Signin.AndroidAccountSigninViewSeedingTime",
                SystemClock.elapsedRealtime() - seedingStartTime, TimeUnit.MILLISECONDS);
    }

    private void setUpCancelButton() {
        setNegativeButtonVisible(true);

        mConsentTextTracker.setText(mNegativeButton, mCancelButtonTextId);
        mNegativeButton.setOnClickListener(view -> {
            setButtonsEnabled(false);
            mListener.onAccountSelectionCanceled();
        });
    }

    private void setUpSigninButton(boolean hasAccounts) {
        if (hasAccounts) {
            mConsentTextTracker.setText(mPositiveButton, R.string.continue_sign_in);
            mPositiveButton.setOnClickListener(view -> showConfirmationPageForSelectedAccount());
        } else {
            mConsentTextTracker.setText(mPositiveButton, R.string.choose_account_sign_in);
            mPositiveButton.setOnClickListener(view -> {
                if (hasGmsError()) return;

                RecordUserAction.record("Signin_AddAccountToDevice");
                mListener.onNewAccount();
            });
        }
        setUpMoreButtonVisible(false);
    }

    private void setUpUndoButton() {
        if (mUndoBehavior == UndoBehavior.INVISIBLE) {
            setNegativeButtonVisible(false);
            return;
        }
        setNegativeButtonVisible(true);
        mConsentTextTracker.setText(mNegativeButton, R.string.undo);
        mNegativeButton.setOnClickListener(view -> {
            RecordUserAction.record("Signin_Undo_Signin");
            onSigninConfirmationCancel();
        });
    }

    private void onSigninConfirmationCancel() {
        if (mUndoBehavior == UndoBehavior.BACK_TO_SELECTION) {
            showSigninPage();
        } else {
            assert mUndoBehavior == UndoBehavior.ABORT;
            mListener.onAccountSelectionCanceled();
        }
    }

    private void setUpConfirmButton() {
        mConsentTextTracker.setText(mPositiveButton, R.string.signin_accept);
        mPositiveButton.setOnClickListener(view -> {
            mListener.onAccountSelected(mSelectedAccountName, mIsDefaultAccountSelected, false);
            RecordUserAction.record("Signin_Signin_WithDefaultSyncSettings");

            // Record the fact that the user consented to the consent text by clicking
            // on |mPositiveButton|.
            recordConsent((TextView) view);
        });
        setUpMoreButtonVisible(true);
    }

    /*
    * mMoreButton is used to scroll mSigninConfirmationView down. It displays at the same position
    * as mPositiveButton.
    */
    private void setUpMoreButtonVisible(boolean enabled) {
        if (enabled) {
            mPositiveButton.setVisibility(View.GONE);
            mMoreButton.setVisibility(View.VISIBLE);
            mMoreButton.setOnClickListener(view -> {
                mSigninConfirmationView.smoothScrollBy(0, mSigninConfirmationView.getHeight());
                RecordUserAction.record("Signin_MoreButton_Shown");
            });
            mSigninConfirmationView.setObserver(() -> setUpMoreButtonVisible(false));
        } else {
            mPositiveButton.setVisibility(View.VISIBLE);
            mMoreButton.setVisibility(View.GONE);
            mSigninConfirmationView.setObserver(null);
        }
    }

    private void setNegativeButtonVisible(boolean enabled) {
        if (enabled) {
            mNegativeButton.setVisibility(View.VISIBLE);
            findViewById(R.id.positive_button_end_padding).setVisibility(View.GONE);
        } else {
            mNegativeButton.setVisibility(View.GONE);
            findViewById(R.id.positive_button_end_padding).setVisibility(View.INVISIBLE);
        }
    }

    private @StringRes int getSettingsControlDescription(
            @ChildAccountStatus.Status int childAccountStatus) {
        if (childAccountStatus == ChildAccountStatus.REGULAR_CHILD) {
            return R.string.signin_signed_in_settings_description_child_account;
        } else {
            return R.string.signin_signed_in_settings_description;
        }
    }

    /**
     * Records the Sync consent.
     * @param confirmationView The view that the user clicked when consenting.
     */
    private void recordConsent(TextView confirmationView) {
        // TODO(crbug.com/831257): Provide the account id synchronously from AccountManagerFacade.
        final AccountIdProvider accountIdProvider = AccountIdProvider.getInstance();
        new AsyncTask<String>() {
            @Override
            public String doInBackground() {
                return accountIdProvider.getAccountId(mSelectedAccountName);
            }

            @Override
            public void onPostExecute(String accountId) {
                mConsentTextTracker.recordConsent(accountId, ConsentAuditorFeature.CHROME_SYNC,
                        confirmationView, findViewById(R.id.signin_confirmation_view),
                        findViewById(R.id.button_bar));
            }
        }
                .executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }
}
