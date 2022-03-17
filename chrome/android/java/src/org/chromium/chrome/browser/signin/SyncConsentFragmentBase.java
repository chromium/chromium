// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.accounts.Account;
import android.accounts.AccountManager;
import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.text.TextUtils;
import android.text.method.LinkMovementMethod;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.fragment.app.Fragment;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.consent_auditor.ConsentAuditorFeature;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.DisplayableProfileData;
import org.chromium.chrome.browser.signin.services.FREMobileIdentityConsistencyFieldTrial;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.ProfileDataCache;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninMetricsUtils;
import org.chromium.chrome.browser.ui.signin.ConfirmSyncDataStateMachine;
import org.chromium.chrome.browser.ui.signin.ConfirmSyncDataStateMachineDelegate;
import org.chromium.chrome.browser.ui.signin.ConsentTextTracker;
import org.chromium.chrome.browser.ui.signin.SigninUtils;
import org.chromium.chrome.browser.ui.signin.SigninView;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerCoordinator;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerDialogCoordinator;
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.components.externalauth.UserRecoverableErrorHandler;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.AccountsChangeObserver;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.AccountInfoServiceProvider;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManagerHolder;
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
 * Derived classes must implement {@link #onSyncAccepted}/{@link #onSyncRefused} to define
 * what happens after the signin flow.
 */
public abstract class SyncConsentFragmentBase
        extends Fragment implements AccountPickerCoordinator.Listener, AccountsChangeObserver,
                                    SigninManager.SignInStateObserver {
    private static final String ARGUMENT_ACCESS_POINT = "SyncConsentFragmentBase.AccessPoint";

    private static final String SETTINGS_LINK_OPEN = "<LINK1>";
    private static final String SETTINGS_LINK_CLOSE = "</LINK1>";

    private static final String ARGUMENT_ACCOUNT_NAME = "SyncConsentFragmentBase.AccountName";

    // This bundle argument is optional; it is set only if the child status cannot be reliably
    // inferred by looking at the last used regular profile, because child sign auto sign in may
    // not have completed.
    private static final String ARGUMENT_CHILD_ACCOUNT_STATUS =
            "SyncConsentFragmentBase.ChildAccountStatus";
    private static final String ARGUMENT_SIGNIN_FLOW_TYPE =
            "SyncConsentFragmentBase.SigninFlowType";

    private static final int ADD_ACCOUNT_REQUEST_CODE = 1;

    @IntDef({SigninFlowType.DEFAULT, SigninFlowType.CHOOSE_ACCOUNT, SigninFlowType.ADD_ACCOUNT})
    @Retention(RetentionPolicy.SOURCE)
    @interface SigninFlowType {
        int DEFAULT = 0;
        int CHOOSE_ACCOUNT = 1;
        int ADD_ACCOUNT = 2;
    }

    private final AccountManagerFacade mAccountManagerFacade;
    protected boolean mIsChild;

    private SigninView mView;
    private ConsentTextTracker mConsentTextTracker;

    private final ProfileDataCache.Observer mProfileDataCacheObserver;
    protected @Nullable String mSelectedAccountName;
    private ProfileDataCache mProfileDataCache;
    private boolean mDestroyed;
    private boolean mIsSigninInProgress;
    private boolean mCanUseGooglePlayServices;
    private boolean mRecordUndoSignin;
    private boolean mIsSignedInWithoutSync;
    protected @SigninAccessPoint int mSigninAccessPoint;
    private ModalDialogManager mModalDialogManager;
    private ConfirmSyncDataStateMachine mConfirmSyncDataStateMachine;
    private @Nullable AccountPickerDialogCoordinator mAccountPickerDialogCoordinator;

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
     *
     * This version of the method should be used where we cannot guarantee that child auto-signin
     * has completed and therefore the child status is explicitly provided.
     *
     * @param accessPoint The access point for starting sign-in flow.
     * @param accountName The account to preselect.
     * @param isChild Whether the selected account is a child one.
     */
    protected static Bundle createArguments(
            @SigninAccessPoint int accessPoint, String accountName, boolean isChild) {
        Bundle result = createArguments(accessPoint, accountName);
        result.putBoolean(ARGUMENT_CHILD_ACCOUNT_STATUS, isChild);
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
        mProfileDataCacheObserver = this::updateProfileData;
        mCanUseGooglePlayServices = true;
    }

    /** The sync consent was refused. */
    protected abstract void onSyncRefused();

    /**
     * The sync consent was accepted.
     * @param accountName The name of the account
     * @param settingsClicked Whether the user requested to see their sync settings
     * @param callback The callback invoke when sign-in process is finished or aborted
     */
    protected abstract void onSyncAccepted(
            String accountName, boolean settingsClicked, Runnable callback);

    @Override
    public void onAttach(Context context) {
        super.onAttach(context);
        mModalDialogManager = ((ModalDialogManagerHolder) getActivity()).getModalDialogManager();
    }

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        Bundle arguments = getArguments();
        mSigninAccessPoint = arguments.getInt(ARGUMENT_ACCESS_POINT, SigninAccessPoint.MAX);
        assert mSigninAccessPoint != SigninAccessPoint.MAX : "Cannot find SigninAccessPoint!";

        // TODO(crbug.com/1306971): remove usage of Profile.isChild() and the need for a bundle
        // argument in the FRE, but moving to a new API for determining device supervision status.
        mSelectedAccountName = arguments.getString(ARGUMENT_ACCOUNT_NAME, null);
        if (arguments.containsKey(ARGUMENT_CHILD_ACCOUNT_STATUS)) {
            mIsChild = arguments.getBoolean(ARGUMENT_CHILD_ACCOUNT_STATUS);
        } else {
            mIsChild = Profile.getLastUsedRegularProfile().isChild();
        }

        @SigninFlowType
        int signinFlowType = arguments.getInt(ARGUMENT_SIGNIN_FLOW_TYPE, SigninFlowType.DEFAULT);

        if (savedInstanceState == null) {
            // If this fragment is being recreated from a saved state there's no need to show
            // account picked or starting AddAccount flow.
            if (signinFlowType == SigninFlowType.CHOOSE_ACCOUNT) {
                mAccountPickerDialogCoordinator = new AccountPickerDialogCoordinator(
                        requireContext(), this, mModalDialogManager);
            } else if (signinFlowType == SigninFlowType.ADD_ACCOUNT) {
                addAccount();
            }
        }

        mConsentTextTracker = new ConsentTextTracker(getResources());

        mProfileDataCache = mIsChild
                ? ProfileDataCache.createWithDefaultImageSize(
                        requireContext(), R.drawable.ic_account_child_20dp)
                : ProfileDataCache.createWithDefaultImageSizeAndNoBadge(requireContext());
        mProfileDataCache.addObserver(mProfileDataCacheObserver);

        IdentityServicesProvider.get()
                .getSigninManager(Profile.getLastUsedRegularProfile())
                .addSignInStateObserver(this);

        // By default this is set to true so that when system back button is pressed user action
        // is recorded in onDestroy().
        mRecordUndoSignin = true;
        SigninMetricsUtils.logSigninStartAccessPoint(mSigninAccessPoint);
        SigninMetricsUtils.logSigninUserActionForAccessPoint(mSigninAccessPoint);
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        IdentityServicesProvider.get()
                .getSigninManager(Profile.getLastUsedRegularProfile())
                .removeSignInStateObserver(this);
        mProfileDataCache.removeObserver(mProfileDataCacheObserver);
        if (mConfirmSyncDataStateMachine != null) {
            mConfirmSyncDataStateMachine.cancel(/* isBeingDestroyed = */ true);
            mConfirmSyncDataStateMachine = null;
        }
        mModalDialogManager.destroy();
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
        if (mIsChild) {
            endImageViewDrawable = SigninView.getCheckmarkDrawable(getContext());
            if (!ChromeFeatureList.isEnabled(ChromeFeatureList.ALLOW_SYNC_OFF_FOR_CHILD_ACCOUNTS)) {
                mView.getRefuseButton().setVisibility(View.GONE);
                mView.getAcceptButtonEndPadding().setVisibility(View.INVISIBLE);
            }
        } else {
            endImageViewDrawable = SigninView.getExpandArrowDrawable(getContext());
        }
        mView.getAccountPickerEndImageView().setImageDrawable(endImageViewDrawable);

        updateConsentText();
        final CoreAccountInfo primaryAccount =
                IdentityServicesProvider.get()
                        .getIdentityManager(Profile.getLastUsedRegularProfile())
                        .getPrimaryAccountInfo(ConsentLevel.SIGNIN);
        mIsSignedInWithoutSync = (FREMobileIdentityConsistencyFieldTrial.isEnabled()
                && mSigninAccessPoint == SigninAccessPoint.START_PAGE && primaryAccount != null);
        if (mIsSignedInWithoutSync) {
            mSelectedAccountName = primaryAccount.getEmail();
        }
        setHasAccounts(true);

        // When a fragment that was in the FragmentManager backstack becomes visible again, the view
        // will be recreated by onCreateView. Update the state of this recreated UI.
        if (mSelectedAccountName != null) {
            updateProfileData(mSelectedAccountName);
        }
        return mView;
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);
        boolean cancelable = !mIsChild;
        mCanUseGooglePlayServices = ExternalAuthUtils.getInstance().canUseGooglePlayServices(
                new UserRecoverableErrorHandler.ModalDialog(requireActivity(), cancelable));
        mView.getAcceptButton().setEnabled(mCanUseGooglePlayServices);
    }

    /**
     * Implements {@link AccountsChangeObserver}.
     */
    @Override
    public void onAccountsChanged() {
        mAccountManagerFacade.getAccounts().then(this::updateAccounts);
    }

    /** Implements {@link SigninManager.SignInStateObserver}. */
    @Override
    public void onSignedIn() {
        final CoreAccountInfo primaryAccount =
                IdentityServicesProvider.get()
                        .getIdentityManager(Profile.getLastUsedRegularProfile())
                        .getPrimaryAccountInfo(ConsentLevel.SIGNIN);
        mIsSignedInWithoutSync = (FREMobileIdentityConsistencyFieldTrial.isEnabled()
                && mSigninAccessPoint == SigninAccessPoint.START_PAGE && primaryAccount != null);
        if (mIsSignedInWithoutSync) {
            mSelectedAccountName = primaryAccount.getEmail();
            mAccountManagerFacade.getAccounts().then(this::updateAccounts);
        }
    }

    /**
     * Account picker is hidden if there are no accounts on the device. Also, accept button
     * becomes "Add account" button in this case.
     */
    private void setHasAccounts(boolean hasAccounts) {
        if (hasAccounts) {
            final boolean hideAccountPicker = mIsSignedInWithoutSync
                    || (FREMobileIdentityConsistencyFieldTrial.isEnabled() && mIsChild);
            mView.getAccountPickerView().setVisibility(
                    hideAccountPicker ? View.GONE : View.VISIBLE);
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
                ? new NoUnderlineClickableSpan(getContext(), this::onSettingsLinkClicked)
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
        mConsentTextTracker.setText(
                mView.getSyncDescriptionView(), R.string.signin_sync_description);

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
        if (mIsChild || !areControlsEnabled()) return;
        mAccountPickerDialogCoordinator =
                new AccountPickerDialogCoordinator(requireContext(), this, mModalDialogManager);
    }

    private void onRefuseButtonClicked(View button) {
        RecordUserAction.record("Signin_Undo_Signin");
        mRecordUndoSignin = false;
        onSyncRefused();
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
        return !mIsSigninInProgress && mCanUseGooglePlayServices;
    }

    private void seedAccountsAndSignin(boolean settingsClicked, View confirmationView) {
        AccountInfoServiceProvider.get()
                .getAccountInfoByEmail(mSelectedAccountName)
                .then(accountInfo -> {
                    assert accountInfo != null : "The seeded CoreAccountInfo shouldn't be null";
                    mConsentTextTracker.recordConsent(accountInfo.getId(),
                            ConsentAuditorFeature.CHROME_SYNC, (TextView) confirmationView, mView);
                    if (isResumed()) {
                        runStateMachineAndSignin(settingsClicked);
                    }
                });
    }

    private void runStateMachineAndSignin(boolean settingsClicked) {
        mConfirmSyncDataStateMachine = new ConfirmSyncDataStateMachine(
                new ConfirmSyncDataStateMachineDelegate(
                        requireContext(), getChildFragmentManager(), mModalDialogManager),
                UserPrefs.get(Profile.getLastUsedRegularProfile())
                        .getString(Pref.GOOGLE_SERVICES_LAST_USERNAME),
                mSelectedAccountName, new ConfirmSyncDataStateMachine.Listener() {
                    @Override
                    public void onConfirm(boolean wipeData) {
                        mConfirmSyncDataStateMachine = null;

                        // Don't start sign-in if this fragment has been destroyed.
                        if (mDestroyed) return;

                        SigninManager signinManager =
                                IdentityServicesProvider.get().getSigninManager(
                                        Profile.getLastUsedRegularProfile());
                        signinManager.runAfterOperationInProgress(() -> {
                            if (wipeData) {
                                signinManager.wipeSyncUserData(() -> {
                                    onSyncAccepted(mSelectedAccountName, settingsClicked,
                                            () -> mIsSigninInProgress = false);
                                });
                            } else {
                                onSyncAccepted(mSelectedAccountName, settingsClicked,
                                        () -> mIsSigninInProgress = false);
                            }
                        });
                    }

                    @Override
                    public void onCancel() {
                        mConfirmSyncDataStateMachine = null;
                        mIsSigninInProgress = false;
                    }
                });
    }

    @Override
    public void onAccountSelected(String accountName) {
        selectAccount(accountName);
        mAccountPickerDialogCoordinator.dismissDialog();
    }

    @Override
    public void addAccount() {
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
        if (requestCode == ADD_ACCOUNT_REQUEST_CODE && resultCode == Activity.RESULT_OK
                && data != null) {
            String addedAccountName = data.getStringExtra(AccountManager.KEY_ACCOUNT_NAME);
            if (addedAccountName == null) return;

            // Found the account name, dismiss the dialog if it is shown
            if (mAccountPickerDialogCoordinator != null) {
                mAccountPickerDialogCoordinator.dismissDialog();
            }

            mSelectedAccountName = addedAccountName;
        }
    }

    @Override
    public void onResume() {
        super.onResume();
        mAccountManagerFacade.addObserver(this);
        updateAccounts(
                AccountUtils.getAccountsIfFulfilledOrEmpty(mAccountManagerFacade.getAccounts()));

        mView.startAnimations();
    }

    @Override
    public void onPause() {
        super.onPause();
        mAccountManagerFacade.removeObserver(this);

        mView.stopAnimations();
    }

    private void selectAccount(String accountName) {
        mSelectedAccountName = accountName;
        updateProfileData(mSelectedAccountName);
    }

    protected void updateAccounts(List<Account> accounts) {
        if (!isResumed() || !mCanUseGooglePlayServices) {
            return;
        }
        if (accounts.isEmpty()) {
            mSelectedAccountName = null;
            setHasAccounts(false);
            return;
        }
        setHasAccounts(true);
        final String defaultAccount = accounts.get(0).name;
        if (mIsSignedInWithoutSync) {
            return;
        }

        if (mSelectedAccountName != null
                && AccountUtils.findAccountByName(accounts, mSelectedAccountName) != null) {
            selectAccount(mSelectedAccountName);
            return;
        }

        if (mConfirmSyncDataStateMachine != null) {
            // Any dialogs that may have been showing are now invalid (they were created
            // for the previously selected account).
            mConfirmSyncDataStateMachine.cancel(/* isBeingDestroyed = */ false);
            mConfirmSyncDataStateMachine = null;
        }

        // Account for forced sign-in flow disappeared before the sign-in was completed.
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.ALLOW_SYNC_OFF_FOR_CHILD_ACCOUNTS)
                && mIsChild) {
            onSyncRefused();
            return;
        }

        if (mSelectedAccountName != null) {
            // Show account picker to user to confirm the account selection if
            // the original selected account is removed.
            mAccountPickerDialogCoordinator =
                    new AccountPickerDialogCoordinator(requireContext(), this, mModalDialogManager);
        }
        selectAccount(defaultAccount);
    }
}
