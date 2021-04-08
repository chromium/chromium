// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.accounts.Account;
import android.accounts.AccountManager;
import android.app.Activity;
import android.content.Intent;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.os.SystemClock;
import android.text.TextUtils;
import android.text.method.LinkMovementMethod;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.appcompat.app.AlertDialog;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentTransaction;

import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.consent_auditor.ConsentAuditorFeature;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.DisplayableProfileData;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.ProfileDataCache;
import org.chromium.chrome.browser.signin.services.SigninMetricsUtils;
import org.chromium.chrome.browser.signin.ui.ConfirmSyncDataStateMachine;
import org.chromium.chrome.browser.signin.ui.ConfirmSyncDataStateMachineDelegate;
import org.chromium.chrome.browser.signin.ui.ConsentTextTracker;
import org.chromium.chrome.browser.signin.ui.SigninUtils;
import org.chromium.chrome.browser.signin.ui.SigninView;
import org.chromium.chrome.browser.signin.ui.account_picker.AccountPickerCoordinator;
import org.chromium.chrome.browser.signin.ui.account_picker.AccountPickerDialogFragment;
import org.chromium.chrome.browser.sync.SyncUserDataWiper;
import org.chromium.components.externalauth.UserRecoverableErrorHandler;
import org.chromium.components.signin.AccountManagerDelegateException;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountManagerResult;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.AccountsChangeObserver;
import org.chromium.components.signin.ChildAccountStatus;
import org.chromium.components.signin.GmsAvailabilityException;
import org.chromium.components.signin.GmsJustUpdatedException;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.AccountInfoService;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;

/**
 * Contains the common logic of fragments used to sign in and enable sync.
 * The account to sync is selected among the list of Google Accounts present on
 * the device. A new account can also be added to the list, e.g. if there was
 * none at first.
 * Derived classes must implement {@link #onSigninAccepted}/{@link #onSigninRefused} to define
 * what happens after the signin flow.
 */
