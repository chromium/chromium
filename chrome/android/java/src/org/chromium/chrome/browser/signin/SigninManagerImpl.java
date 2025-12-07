// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import static org.chromium.build.NullUtil.assumeNonNull;

import androidx.annotation.IntDef;
import androidx.annotation.MainThread;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.base.CollectionUtil;
import org.chromium.base.ContextUtils;
import org.chromium.base.DeviceInfo;
import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.browsing_data.BrowsingDataBridge;
import org.chromium.chrome.browser.browsing_data.BrowsingDataType;
import org.chromium.chrome.browser.browsing_data.TimePeriod;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninPreferencesManager;
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.components.prefs.PrefChangeRegistrar;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.AccountsChangeObserver;
import org.chromium.components.signin.SigninFeatureMap;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.AccountInfoServiceProvider;
import org.chromium.components.signin.identitymanager.AccountManagedStatusFinder;
import org.chromium.components.signin.identitymanager.AccountManagedStatusFinderOutcome;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.identitymanager.IdentityMutator;
import org.chromium.components.signin.identitymanager.PrimaryAccountError;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.signin.metrics.SignoutReason;
import org.chromium.google_apis.gaia.CoreAccountId;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.time.Duration;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Objects;

/**
 * Android wrapper of the SigninManager which provides access from the Java layer.
 *
 * <p>This class handles common paths during the sign-in and sign-out flows.
 *
 * <p>Only usable from the UI thread as the native SigninManager requires its access to be in the UI
 * thread.
 *
 * <p>See chrome/browser/android/signin/signin_manager_android.h for more details.
 */
@NullMarked
class SigninManagerImpl implements SigninManager, AccountsChangeObserver {
    private static final String TAG = "SigninManager";

    private static final Duration MANAGED_STATUS_TIMEOUT = Duration.ofSeconds(10);

    /**
     * Address of the native Signin Manager android. This is not final, as destroy() updates this.
     */
    private long mNativeSigninManagerAndroid;

    private final Profile mProfile;
    private final AccountManagerFacade mAccountManagerFacade;
    private final IdentityManager mIdentityManager;
    private final IdentityMutator mIdentityMutator;
    private final ObserverList<SignInStateObserver> mSignInStateObservers = new ObserverList<>();
    private final List<Runnable> mCallbacksWaitingForPendingOperation = new ArrayList<>();
    private final PrefChangeRegistrar mPrefChangeRegistrar;
    private final PrefService mPrefService;

    /**
     * Will be set during the sign in process, and nulled out when there is not a pending sign in.
     * Needs to be null checked after ever async entry point because it can be nulled out at any
     * time by system accounts changing.
     */
    private @Nullable SignInState mSignInState;

    /**
     * Set during sign-out process and nulled out once complete. Helps to atomically gather/clear
     * various sign-out state.
     */
    private @Nullable SignOutState mSignOutState;

    /** Tracks whether deletion of browsing data is in progress. */
    private boolean mWipeUserDataInProgress;

    /**
     * Called by native to create an instance of SigninManager.
     *
     * @param nativeSigninManagerAndroid A pointer to native's SigninManagerAndroid.
     */
    @CalledByNative
    @VisibleForTesting
    static SigninManager create(
            long nativeSigninManagerAndroid,
            @JniType("Profile*") Profile profile,
            @JniType("PrefService*") PrefService prefService,
            @JniType("signin::IdentityManager*") IdentityManager identityManager,
            IdentityMutator identityMutator) {
        assert nativeSigninManagerAndroid != 0;
        assert profile != null;
        assert prefService != null;
        assert identityManager != null;
        assert identityMutator != null;
        final SigninManagerImpl signinManager =
                new SigninManagerImpl(
                        nativeSigninManagerAndroid,
                        profile,
                        prefService,
                        identityManager,
                        identityMutator);

        AccountInfoServiceProvider.init(identityManager);

        return signinManager;
    }