public abstract class SyncConsentFragmentBase
        extends Fragment implements AccountPickerCoordinator.Listener {
    private static final String TAG = "SigninFragmentBase";

    private static final String ARGUMENT_ACCESS_POINT = "SigninFragmentBase.AccessPoint";

    private static final String SETTINGS_LINK_OPEN = "<LINK1>";
    private static final String SETTINGS_LINK_CLOSE = "</LINK1>";

    private static final String ARGUMENT_ACCOUNT_NAME = "SigninFragmentBase.AccountName";
    private static final String ARGUMENT_CHILD_ACCOUNT_STATUS =
            "SigninFragmentBase.ChildAccountStatus";
    private static final String ARGUMENT_SIGNIN_FLOW_TYPE = "SigninFragmentBase.SigninFlowType";

    private static final String ACCOUNT_PICKER_DIALOG_TAG =
            "SigninFragmentBase.AccountPickerDialogFragment";

    private static final int ADD_ACCOUNT_REQUEST_CODE = 1;
    private static final int ACCOUNT_PICKER_DIALOG_REQUEST_CODE = 2;

    @IntDef({SigninFlowType.DEFAULT, SigninFlowType.CHOOSE_ACCOUNT, SigninFlowType.ADD_ACCOUNT})
    @Retention(RetentionPolicy.SOURCE)
    @interface SigninFlowType {
        int DEFAULT = 0;
        int CHOOSE_ACCOUNT = 1;
        int ADD_ACCOUNT = 2;
    }

    private final AccountManagerFacade mAccountManagerFacade;
    protected @ChildAccountStatus.Status int mChildAccountStatus;

    private SigninView mView;
    private ConsentTextTracker mConsentTextTracker;

    private boolean mAccountSelectionPending;
    private @Nullable String mRequestedAccountName;

    private String mSelectedAccountName;
    private boolean mIsDefaultAccountSelected;
    private final AccountsChangeObserver mAccountsChangedObserver;
    private final ProfileDataCache.Observer mProfileDataCacheObserver;
    private ProfileDataCache mProfileDataCache;
    private List<String> mAccountNames;
    private boolean mDestroyed;
    private boolean mIsSigninInProgress;
    private boolean mHasGmsError;
    private boolean mRecordUndoSignin;
    protected @SigninAccessPoint int mSigninAccessPoint;

    private UserRecoverableErrorHandler.ModalDialog mGooglePlayServicesUpdateErrorHandler;
    private AlertDialog mGmsIsUpdatingDialog;
    private long mGmsIsUpdatingDialogShowTime;
    private ConfirmSyncDataStateMachine mConfirmSyncDataStateMachine;

    /**
     * Creates an argument bundle for the default {@link SyncConsentFragment} flow.
     * (account selection is enabled, etc.).
     * @param accessPoint The access point for starting sign-in flow.
     * @param accountName The account to preselect or null to preselect the default account.
     */
    protected static Bundle createArguments(
            @SigninAccessPoint int accessPoint, @Nullable String accountName) {
        Bundle result = new Bundle();
        result.putInt(ARGUMENT_SIGNIN_FLOW_TYPE, SigninFlowType.DEFAULT);
        result.putInt(ARGUMENT_ACCESS_POINT, accessPoint);
        result.putString(ARGUMENT_ACCOUNT_NAME, accountName);
        return result;
    }

    /**
     * Creates an argument bundle for the default {@link SyncConsentFragment} flow with
     * {@link ChildAccountStatus}.
     * @param accessPoint The access point for starting sign-in flow.
     * @param accountName The account to preselect.
     * @param childAccountStatus Whether the selected account is a child one.
     */
    protected static Bundle createArguments(@SigninAccessPoint int accessPoint, String accountName,
            @ChildAccountStatus.Status int childAccountStatus) {
        Bundle result = createArguments(accessPoint, accountName);
        result.putInt(ARGUMENT_CHILD_ACCOUNT_STATUS, childAccountStatus);
        return result;
    }

    /**
     * Creates an argument bundle for "Choose account" sign-in flow. Account selection dialog will
     * be shown at the start of the sign-in process.
     * @param accountName The account to preselect or null to preselect the default account.
     */
    protected static Bundle createArgumentsForChooseAccountFlow(
            @SigninAccessPoint int accessPoint, @Nullable String accountName) {
        Bundle result = new Bundle();
        result.putInt(ARGUMENT_SIGNIN_FLOW_TYPE, SigninFlowType.CHOOSE_ACCOUNT);
        result.putInt(ARGUMENT_ACCESS_POINT, accessPoint);
        result.putString(ARGUMENT_ACCOUNT_NAME, accountName);
        return result;
    }

    /**
     * Creates an argument bundle for "Add account" sign-in flow. Activity to add an account will be
     * shown at the start of the sign-in process.
     */
    protected static Bundle createArgumentsForAddAccountFlow(@SigninAccessPoint int accessPoint) {
        Bundle result = new Bundle();
        result.putInt(ARGUMENT_SIGNIN_FLOW_TYPE, SigninFlowType.ADD_ACCOUNT);
        result.putInt(ARGUMENT_ACCESS_POINT, accessPoint);
        return result;
    }

    protected SyncConsentFragmentBase() {
        mAccountManagerFacade = AccountManagerFacadeProvider.getInstance();
        mAccountsChangedObserver = this::triggerUpdateAccounts;
        mProfileDataCacheObserver = this::updateProfileData;
    }

    /** The sign-in was refused. */
    protected abstract void onSigninRefused();

    /**
     * The sign-in was accepted.
     * @param accountName The name of the account
     * @param isDefaultAccount Whether selected account is a default one (first of all accounts)
     * @param settingsClicked Whether the user requested to see their sync settings
     * @param callback The callback invoke when sign-in process is finished or aborted
     */
    protected abstract void onSigninAccepted(String accountName, boolean isDefaultAccount,
            boolean settingsClicked, Runnable callback);

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        Bundle arguments = getArguments();
        mSigninAccessPoint = arguments.getInt(ARGUMENT_ACCESS_POINT, SigninAccessPoint.MAX);
        assert mSigninAccessPoint != SigninAccessPoint.MAX : "Cannot find SigninAccessPoint!";
        mRequestedAccountName = arguments.getString(ARGUMENT_ACCOUNT_NAME, null);
        mChildAccountStatus =
                arguments.getInt(ARGUMENT_CHILD_ACCOUNT_STATUS, ChildAccountStatus.NOT_CHILD);
        @SigninFlowType
        int signinFlowType = arguments.getInt(ARGUMENT_SIGNIN_FLOW_TYPE, SigninFlowType.DEFAULT);

        // Don't have a selected account now, onResume will trigger the selection.
        mAccountSelectionPending = true;

        if (savedInstanceState == null) {
            // If this fragment is being recreated from a saved state there's no need to show
            // account picked or starting AddAccount flow.
            if (signinFlowType == SigninFlowType.CHOOSE_ACCOUNT) {
                showAccountPicker();
            } else if (signinFlowType == SigninFlowType.ADD_ACCOUNT) {
                addAccount();
            }
        }

        mConsentTextTracker = new ConsentTextTracker(getResources());

        mProfileDataCache = ChildAccountStatus.isChild(mChildAccountStatus)
                ? ProfileDataCache.createWithDefaultImageSize(
                        requireContext(), R.drawable.ic_account_child_20dp)
                : ProfileDataCache.createWithDefaultImageSizeAndNoBadge(requireContext());

        // By default this is set to true so that when system back button is pressed user action
        // is recorded in onDestroy().
        mRecordUndoSignin = true;
        SigninMetricsUtils.logSigninStartAccessPoint(mSigninAccessPoint);
        SigninMetricsUtils.logSigninUserActionForAccessPoint(mSigninAccessPoint);
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        dismissGmsErrorDialog();
        dismissGmsUpdatingDialog();
        if (mConfirmSyncDataStateMachine != null) {
            mConfirmSyncDataStateMachine.cancel(/* isBeingDestroyed = */ true);
            mConfirmSyncDataStateMachine = null;
        }
        if (mRecordUndoSignin) RecordUserAction.record("Signin_Undo_Signin");
        mDestroyed = true;
    }

    @Override
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        mView = (SigninView) inflater.inflate(R.layout.signin_view, container, false);

        mView.getAccountPickerView().setOnClickListener(view -> onAccountPickerClicked());

        mView.getRefuseButton().setOnClickListener(this::onRefuseButtonClicked);
        mView.getAcceptButton().setVisibility(View.GONE);
        mView.getMoreButton().setVisibility(View.VISIBLE);
        mView.getMoreButton().setOnClickListener(view -> {
            mView.getScrollView().smoothScrollBy(0, mView.getScrollView().getHeight());
            // TODO(https://crbug.com/821127): Revise this user action.
            RecordUserAction.record("Signin_MoreButton_Shown");
        });
        mView.getScrollView().setScrolledToBottomObserver(this::showAcceptButton);
        mView.getDetailsDescriptionView().setMovementMethod(LinkMovementMethod.getInstance());

        final Drawable endImageViewDrawable;
        if (ChildAccountStatus.isChild(mChildAccountStatus)) {
            endImageViewDrawable = SigninView.getCheckmarkDrawable(getContext());
            mView.getRefuseButton().setVisibility(View.GONE);
            mView.getAcceptButtonEndPadding().setVisibility(View.INVISIBLE);
        } else {
            endImageViewDrawable = SigninView.getExpandArrowDrawable(getContext());
        }
        mView.getAccountPickerEndImageView().setImageDrawable(endImageViewDrawable);

        updateConsentText();
        setHasAccounts(true); // Assume there are accounts, updateAccounts will set the real value.

        // When a fragment that was in the FragmentManager backstack becomes visible again, the view
        // will be recreated by onCreateView. Update the state of this recreated UI.
        if (mSelectedAccountName != null) {
            updateProfileData(mSelectedAccountName);
        }
        return mView;
    }

    /**
     * Account picker is hidden if there are no accounts on the device. Also, accept button
     * becomes "Add account" button in this case.
     */
    private void setHasAccounts(boolean hasAccounts) {
        if (hasAccounts) {
            mView.getAccountPickerView().setVisibility(View.VISIBLE);
            mConsentTextTracker.setText(mView.getAcceptButton(), R.string.signin_accept_button);
            mView.getAcceptButton().setOnClickListener(this::onAcceptButtonClicked);
        } else {
            mView.getAccountPickerView().setVisibility(View.GONE);
            mConsentTextTracker.setText(mView.getAcceptButton(), R.string.signin_add_account);
            mView.getAcceptButton().setOnClickListener(this::onAddAccountButtonClicked);
        }

        // Show "Settings" link in description only if there are accounts on the device.
        updateSigninDetailsDescription(hasAccounts);
    }

    private void updateSigninDetailsDescription(boolean addSettingsLink) {
        final @Nullable Object settingsLinkSpan = addSettingsLink
                ? new NoUnderlineClickableSpan(getResources(), this::onSettingsLinkClicked)
                : null;
        final SpanApplier.SpanInfo spanInfo =
                new SpanApplier.SpanInfo(SETTINGS_LINK_OPEN, SETTINGS_LINK_CLOSE, settingsLinkSpan);
        mConsentTextTracker.setText(mView.getDetailsDescriptionView(),
                R.string.signin_details_description,
                input -> SpanApplier.applySpans(input.toString(), spanInfo));
    }

    /** Sets texts for immutable elements. Accept button text is set by {@link #setHasAccounts}. */
    private void updateConsentText() {
        mConsentTextTracker.setText(mView.getTitleView(), R.string.signin_title);

        mConsentTextTracker.setText(mView.getSyncTitleView(), R.string.signin_sync_title);
        final @StringRes int syncDescription =
                mChildAccountStatus == ChildAccountStatus.REGULAR_CHILD
                ? R.string.signin_sync_description_child_account
                : R.string.signin_sync_description;
        mConsentTextTracker.setText(mView.getSyncDescriptionView(), syncDescription);

        final @StringRes int refuseButtonTextId =
                mSigninAccessPoint == SigninAccessPoint.SIGNIN_PROMO
                        || mSigninAccessPoint == SigninAccessPoint.START_PAGE
                ? R.string.no_thanks
                : R.string.cancel;
        mConsentTextTracker.setText(mView.getRefuseButton(), refuseButtonTextId);
        mConsentTextTracker.setText(mView.getMoreButton(), R.string.more);
    }

    private void updateProfileData(String accountEmail) {
        if (!TextUtils.equals(accountEmail, mSelectedAccountName)) {
            return;
        }
        DisplayableProfileData profileData =
                mProfileDataCache.getProfileDataOrDefault(mSelectedAccountName);
        mView.getAccountImageView().setImageDrawable(profileData.getImage());

        final String fullName = profileData.getFullName();
        if (!TextUtils.isEmpty(fullName)) {
            mConsentTextTracker.setTextNonRecordable(mView.getAccountTextPrimary(), fullName);
            mConsentTextTracker.setTextNonRecordable(
                    mView.getAccountTextSecondary(), profileData.getAccountEmail());
            mView.getAccountTextSecondary().setVisibility(View.VISIBLE);
        } else {
            // Full name is not available, show the email in the primary TextView.
            mConsentTextTracker.setTextNonRecordable(
                    mView.getAccountTextPrimary(), profileData.getAccountEmail());
            mView.getAccountTextSecondary().setVisibility(View.GONE);
        }
    }

    private void showAcceptButton() {
        mView.getAcceptButton().setVisibility(View.VISIBLE);
        mView.getMoreButton().setVisibility(View.GONE);
        mView.getScrollView().setScrolledToBottomObserver(null);
    }

    private void onAccountPickerClicked() {
        if (ChildAccountStatus.isChild(mChildAccountStatus) || !areControlsEnabled()) return;
        showAccountPicker();
    }

    private void onRefuseButtonClicked(View button) {
        RecordUserAction.record("Signin_Undo_Signin");
        mRecordUndoSignin = false;
        onSigninRefused();
    }

    private void onAcceptButtonClicked(View button) {
        if (!areControlsEnabled()) return;
        mIsSigninInProgress = true;
        mRecordUndoSignin = false;
        RecordUserAction.record("Signin_Signin_WithDefaultSyncSettings");
        seedAccountsAndSignin(false, button);
    }

    private void onAddAccountButtonClicked(View button) {
        if (!areControlsEnabled()) return;
        addAccount();
    }

    private void onSettingsLinkClicked(View view) {
        if (!areControlsEnabled()) return;
        mIsSigninInProgress = true;
        RecordUserAction.record("Signin_Signin_WithAdvancedSyncSettings");
        seedAccountsAndSignin(true, view);
    }

    /**
     * Whether account picker and accept button should react to clicks. This doesn't change the
     * visual appearance of these controls. Refuse button is always enabled.
     */
    private boolean areControlsEnabled() {
        // Ignore clicks if the fragment is being removed or the app is being backgrounded.
        if (!isResumed() || isStateSaved()) return false;
        return !mAccountSelectionPending && !mIsSigninInProgress && !mHasGmsError;
    }

    private void seedAccountsAndSignin(boolean settingsClicked, View confirmationView) {
        // Ensure that the AccountTrackerService has a fully up to date GAIA id <-> email mapping,
        // as this is needed for the previous account check.
        final long seedingStartTime = SystemClock.elapsedRealtime();
        IdentityServicesProvider.get()
                .getAccountTrackerService(Profile.getLastUsedRegularProfile())
                .seedAccountsIfNeeded(() -> {
                    final CoreAccountInfo accountInfo =
                            AccountInfoService.get().getAccountInfoByEmail(mSelectedAccountName);
                    assert accountInfo != null : "The seeded CoreAccountInfo shouldn't be null";
                    mConsentTextTracker.recordConsent(accountInfo.getId(),
                            ConsentAuditorFeature.CHROME_SYNC, (TextView) confirmationView, mView);
                    RecordHistogram.recordTimesHistogram(
                            "Signin.AndroidAccountSigninViewSeedingTime",
                            SystemClock.elapsedRealtime() - seedingStartTime);
                    if (isResumed()) {
                        runStateMachineAndSignin(settingsClicked);
                    }
                });
    }

    private void runStateMachineAndSignin(boolean settingsClicked) {
        mConfirmSyncDataStateMachine = new ConfirmSyncDataStateMachine(
                new ConfirmSyncDataStateMachineDelegate(getChildFragmentManager()),
                UserPrefs.get(Profile.getLastUsedRegularProfile())
                        .getString(Pref.GOOGLE_SERVICES_LAST_USERNAME),
                mSelectedAccountName, new ConfirmSyncDataStateMachine.Listener() {
                    @Override
                    public void onConfirm(boolean wipeData) {
                        mConfirmSyncDataStateMachine = null;

                        // Don't start sign-in if this fragment has been destroyed.
                        if (mDestroyed) return;
                        SyncUserDataWiper.wipeSyncUserDataIfRequired(wipeData).then((Void v) -> {
                            onSigninAccepted(mSelectedAccountName, mIsDefaultAccountSelected,
                                    settingsClicked, () -> mIsSigninInProgress = false);
                        });
                    }

                    @Override
                    public void onCancel() {
                        mConfirmSyncDataStateMachine = null;
                        mIsSigninInProgress = false;
                    }
                });
    }

    private void showAccountPicker() {
        // Account picker is already shown
        if (getAccountPickerDialogFragment() != null) return;

        AccountPickerDialogFragment dialog =
                AccountPickerDialogFragment.create(mSelectedAccountName);
        dialog.setTargetFragment(this, ACCOUNT_PICKER_DIALOG_REQUEST_CODE);
        FragmentTransaction transaction = getFragmentManager().beginTransaction();
        transaction.add(dialog, ACCOUNT_PICKER_DIALOG_TAG);
        transaction.commitAllowingStateLoss();
    }

    private AccountPickerDialogFragment getAccountPickerDialogFragment() {
        return (AccountPickerDialogFragment) getFragmentManager().findFragmentByTag(
                ACCOUNT_PICKER_DIALOG_TAG);
    }

    @Override
    public void onAccountSelected(String accountName, boolean isDefaultAccount) {
        selectAccount(accountName, isDefaultAccount);
        getAccountPickerDialogFragment().dismissAllowingStateLoss();
    }

    @Override
    public void addAccount() {
        RecordUserAction.record("Signin_AddAccountToDevice");
        // TODO(https://crbug.com/842860): Revise createAddAccountIntent and AccountAdder.
        mAccountManagerFacade.createAddAccountIntent((@Nullable Intent intent) -> {
            if (intent != null) {
                startActivityForResult(intent, ADD_ACCOUNT_REQUEST_CODE);
                return;
            }

            // AccountManagerFacade couldn't create intent, use SigninUtils to open settings
            // instead.
            SigninUtils.openSettingsForAllAccounts(getActivity());
        });
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        if (requestCode == ADD_ACCOUNT_REQUEST_CODE && resultCode == Activity.RESULT_OK) {
            if (data == null) return;
            String addedAccountName = data.getStringExtra(AccountManager.KEY_ACCOUNT_NAME);
            if (addedAccountName == null) return;

            // Found the account name, dismiss the account picker dialog if it is shown.
            AccountPickerDialogFragment accountPickerFragment = getAccountPickerDialogFragment();
            if (accountPickerFragment != null) {
                accountPickerFragment.dismissAllowingStateLoss();
            }

            // Wait for the account cache to be updated and select newly-added account.
            mAccountManagerFacade.waitForPendingUpdates(() -> {
                mAccountSelectionPending = true;
                mRequestedAccountName = addedAccountName;
                triggerUpdateAccounts();
            });
        }
    }

    @Override
    public void onResume() {
        super.onResume();
        mAccountManagerFacade.addObserver(mAccountsChangedObserver);
        mProfileDataCache.addObserver(mProfileDataCacheObserver);
        triggerUpdateAccounts();

        mView.startAnimations();
    }

    @Override
    public void onPause() {
        super.onPause();
        mProfileDataCache.removeObserver(mProfileDataCacheObserver);
        mAccountManagerFacade.removeObserver(mAccountsChangedObserver);

        mView.stopAnimations();
    }

    private void selectAccount(String accountName, boolean isDefaultAccount) {
        mSelectedAccountName = accountName;
        mIsDefaultAccountSelected = isDefaultAccount;
        updateProfileData(mSelectedAccountName);

        AccountPickerDialogFragment accountPickerFragment = getAccountPickerDialogFragment();
        if (accountPickerFragment != null) {
            accountPickerFragment.updateSelectedAccount(accountName);
        }
    }

    private void triggerUpdateAccounts() {
        mAccountManagerFacade.getGoogleAccounts(this::updateAccounts);
    }

    private void updateAccounts(AccountManagerResult<List<Account>> accounts) {
        if (!isResumed()) {
            return;
        }

        mAccountNames = getAccountNames(accounts);
        mHasGmsError = mAccountNames == null;
        mView.getAcceptButton().setEnabled(!mHasGmsError);
        if (mHasGmsError) return;

        if (mAccountNames.isEmpty()) {
            mSelectedAccountName = null;
            mAccountSelectionPending = false;
            setHasAccounts(false);
            return;
        } else {
            setHasAccounts(true);
        }

        if (mAccountSelectionPending) {
            String defaultAccount = mAccountNames.get(0);
            String accountToSelect =
                    mRequestedAccountName != null ? mRequestedAccountName : defaultAccount;
            selectAccount(accountToSelect, accountToSelect.equals(defaultAccount));
            mAccountSelectionPending = false;
            mRequestedAccountName = null;
        }

        if (mSelectedAccountName != null && mAccountNames.contains(mSelectedAccountName)) return;

        if (mConfirmSyncDataStateMachine != null) {
            // Any dialogs that may have been showing are now invalid (they were created
            // for the previously selected account).
            mConfirmSyncDataStateMachine.cancel(/* isBeingDestroyed = */ false);
            mConfirmSyncDataStateMachine = null;
        }

        // Account for forced sign-in flow disappeared before the sign-in was completed.
        if (ChildAccountStatus.isChild(mChildAccountStatus)) {
            onSigninRefused();
            return;
        }

        selectAccount(mAccountNames.get(0), true);
        showAccountPicker(); // Show account picker so user can confirm the account selection.
    }

    @Nullable
    private List<String> getAccountNames(AccountManagerResult<List<Account>> accounts) {
        try {
            List<String> result = AccountUtils.toAccountNames(accounts.get());
            dismissGmsErrorDialog();
            dismissGmsUpdatingDialog();
            return result;
        } catch (GmsAvailabilityException e) {
            dismissGmsUpdatingDialog();
            if (e.isUserResolvableError()) {
                showGmsErrorDialog(e.getGmsAvailabilityReturnCode());
            } else {
                Log.e(TAG, "Unresolvable GmsAvailabilityException.", e);
            }
            return null;
        } catch (GmsJustUpdatedException e) {
            dismissGmsErrorDialog();
            showGmsUpdatingDialog();
            return null;
        } catch (AccountManagerDelegateException e) {
            Log.e(TAG, "Unknown exception from AccountManagerFacade.", e);
            dismissGmsErrorDialog();
            dismissGmsUpdatingDialog();
            return null;
        }
    }

    private void showGmsErrorDialog(int gmsErrorCode) {
        if (mGooglePlayServicesUpdateErrorHandler != null
                && mGooglePlayServicesUpdateErrorHandler.isShowing()) {
            return;
        }
        boolean cancelable = !IdentityServicesProvider.get()
                                      .getSigninManager(Profile.getLastUsedRegularProfile())
                                      .isForceSigninEnabled();
        mGooglePlayServicesUpdateErrorHandler =
                new UserRecoverableErrorHandler.ModalDialog(getActivity(), cancelable);
        mGooglePlayServicesUpdateErrorHandler.handleError(getActivity(), gmsErrorCode);
    }

    private void showGmsUpdatingDialog() {
        if (mGmsIsUpdatingDialog != null) {
            return;
        }
        // TODO(https://crbug.com/814728): Use DialogFragment here.
        mGmsIsUpdatingDialog = new AlertDialog.Builder(getActivity())
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
                SystemClock.elapsedRealtime() - mGmsIsUpdatingDialogShowTime);
    }
}