    private SigninManagerImpl(
            long nativeSigninManagerAndroid,
            Profile profile,
            PrefService prefService,
            IdentityManager identityManager,
            IdentityMutator identityMutator) {
        ThreadUtils.assertOnUiThread();
        mNativeSigninManagerAndroid = nativeSigninManagerAndroid;
        mProfile = profile;
        mPrefService = prefService;
        mIdentityManager = identityManager;
        mIdentityMutator = identityMutator;

        mAccountManagerFacade = AccountManagerFacadeProvider.getInstance();
        mAccountManagerFacade.addObserver(this);
        var accountsPromise = mAccountManagerFacade.getAccounts();
        if (accountsPromise.isFulfilled()
                && (mAccountManagerFacade.didAccountFetchSucceed()
                        || !accountsPromise.getResult().isEmpty())) {
            seedThenReloadAllAccountsFromSystem(
                    mAccountManagerFacade.getAccounts().getResult(),
                    CoreAccountInfo.getIdFrom(
                            identityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN)));
        }
        mPrefChangeRegistrar = new PrefChangeRegistrar(mPrefService);
        mPrefChangeRegistrar.addObserver(Pref.SIGNIN_ALLOWED, this::notifySignInAllowedChanged);
    }

    /**
     * Triggered during SigninManagerAndroidWrapper's KeyedService::Shutdown. Drop references with
     * external services and native.
     */
    @VisibleForTesting
    @CalledByNative
    void destroy() {
        AccountInfoServiceProvider.get().destroy();
        mAccountManagerFacade.removeObserver(this);
        mPrefChangeRegistrar.destroy();
        mNativeSigninManagerAndroid = 0;
    }

    /** Implements {@link AccountsChangeObserver}. */
    @Override
    public void onCoreAccountInfosChanged() {
        var accountsPromise = mAccountManagerFacade.getAccounts();
        assert accountsPromise.isFulfilled();
        List<AccountInfo> accounts = accountsPromise.getResult();
        if (!mAccountManagerFacade.didAccountFetchSucceed() && accounts.isEmpty()) {
            // If the account fetch did not succeed, the AccountManagerFacade falls back to an empty
            // list. Do nothing when this is the case.
            return;
        }

        @Nullable CoreAccountInfo primaryAccountInfo =
                mIdentityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN);
        if (primaryAccountInfo == null) {
            seedThenReloadAllAccountsFromSystem(accounts, null);
            return;
        }
        if (AccountUtils.findAccountByGaiaId(accounts, primaryAccountInfo.getGaiaId()) != null) {
            // The primary account is still on the device, reseed accounts.
            seedThenReloadAllAccountsFromSystem(
                    accounts, CoreAccountInfo.getIdFrom(primaryAccountInfo));
            return;
        }
        if (isOperationInProgress()) {
            // Re-check whether there's still a primary account after the current operation.
            runAfterOperationInProgress(this::onCoreAccountInfosChanged);
        } else {
            // Sign out if the current primary account is no longer on the device.
            signOut(SignoutReason.ACCOUNT_REMOVED_FROM_DEVICE);
        }
    }

    /**
     * Updates the email of the primary account stored in shared preferences to match the one used
     * by the native component. Sets the email of the primary account stored in shared preferences
     * to null in case the user is signed out.
     */
    private void maybeUpdateLegacyPrimaryAccountEmail() {
        CoreAccountInfo accountInfo = mIdentityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN);
        if (Objects.equals(
                CoreAccountInfo.getEmailFrom(accountInfo),
                SigninPreferencesManager.getInstance().getLegacyPrimaryAccountEmail())) {
            return;
        }
        SigninPreferencesManager.getInstance()
                .setLegacyPrimaryAccountEmail(CoreAccountInfo.getEmailFrom(accountInfo));
    }

    /** Extracts the domain name of a given account's email. */
    @Override
    public String extractDomainName(String accountEmail) {
        return SigninManagerImplJni.get().extractDomainName(accountEmail);
    }

    /** Returns the IdentityManager used by SigninManager. */
    @Override
    public IdentityManager getIdentityManager() {
        return mIdentityManager;
    }

    /** Returns true if sign in can be started now. */
    @Override
    public boolean isSigninAllowed() {
        return mSignInState == null
                && mPrefService.getBoolean(Pref.SIGNIN_ALLOWED)
                && mIdentityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN) == null
                && isSigninSupported(/* requireUpdatedPlayServices= */ false);
    }

    /** Returns true if sign out can be started now. */
    @Override
    public boolean isSignOutAllowed() {
        return mSignOutState == null
                && mSignInState == null
                && mIdentityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN) != null
                && mIdentityManager.isClearPrimaryAccountAllowed();
    }

    /**
     * Returns whether the user can sign-in (maybe after an update to Google Play services).
     *
     * @param requireUpdatedPlayServices Indicates whether an updated version of play services is
     *     required or not.
     */
    @Override
    public boolean isSigninSupported(boolean requireUpdatedPlayServices) {
        if (DeviceInfo.isRetailDemoMode()) {
            return false;
        }
        if (requireUpdatedPlayServices) {
            return ExternalAuthUtils.getInstance().canUseGooglePlayServices();
        }
        return !ExternalAuthUtils.getInstance()
                .isGooglePlayServicesMissing(ContextUtils.getApplicationContext());
    }

    /**
     * @return Whether force sign-in is enabled by policy.
     */
    @Override
    public boolean isForceSigninEnabled() {
        return SigninManagerImplJni.get().isForceSigninEnabled(mNativeSigninManagerAndroid);
    }

    /** Registers a SignInStateObserver to be notified when the user signs in or out of Chrome. */
    @Override
    public void addSignInStateObserver(SignInStateObserver observer) {
        mSignInStateObservers.addObserver(observer);
    }

    /** Unregisters a SignInStateObserver to be notified when the user signs in or out of Chrome. */
    @Override
    public void removeSignInStateObserver(SignInStateObserver observer) {
        mSignInStateObservers.removeObserver(observer);
    }

    private void notifySignInAllowedChanged() {
        PostTask.postTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    for (SignInStateObserver observer : mSignInStateObservers) {
                        observer.onSignInAllowedChanged();
                    }
                });
    }

    private void notifySignOutAllowedChanged() {
        PostTask.postTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    for (SignInStateObserver observer : mSignInStateObservers) {
                        observer.onSignOutAllowedChanged();
                    }
                });
    }

    @Override
    public void signin(
            CoreAccountInfo coreAccountInfo,
            @SigninAccessPoint int accessPoint,
            @Nullable SignInCallback callback) {
        signinInternal(SignInState.createForSignin(accessPoint, coreAccountInfo, callback));
    }

    @Override
    public void turnOnSyncForTesting(
            CoreAccountInfo coreAccountInfo, @SigninAccessPoint int accessPoint) {
        CoreAccountInfo primaryAccountInfo =
                mIdentityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN);
        assert primaryAccountInfo != null && primaryAccountInfo.equals(coreAccountInfo)
                : "Must be signed-in to turn on sync ";
        @PrimaryAccountError
        int primaryAccountError =
                mIdentityMutator.setPrimaryAccount(
                        coreAccountInfo.getId(), ConsentLevel.SYNC, accessPoint, () -> {});
        assert primaryAccountError == PrimaryAccountError.NO_ERROR
                : "Encountered error: " + primaryAccountError;
    }

    private void signinInternal(SignInState signInState) {
        if (signInState == null) {
            throw new IllegalArgumentException("SigninState shouldn't be null!");
        }
        if (!isSigninAllowed()) {
            throw new IllegalStateException(
                    String.format(
                            "Sign-in isn't allowed!\n"
                                    + "  mSignInState: %s\n"
                                    + "  Pref.SIGNIN_ALLOWED: %s\n"
                                    + "  Signed-in account: %s",
                            mSignInState,
                            mPrefService.getBoolean(Pref.SIGNIN_ALLOWED),
                            mIdentityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN)));
        }

        // The mSignInState must be updated prior to the async processing below, as this indicates
        // that a signin operation is in progress and prevents other sign in operations from being
        // started until this one completes (see {@link isOperationInProgress()}).
        mSignInState = signInState;

        if (!SigninFeatureMap.isEnabled(SigninFeatures.SKIP_CHECK_FOR_ACCOUNT_MANAGEMENT_ON_SIGNIN)
                && !getUserAcceptedAccountManagement()) {
            isAccountManaged(
                    mSignInState.mCoreAccountInfo,
                    (Boolean isAccountManaged) -> {
                        if (isAccountManaged) {
                            throw new IllegalStateException(
                                    "User must accept Account Management before "
                                            + "signing into a Managed account.");
                        } else {
                            signinInternalAfterCheckingManagedState();
                        }
                    });
        } else {
            signinInternalAfterCheckingManagedState();
        }
    }

    private void signinInternalAfterCheckingManagedState() {
        // Retrieve the primary account and use it to seed and reload all accounts.
        if (!mAccountManagerFacade.getAccounts().isFulfilled()) {
            throw new IllegalStateException("Account information should be available on signin");
        }
        if (mSignInState == null || mSignInState.mCoreAccountInfo == null) {
            throw new IllegalStateException(
                    "The account should be on the device before it can be set as primary.");
        }
        if (!SigninFeatureMap.isEnabled(
                SigninFeatures.MAKE_ACCOUNTS_AVAILABLE_IN_IDENTITY_MANAGER)) {
            seedThenReloadAllAccountsFromSystem(
                    mAccountManagerFacade.getAccounts().getResult(),
                    mSignInState.mCoreAccountInfo.getId());
        }
        notifySignInAllowedChanged();

        Log.d(TAG, "Checking if account has policy management enabled");
        fetchAndApplyCloudPolicy(
                mSignInState.mCoreAccountInfo, this::finishSignInAfterPolicyEnforced);
    }

    /**
     * Finishes the sign-in flow. If the user is managed, the policy should be fetched and enforced
     * before calling this method.
     */
    private void finishSignInAfterPolicyEnforced() {
        assert mSignInState != null : "SigninState shouldn't be null!";
        assert !mIdentityManager.hasPrimaryAccount(ConsentLevel.SIGNIN)
                : "The user should not be already signed in";

        // Retain the sign-in callback since pref commit callback will be called after sign-in is
        // considered completed and sign-in state is reset.
        final SignInCallback signInCallback = mSignInState.mCallback;
        @PrimaryAccountError
        int primaryAccountError =
                mIdentityMutator.setPrimaryAccount(
                        mSignInState.mCoreAccountInfo.getId(),
                        ConsentLevel.SIGNIN,
                        mSignInState.getAccessPoint(),
                        () -> {
                            Log.d(TAG, "Sign-in native prefs written.");
                            if (signInCallback != null) {
                                signInCallback.onPrefsCommitted();
                            }
                        });

        if (primaryAccountError != PrimaryAccountError.NO_ERROR) {
            Log.w(
                    TAG,
                    "SetPrimaryAccountError in IdentityManager: %d, aborting signin",
                    primaryAccountError);
            abortSignIn();
            return;
        }

        // Should be called after setting the primary account.
        maybeUpdateLegacyPrimaryAccountEmail();

        if (mSignInState.mCallback != null) {
            mSignInState.mCallback.onSignInComplete();
        }

        Log.i(TAG, "Signin completed.");
        mSignInState = null;
        notifyCallbacksWaitingForOperation();
        notifySignInAllowedChanged();
        notifySignOutAllowedChanged();

        for (SignInStateObserver observer : mSignInStateObservers) {
            observer.onSignedIn();
        }
    }

    @Override
    @MainThread
    public void runAfterOperationInProgress(Runnable runnable) {
        ThreadUtils.assertOnUiThread();
        if (isOperationInProgress()) {
            mCallbacksWaitingForPendingOperation.add(runnable);
            return;
        }
        PostTask.postTask(TaskTraits.UI_DEFAULT, runnable);
    }

    /**
     * Whether an operation is in progress for which we should wait before scheduling users of
     * {@link runAfterOperationInProgress}.
     */
    private boolean isOperationInProgress() {
        ThreadUtils.assertOnUiThread();
        return mSignInState != null || mSignOutState != null || mWipeUserDataInProgress;
    }

    /**
     * Maybe notifies any callbacks registered via runAfterOperationInProgress().
     *
     * <p>The callbacks are notified in FIFO order, and each callback is only notified if there is
     * no outstanding work (either work which was outstanding at the time the callback was added, or
     * which was scheduled by subsequently dequeued callbacks).
     */
    private void notifyCallbacksWaitingForOperation() {
        ThreadUtils.assertOnUiThread();
        while (!mCallbacksWaitingForPendingOperation.isEmpty()) {
            if (isOperationInProgress()) return;
            Runnable callback = mCallbacksWaitingForPendingOperation.remove(0);
            PostTask.postTask(TaskTraits.UI_DEFAULT, callback);
        }
    }

    /** Initialize SignOutState, and call identity mutator to revoke the sync consent. */
    @Override
    public void revokeSyncConsent(
            @SignoutReason int signoutSource,
            @Nullable SignOutCallback signOutCallback,
            boolean forceWipeUserData) {
        // Only one signOut at a time!
        assert mSignOutState == null;
        // User must be syncing.
        assert mIdentityManager.hasPrimaryAccount(ConsentLevel.SYNC);

        // Grab the management domain before nativeSignOut() potentially clears it.
        String managementDomain = getManagementDomain();

        // We wipe sync data only, as wiping the profile data would also trigger sign-out.
        mSignOutState =
                new SignOutState(
                        signOutCallback,
                        (forceWipeUserData || managementDomain != null)
                                ? SignOutState.DataWipeAction.WIPE_SYNC_DATA_ONLY
                                : SignOutState.DataWipeAction.WIPE_SIGNIN_DATA_ONLY);
        Log.i(
                TAG,
                "Revoking sync consent, dataWipeAction: %d",
                (forceWipeUserData || managementDomain != null)
                        ? SignOutState.DataWipeAction.WIPE_SYNC_DATA_ONLY
                        : SignOutState.DataWipeAction.WIPE_SIGNIN_DATA_ONLY);

        mIdentityMutator.revokeSyncConsent(signoutSource);

        notifySignOutAllowedChanged();
        disableSyncAndWipeData(this::finishSignOut);
    }

    /**
     * Signs out of Chrome. This method clears the signed-in username, stops sync and sends out a
     * sign-out notification on the native side.
     *
     * @param signoutSource describes the event driving the signout (e.g.
     *         {@link SignoutReason.USER_CLICKED_SIGNOUT_SETTINGS}).
     * @param signOutCallback Callback to notify about the sign-out progress.
     * @param forceWipeUserData Whether user selected to wipe all device data.
     */
    @Override
    public void signOut(
            @SignoutReason int signoutSource,
            @Nullable SignOutCallback signOutCallback,
            boolean forceWipeUserData) {
        // Only one signOut at a time!
        assert mSignOutState == null;

        // Check the management domain before nativeSignOut() potentially clears it.
        boolean shouldWipeBecauseOfAccountManagement =
                getManagementDomain() != null
                        && mIdentityManager.hasPrimaryAccount(ConsentLevel.SYNC);
        @SignOutState.DataWipeAction
        int dataWipeAction =
                (forceWipeUserData || shouldWipeBecauseOfAccountManagement)
                        ? SignOutState.DataWipeAction.WIPE_ALL_PROFILE_DATA
                        : SignOutState.DataWipeAction.WIPE_SIGNIN_DATA_ONLY;
        mSignOutState = new SignOutState(signOutCallback, dataWipeAction);
        Log.i(TAG, "Signing out, dataWipeAction: %d", dataWipeAction);

        mIdentityMutator.clearPrimaryAccount(signoutSource);

        notifySignOutAllowedChanged();
        disableSyncAndWipeData(this::finishSignOut);
    }

    /**
     * Returns the management domain if the signed in account is managed, otherwise returns null.
     */
    @Override
    public @Nullable String getManagementDomain() {
        return SigninManagerImplJni.get().getManagementDomain(mNativeSigninManagerAndroid);
    }

    /**
     * Aborts the current sign in.
     *
     * Package protected to allow dialog fragments to abort the signin flow.
     */
    private void abortSignIn() {
        // Ensure this function can only run once per signin flow.
        SignInState signInState = mSignInState;
        assert signInState != null;
        mSignInState = null;
        notifyCallbacksWaitingForOperation();

        RecordHistogram.recordEnumeratedHistogram(
                "Signin.SigninAbortedAccessPoint",
                signInState.getAccessPoint(),
                SigninAccessPoint.MAX_VALUE);

        if (signInState.mCallback != null) {
            signInState.mCallback.onSignInAborted();
        }

        stopApplyingCloudPolicy();

        Log.d(TAG, "Signin flow aborted.");
        notifySignInAllowedChanged();
        if (!SigninFeatureMap.isEnabled(
                SigninFeatures.MAKE_ACCOUNTS_AVAILABLE_IN_IDENTITY_MANAGER)) {
            seedThenReloadAllAccountsFromSystem(
                    mAccountManagerFacade.getAccounts().getResult(), null);
        }
    }

    @VisibleForTesting
    void finishSignOut() {
        // Should be set at start of sign-out flow.
        assert mSignOutState != null;

        // After sign-out, reset the Sync promo show count, so the user will see Sync promos
        // again.
        ChromeSharedPreferences.getInstance()
                .writeInt(
                        ChromePreferenceKeys.SYNC_PROMO_SHOW_COUNT.createKey(
                                SigninPreferencesManager.SigninPromoAccessPointId.NTP),
                        0);
        SignOutCallback signOutCallback = mSignOutState.mSignOutCallback;
        if (mAccountManagerFacade.getAccounts().isFulfilled()
                && !SigninFeatureMap.isEnabled(
                        SigninFeatures.MAKE_ACCOUNTS_AVAILABLE_IN_IDENTITY_MANAGER)) {
            // We don't reload the accounts if they are not yet available.
            // They will be seeded in onCoreAccountInfosChanged() when they become available.
            seedThenReloadAllAccountsFromSystem(
                    mAccountManagerFacade.getAccounts().getResult(), null);
        }
        mSignOutState = null;

        if (signOutCallback != null) signOutCallback.signOutComplete();
        notifyCallbacksWaitingForOperation();

        for (SignInStateObserver observer : mSignInStateObservers) {
            observer.onSignedOut();
        }
    }

    @Override
    public void isAccountManaged(
            @Nullable CoreAccountInfo account, final Callback<Boolean> callback) {
        if (account == null) throw new IllegalArgumentException("Account shouldn't be null!");

        Callback<Integer> finderCallback =
                (outcome) -> {
                    boolean isManaged =
                            outcome == AccountManagedStatusFinderOutcome.ENTERPRISE
                                    || outcome
                                            == AccountManagedStatusFinderOutcome
                                                    .ENTERPRISE_GOOGLE_DOT_COM;
                    callback.onResult(isManaged);
                };
        AccountManagedStatusFinder finder =
                new AccountManagedStatusFinder(
                        getIdentityManager(), account, finderCallback, MANAGED_STATUS_TIMEOUT);
        if (finder.getOutcome() != AccountManagedStatusFinderOutcome.PENDING) {
            finderCallback.onResult(finder.getOutcome());
        }
        // `destroy` for `finder` will be called automatically when the outcome is decided (or
        // when the timeout is reached).
    }

    private void seedThenReloadAllAccountsFromSystem(
            List<AccountInfo> accounts, @Nullable CoreAccountId primaryAccountId) {
        if (primaryAccountId != null
                && AccountUtils.findAccountByAccountId(accounts, primaryAccountId) == null) {
            throw new IllegalStateException(
                    "Primary account should exist in the list of accounts when seeding");
        }
        mIdentityMutator.seedAccountsThenReloadAllAccountsWithPrimaryAccount(
                accounts, primaryAccountId);
        mIdentityManager.refreshAccountInfoIfStale(accounts);
        // Should be called after re-seeding accounts to make sure that we get the new email.
        maybeUpdateLegacyPrimaryAccountEmail();
    }

    /**
     * Wipes the user's bookmarks and sync data.
     *
     * @param wipeDataCallback A callback which will be called once the data is wiped.
     * @param dataWipeOption What kind of data to delete.
     */
    @Override
    public void wipeSyncUserData(Runnable wipeDataCallback, @DataWipeOption int dataWipeOption) {
        assert !mWipeUserDataInProgress;
        mWipeUserDataInProgress = true;

        switch (dataWipeOption) {
            case DataWipeOption.WIPE_SYNC_DATA:
                wipeSyncUserDataOnly(wipeDataCallback);
                break;
            case DataWipeOption.WIPE_ALL_PROFILE_DATA:
                SigninManagerImplJni.get()
                        .wipeProfileData(
                                mNativeSigninManagerAndroid,
                                () -> {
                                    mWipeUserDataInProgress = false;
                                    wipeDataCallback.run();
                                    notifyCallbacksWaitingForOperation();
                                });
                break;
        }
    }

    // TODO(crbug.com/40806620): this function and disableSyncAndWipeData() have very similar
    // functionality, but with different implementations.  Consider merging them.
    // TODO(crbug.com/40806620): add test coverage for this function (including its effect on
    // notifyCallbacksWaitingForOperation()), after resolving the TODO above.
    private void wipeSyncUserDataOnly(Runnable wipeDataCallback) {
        final BookmarkModel model = BookmarkModel.getForProfile(mProfile);
        model.finishLoadingBookmarkModel(
                new Runnable() {
                    @Override
                    public void run() {
                        List<Integer> clearedTypes =
                                new ArrayList<>(
                                        Arrays.asList(
                                                BrowsingDataType.HISTORY,
                                                BrowsingDataType.CACHE,
                                                BrowsingDataType.SITE_DATA,
                                                BrowsingDataType.FORM_DATA));
                        model.removeAllUserBookmarks();
                        BrowsingDataBridge.getForProfile(mProfile)
                                .clearBrowsingData(
                                        new BrowsingDataBridge.OnClearBrowsingDataListener() {
                                            @Override
                                            public void onBrowsingDataCleared() {
                                                assert mWipeUserDataInProgress;
                                                mWipeUserDataInProgress = false;
                                                wipeDataCallback.run();
                                                notifyCallbacksWaitingForOperation();
                                            }
                                        },
                                        CollectionUtil.integerCollectionToIntArray(clearedTypes),
                                        TimePeriod.ALL_TIME);
                    }
                });
    }

    @Override
    public void setUserAcceptedAccountManagement(boolean acceptedAccountManagement) {
        SigninManagerImplJni.get()
                .setUserAcceptedAccountManagement(
                        mNativeSigninManagerAndroid, acceptedAccountManagement);
    }

    @Override
    public boolean getUserAcceptedAccountManagement() {
        return SigninManagerImplJni.get()
                .getUserAcceptedAccountManagement(mNativeSigninManagerAndroid);
    }

    private void fetchAndApplyCloudPolicy(CoreAccountInfo account, final Runnable callback) {
        SigninManagerImplJni.get()
                .fetchAndApplyCloudPolicy(mNativeSigninManagerAndroid, account, callback);
    }

    private void stopApplyingCloudPolicy() {
        SigninManagerImplJni.get().stopApplyingCloudPolicy(mNativeSigninManagerAndroid);
    }

    private void disableSyncAndWipeData(final Runnable wipeDataCallback) {
        assumeNonNull(mSignOutState);
        Log.i(
                TAG,
                "Native signout complete, wiping data (user callback: %s)",
                mSignOutState.mDataWipeAction);

        maybeUpdateLegacyPrimaryAccountEmail();

        if (mSignOutState.mSignOutCallback != null) {
            mSignOutState.mSignOutCallback.preWipeData();
        }
        switch (mSignOutState.mDataWipeAction) {
            case SignOutState.DataWipeAction.WIPE_SIGNIN_DATA_ONLY:
                SigninManagerImplJni.get()
                        .wipeGoogleServiceWorkerCaches(
                                mNativeSigninManagerAndroid, wipeDataCallback);
                break;
            case SignOutState.DataWipeAction.WIPE_SYNC_DATA_ONLY:
                wipeSyncUserData(wipeDataCallback, DataWipeOption.WIPE_SYNC_DATA);
                break;
            case SignOutState.DataWipeAction.WIPE_ALL_PROFILE_DATA:
                wipeSyncUserData(wipeDataCallback, DataWipeOption.WIPE_ALL_PROFILE_DATA);
                break;
        }
    }

    /**
     * Contains all the state needed for signin. This forces signin flow state to be cleared
     * atomically, and all final fields to be set upon initialization.
     */
    private static class SignInState {
        private final @SigninAccessPoint Integer mAccessPoint;
        private final CoreAccountInfo mCoreAccountInfo;
        final @Nullable SignInCallback mCallback;

        /**
         * State for the sign-in flow that doesn't enable sync.
         *
         * @param accessPoint {@link SigninAccessPoint} that has initiated the sign-in.
         * @param coreAccountInfo The {@link CoreAccountInfo} to sign in with.
         * @param callback Called when the sign-in process finishes or is cancelled. Can be null.
         */
        static SignInState createForSignin(
                @SigninAccessPoint int accessPoint,
                CoreAccountInfo coreAccountInfo,
                @Nullable SignInCallback callback) {
            return new SignInState(accessPoint, coreAccountInfo, callback);
        }

        private SignInState(
                @SigninAccessPoint Integer accessPoint,
                CoreAccountInfo coreAccountInfo,
                @Nullable SignInCallback callback) {
            assert coreAccountInfo != null : "CoreAccountInfo must be set and valid to progress.";
            mAccessPoint = accessPoint;
            mCoreAccountInfo = coreAccountInfo;
            mCallback = callback;
        }

        /** Getter for the access point that initiated sign-in flow. */
        @SigninAccessPoint
        int getAccessPoint() {
            assert mAccessPoint != null : "Not going to enable sync - no access point!";
            return mAccessPoint;
        }
    }

    /**
     * Contains all the state needed for sign out. Like SignInState, this forces flow state to be
     * cleared atomically, and all final fields to be set upon initialization.
     */
    private static class SignOutState {
        @IntDef({
            DataWipeAction.WIPE_SIGNIN_DATA_ONLY,
            DataWipeAction.WIPE_SYNC_DATA_ONLY,
            DataWipeAction.WIPE_ALL_PROFILE_DATA
        })
        @Retention(RetentionPolicy.SOURCE)
        public @interface DataWipeAction {
            int WIPE_SIGNIN_DATA_ONLY = 0;
            int WIPE_SYNC_DATA_ONLY = 1;
            int WIPE_ALL_PROFILE_DATA = 2;
        }

        final @Nullable SignOutCallback mSignOutCallback;
        final @DataWipeAction int mDataWipeAction;

        /**
         * @param signOutCallback Hooks to call before/after data wiping phase of sign-out.
         * @param dataWipeAction Flag to wipe user data as requested by the user and enforced for
         *     managed users.
         */
        SignOutState(
                @Nullable SignOutCallback signOutCallback, @DataWipeAction int dataWipeAction) {
            this.mSignOutCallback = signOutCallback;
            this.mDataWipeAction = dataWipeAction;
        }
    }

    @NativeMethods
    interface Natives {
        boolean isForceSigninEnabled(long nativeSigninManagerAndroid);

        @JniType("std::string")
        String extractDomainName(@JniType("std::string") String email);

        void fetchAndApplyCloudPolicy(
                long nativeSigninManagerAndroid,
                @JniType("CoreAccountInfo") CoreAccountInfo account,
                @JniType("base::RepeatingClosure") Runnable callback);

        void stopApplyingCloudPolicy(long nativeSigninManagerAndroid);

        @Nullable String getManagementDomain(long nativeSigninManagerAndroid);

        void wipeProfileData(
                long nativeSigninManagerAndroid,
                @JniType("base::RepeatingClosure") Runnable callback);

        void wipeGoogleServiceWorkerCaches(
                long nativeSigninManagerAndroid,
                @JniType("base::RepeatingClosure") Runnable callback);

        void setUserAcceptedAccountManagement(
                long nativeSigninManagerAndroid, boolean acceptedAccountManagement);

        boolean getUserAcceptedAccountManagement(long nativeSigninManagerAndroid);
    }
}
