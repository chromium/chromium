// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.accounts.Account;

import androidx.annotation.IntDef;
import androidx.annotation.MainThread;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.browsing_data.BrowsingDataBridge;
import org.chromium.chrome.browser.browsing_data.BrowsingDataType;
import org.chromium.chrome.browser.browsing_data.TimePeriod;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninPreferencesManager;
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.components.signin.base.CoreAccountId;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.AccountInfoServiceProvider;
import org.chromium.components.signin.identitymanager.AccountTrackerService;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.identitymanager.IdentityMutator;
import org.chromium.components.signin.identitymanager.PrimaryAccountError;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.signin.metrics.SigninReason;
import org.chromium.components.signin.metrics.SignoutDelete;
import org.chromium.components.signin.metrics.SignoutReason;
import org.chromium.components.sync.SyncService;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.List;

/**
 * Android wrapper of the SigninManager which provides access from the Java layer.
 * <p/>
 * This class handles common paths during the sign-in and sign-out flows.
 * <p/>
 * Only usable from the UI thread as the native SigninManager requires its access to be in the
 * UI thread.
 * <p/>
 * See chrome/browser/android/signin/signin_manager_android.h for more details.
 */
class SigninManagerImpl implements IdentityManager.Observer, SigninManager {
    private static final String TAG = "SigninManager";
    private static final int[] SYNC_DATA_TYPES = {BrowsingDataType.HISTORY, BrowsingDataType.CACHE,
            BrowsingDataType.COOKIES, BrowsingDataType.PASSWORDS, BrowsingDataType.FORM_DATA};

    /**
     * Address of the native Signin Manager android.
     * This is not final, as destroy() updates this.
     */
    private long mNativeSigninManagerAndroid;
    private final AccountTrackerService mAccountTrackerService;
    private final IdentityManager mIdentityManager;
    private final IdentityMutator mIdentityMutator;
    private final SyncService mSyncService;
    private final ObserverList<SignInStateObserver> mSignInStateObservers = new ObserverList<>();
    private final List<Runnable> mCallbacksWaitingForPendingOperation = new ArrayList<>();
    private boolean mSigninAllowedByPolicy;

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
     * @param nativeSigninManagerAndroid A pointer to native's SigninManagerAndroid.
     */
    @CalledByNative
    @VisibleForTesting
    static SigninManager create(long nativeSigninManagerAndroid,
            AccountTrackerService accountTrackerService, IdentityManager identityManager,
            IdentityMutator identityMutator, SyncService syncService) {
        assert nativeSigninManagerAndroid != 0;
        assert accountTrackerService != null;
        assert identityManager != null;
        assert identityMutator != null;
        final SigninManagerImpl signinManager = new SigninManagerImpl(nativeSigninManagerAndroid,
                accountTrackerService, identityManager, identityMutator, syncService);

        identityManager.addObserver(signinManager);
        AccountInfoServiceProvider.init(identityManager, accountTrackerService);

        signinManager.reloadAllAccountsFromSystem(CoreAccountInfo.getIdFrom(
                identityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN)));
        return signinManager;
    }

    private SigninManagerImpl(long nativeSigninManagerAndroid,
            AccountTrackerService accountTrackerService, IdentityManager identityManager,
            IdentityMutator identityMutator, SyncService syncService) {
        ThreadUtils.assertOnUiThread();
        mNativeSigninManagerAndroid = nativeSigninManagerAndroid;
        mAccountTrackerService = accountTrackerService;
        mIdentityManager = identityManager;
        mIdentityMutator = identityMutator;
        mSyncService = syncService;

        mSigninAllowedByPolicy =
                SigninManagerImplJni.get().isSigninAllowedByPolicy(mNativeSigninManagerAndroid);
    }

    /**
     * Triggered during SigninManagerAndroidWrapper's KeyedService::Shutdown.
     * Drop references with external services and native.
     */
    @VisibleForTesting
    @CalledByNative
    void destroy() {
        AccountInfoServiceProvider.get().destroy();
        mIdentityManager.removeObserver(this);
        mNativeSigninManagerAndroid = 0;
    }

    /**
     * Extracts the domain name of a given account's email.
     */
    @Override
    public String extractDomainName(String accountEmail) {
        return SigninManagerImplJni.get().extractDomainName(accountEmail);
    };

    /**
     * Returns the IdentityManager used by SigninManager.
     */
    @Override
    public IdentityManager getIdentityManager() {
        return mIdentityManager;
    }

    /**
     * Returns true if sign in can be started now.
     */
    @Override
    public boolean isSigninAllowed() {
        return mSignInState == null && mSigninAllowedByPolicy
                && mIdentityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN) == null
                && isSigninSupported(/*requireUpdatedPlayServices=*/false);
    }

    /**
     * Returns true if sync opt in can be started now.
     */
    @Override
    public boolean isSyncOptInAllowed() {
        return mSignInState == null && mSigninAllowedByPolicy
                && mIdentityManager.getPrimaryAccountInfo(ConsentLevel.SYNC) == null
                && isSigninSupported(/*requireUpdatedPlayServices=*/false);
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
     * Returns true if signin is disabled by policy.
     */
    @Override
    public boolean isSigninDisabledByPolicy() {
        return !mSigninAllowedByPolicy;
    }

    /**
     * Returns whether the user can sign-in (maybe after an update to Google Play services).
     * @param requireUpdatedPlayServices Indicates whether an updated version of play services is
     *         required or not.
     */
    @Override
    public boolean isSigninSupported(boolean requireUpdatedPlayServices) {
        if (ApiCompatibilityUtils.isDemoUser()) {
            return false;
        }
        if (requireUpdatedPlayServices) {
            return ExternalAuthUtils.getInstance().canUseGooglePlayServices();
        }
        return !ExternalAuthUtils.getInstance().isGooglePlayServicesMissing(
                ContextUtils.getApplicationContext());
    }

    /**
     * @return Whether force sign-in is enabled by policy.
     */
    @Override
    public boolean isForceSigninEnabled() {
        return SigninManagerImplJni.get().isForceSigninEnabled(mNativeSigninManagerAndroid);
    }

    /**
     * Registers a SignInStateObserver to be notified when the user signs in or out of Chrome.
     */
    @Override
    public void addSignInStateObserver(SignInStateObserver observer) {
        mSignInStateObservers.addObserver(observer);
    }

    /**
     * Unregisters a SignInStateObserver to be notified when the user signs in or out of Chrome.
     */
    @Override
    public void removeSignInStateObserver(SignInStateObserver observer) {
        mSignInStateObservers.removeObserver(observer);
    }

    private void notifySignInAllowedChanged() {
        PostTask.postTask(TaskTraits.UI_DEFAULT, () -> {
            for (SignInStateObserver observer : mSignInStateObservers) {
                observer.onSignInAllowedChanged();
            }
        });
    }

    private void notifySignOutAllowedChanged() {
        PostTask.postTask(TaskTraits.UI_DEFAULT, () -> {
            for (SignInStateObserver observer : mSignInStateObservers) {
                observer.onSignOutAllowedChanged();
            }
        });
    }

    @Override
    public void signin(Account account, @SigninAccessPoint int accessPoint,
            @Nullable SignInCallback callback) {
        signinInternal(SignInState.createForSignin(accessPoint, account, callback));
    }

    @Override
    public void signin(
            CoreAccountInfo coreAccountInfo,
            @SigninAccessPoint int accessPoint,
            @Nullable SignInCallback callback) {
        // TODO(crbug.com/1462264): Replace Account with CoreAccountInfo.
        Account account = CoreAccountInfo.getAndroidAccountFrom(coreAccountInfo);
        signin(account, accessPoint, callback);
    }

    @Override
    public void signinAndEnableSync(Account account, @SigninAccessPoint int accessPoint,
            @Nullable SignInCallback callback) {
        signinInternal(SignInState.createForSigninAndEnableSync(accessPoint, account, callback));
    }

    @Override
    public void signinAndEnableSync(
            CoreAccountInfo coreAccountInfo,
            @SigninAccessPoint int accessPoint,
            @Nullable SignInCallback callback) {
        // TODO(crbug.com/1462264): Replace Account with CoreAccountInfo.
        Account account = CoreAccountInfo.getAndroidAccountFrom(coreAccountInfo);
        signinAndEnableSync(account, accessPoint, callback);
    }

    private void signinInternal(SignInState signInState) {
        assert isSyncOptInAllowed()
            : String.format("Sign-in isn't allowed!\n"
                            + "  mSignInState: %s\n"
                            + "  mSigninAllowedByPolicy: %s\n"
                            + "  Primary sync account: %s",
                    mSignInState, mSigninAllowedByPolicy,
                    mIdentityManager.getPrimaryAccountInfo(ConsentLevel.SYNC));
        assert signInState != null : "SigninState shouldn't be null!";
        assert signInState.mCoreAccountInfo == null : "mCoreAccountInfo shouldn't be set!";

        // The mSignInState must be updated prior to the async processing below, as this indicates
        // that a signin operation is in progress and prevents other sign in operations from being
        // started until this one completes (see {@link isOperationInProgress()}).
        mSignInState = signInState;
        signInState = null;

        Log.i(TAG, "Signin starts (enabling sync: %b).", mSignInState.shouldTurnSyncOn());
        AccountInfoServiceProvider.get()
                .getAccountInfoByEmail(mSignInState.mAccount.name)
                .then(accountInfo -> {
                    mSignInState.mCoreAccountInfo = accountInfo;
                    notifySignInAllowedChanged();

                    if (mSignInState.shouldTurnSyncOn()) {
                        Log.d(TAG, "Checking if account has policy management enabled");
                        fetchAndApplyCloudPolicy(mSignInState.mCoreAccountInfo,
                                this::finishSignInAfterPolicyEnforced);
                    } else {
                        // Sign-in without sync doesn't enforce enterprise policy, so skip that
                        // step.
                        finishSignInAfterPolicyEnforced();
                    }
                });
    }

    /**
     * Finishes the sign-in flow. If the user is managed, the policy should be fetched and enforced
     * before calling this method.
     */
    @VisibleForTesting
    void finishSignInAfterPolicyEnforced() {
        assert mSignInState != null : "SigninState shouldn't be null!";
        assert !mIdentityManager.hasPrimaryAccount(ConsentLevel.SYNC)
            : "The user should not be already signed in";

        // Setting the primary account triggers observers which query accounts from IdentityManager.
        // Reloading before setting the primary ensures they don't get an empty list of accounts.
        reloadAllAccountsFromSystem(mSignInState.mCoreAccountInfo.getId());

        @ConsentLevel
        int consentLevel =
                mSignInState.shouldTurnSyncOn() ? ConsentLevel.SYNC : ConsentLevel.SIGNIN;
        @PrimaryAccountError
        int primaryAccountError = mIdentityMutator.setPrimaryAccount(
                mSignInState.mCoreAccountInfo.getId(), consentLevel, mSignInState.getAccessPoint());
        if (primaryAccountError != PrimaryAccountError.NO_ERROR) {
            Log.w(TAG, "SetPrimaryAccountError in IdentityManager: %d, aborting signin",
                    primaryAccountError);
            abortSignIn();
            return;
        }

        if (mSignInState.shouldTurnSyncOn()) {
            // TODO(https://crbug.com/1091858): Remove this after migrating the legacy code that
            // uses the sync account before the native is loaded.
            SigninPreferencesManager.getInstance().setLegacySyncAccountEmail(
                    mSignInState.mCoreAccountInfo.getEmail());

            mSyncService.setSyncRequested();

            RecordUserAction.record("Signin_Signin_Succeed");
            RecordHistogram.recordEnumeratedHistogram("Signin.SigninCompletedAccessPoint",
                    mSignInState.getAccessPoint(), SigninAccessPoint.MAX);
            RecordHistogram.recordEnumeratedHistogram("Signin.SigninReason",
                    SigninReason.SIGNIN_PRIMARY_ACCOUNT, SigninReason.MAX_VALUE + 1);
        }

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
     * Whether an operation is in progress for which we should wait before
     * scheduling users of {@link runAfterOperationInProgress}.
     */
    private boolean isOperationInProgress() {
        ThreadUtils.assertOnUiThread();
        return mSignInState != null || mSignOutState != null || mWipeUserDataInProgress;
    }

    /**
     * Maybe notifies any callbacks registered via runAfterOperationInProgress().
     *
     * The callbacks are notified in FIFO order, and each callback is only notified if there is no
     * outstanding work (either work which was outstanding at the time the callback was added, or
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

    /**
     * Initialize SignOutState, and call identity mutator to revoke the sync consent.  Processing
     * will complete asynchronously in the {@link #onPrimaryAccountChanged()} callback.
     */
    @Override
    public void revokeSyncConsent(@SignoutReason int signoutSource, SignOutCallback signOutCallback,
            boolean forceWipeUserData) {
        // Only one signOut at a time!
        assert mSignOutState == null;
        // User must be syncing.
        assert mIdentityManager.hasPrimaryAccount(ConsentLevel.SYNC);

        // Grab the management domain before nativeSignOut() potentially clears it.
        String managementDomain = getManagementDomain();

        // We wipe sync data only, as wiping the profile data would also trigger sign-out.
        mSignOutState = new SignOutState(signOutCallback,
                (forceWipeUserData || managementDomain != null)
                        ? SignOutState.DataWipeAction.WIPE_SYNC_DATA_ONLY
                        : SignOutState.DataWipeAction.WIPE_SIGNIN_DATA_ONLY);
        Log.i(TAG, "Revoking sync consent, dataWipeAction: %d",
                (forceWipeUserData || managementDomain != null)
                        ? SignOutState.DataWipeAction.WIPE_SYNC_DATA_ONLY
                        : SignOutState.DataWipeAction.WIPE_SIGNIN_DATA_ONLY);

        mIdentityMutator.revokeSyncConsent(signoutSource,
                // Always use IGNORE_METRIC as Chrome Android has just a single-profile which is
                // never deleted.
                SignoutDelete.IGNORE_METRIC);

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
    public void signOut(@SignoutReason int signoutSource, SignOutCallback signOutCallback,
            boolean forceWipeUserData) {
        // Only one signOut at a time!
        assert mSignOutState == null;

        // Grab the management domain before nativeSignOut() potentially clears it.
        String managementDomain = getManagementDomain();
        mSignOutState = new SignOutState(signOutCallback,
                (forceWipeUserData || managementDomain != null)
                        ? SignOutState.DataWipeAction.WIPE_ALL_PROFILE_DATA
                        : SignOutState.DataWipeAction.WIPE_SIGNIN_DATA_ONLY);
        Log.i(TAG, "Signing out, dataWipeAction: %d",
                (forceWipeUserData || managementDomain != null)
                        ? SignOutState.DataWipeAction.WIPE_ALL_PROFILE_DATA
                        : SignOutState.DataWipeAction.WIPE_SIGNIN_DATA_ONLY);

        mIdentityMutator.clearPrimaryAccount(signoutSource,
                // Always use IGNORE_METRIC for the profile deletion argument. Chrome
                // Android has just a single-profile which is never deleted upon
                // sign-out.
                SignoutDelete.IGNORE_METRIC);

        notifySignOutAllowedChanged();
        disableSyncAndWipeData(this::finishSignOut);
    }

    /**
     * Returns the management domain if the signed in account is managed, otherwise returns null.
     */
    @Override
    public String getManagementDomain() {
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

        RecordHistogram.recordEnumeratedHistogram("Signin.SigninAbortedAccessPoint",
                signInState.getAccessPoint(), SigninAccessPoint.MAX);

        if (signInState.mCallback != null) {
            signInState.mCallback.onSignInAborted();
        }

        stopApplyingCloudPolicy();

        Log.d(TAG, "Signin flow aborted.");
        notifySignInAllowedChanged();
    }

    @VisibleForTesting
    void finishSignOut() {
        // Should be set at start of sign-out flow.
        assert mSignOutState != null;

        if (ChromeFeatureList.isEnabled(
                    ChromeFeatureList.SYNC_ANDROID_LIMIT_NTP_PROMO_IMPRESSIONS)) {
            // After sign-out, reset the Sync promo show count, so the user will see Sync promos
            // again.
            ChromeSharedPreferences.getInstance().writeInt(
                    ChromePreferenceKeys.SYNC_PROMO_SHOW_COUNT.createKey(
                            SigninPreferencesManager.SyncPromoAccessPointId.NTP),
                    0);
        }
        SignOutCallback signOutCallback = mSignOutState.mSignOutCallback;
        mSignOutState = null;

        if (signOutCallback != null) signOutCallback.signOutComplete();
        notifyCallbacksWaitingForOperation();

        for (SignInStateObserver observer : mSignInStateObservers) {
            observer.onSignedOut();
        }
    }

    @CalledByNative
    private void onSigninAllowedByPolicyChanged(boolean newSigninAllowedByPolicy) {
        mSigninAllowedByPolicy = newSigninAllowedByPolicy;
        notifySignInAllowedChanged();
    }

    /**
     * Verifies if the account is managed. Callback may be called either
     * synchronously or asynchronously depending on the availability of the
     * result.
     * @param email An email of the account.
     * @param callback The callback that will receive true if the account is managed, false
     *                 otherwise.
     */
    // TODO(crbug.com/1002408) Update API to use CoreAccountInfo instead of email
    @Override
    public void isAccountManaged(String email, final Callback<Boolean> callback) {
        assert email != null;
        CoreAccountInfo account = mIdentityManager.findExtendedAccountInfoByEmailAddress(email);
        assert account != null;
        SigninManagerImplJni.get().isAccountManaged(mNativeSigninManagerAndroid, account, callback);
    }

    @Override
    public void reloadAllAccountsFromSystem(@Nullable CoreAccountId primaryAccountId) {
        mIdentityMutator.reloadAllAccountsFromSystemWithPrimaryAccount(primaryAccountId);
    }

    /** Called when account seeding is complete. */
    public void onAccountsSeeded(List<CoreAccountInfo> coreAccountInfos) {
        // TODO(crbug/1491005): Call this right after seeding.
        mIdentityManager.refreshAccountInfoIfStale(coreAccountInfos);
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
                SigninManagerImplJni.get().wipeProfileData(mNativeSigninManagerAndroid, () -> {
                    mWipeUserDataInProgress = false;
                    wipeDataCallback.run();
                    notifyCallbacksWaitingForOperation();
                });
                break;
        }
    }

    // TODO(crbug.com/1272911): this function and disableSyncAndWipeData() have very similar
    // functionality, but with different implementations.  Consider merging them.
    // TODO(crbug.com/1272911): add test coverage for this function (including its effect on
    // notifyCallbacksWaitingForOperation()), after resolving the TODO above.
    private void wipeSyncUserDataOnly(Runnable wipeDataCallback) {
        final BookmarkModel model =
                BookmarkModel.getForProfile(Profile.getLastUsedRegularProfile());
        model.finishLoadingBookmarkModel(new Runnable() {
            @Override
            public void run() {
                model.removeAllUserBookmarks();
                BrowsingDataBridge.getInstance().clearBrowsingData(
                        new BrowsingDataBridge.OnClearBrowsingDataListener() {
                            @Override
                            public void onBrowsingDataCleared() {
                                assert mWipeUserDataInProgress;
                                mWipeUserDataInProgress = false;
                                wipeDataCallback.run();
                                notifyCallbacksWaitingForOperation();
                            }
                        },
                        SYNC_DATA_TYPES, TimePeriod.ALL_TIME);
            }
        });
    }

    private boolean isGooglePlayServicesPresent() {
        return !ExternalAuthUtils.getInstance().isGooglePlayServicesMissing(
                ContextUtils.getApplicationContext());
    }

    private void fetchAndApplyCloudPolicy(CoreAccountInfo account, final Runnable callback) {
        SigninManagerImplJni.get().fetchAndApplyCloudPolicy(
                mNativeSigninManagerAndroid, account, callback);
    }

    private void stopApplyingCloudPolicy() {
        SigninManagerImplJni.get().stopApplyingCloudPolicy(mNativeSigninManagerAndroid);
    }

    private void disableSyncAndWipeData(final Runnable wipeDataCallback) {
        Log.i(TAG, "Native signout complete, wiping data (user callback: %s)",
                mSignOutState.mDataWipeAction);

        // TODO(https://crbug.com/1091858): Remove this after migrating the legacy code that
        //                                  uses the sync account before the native is
        //                                  loaded.
        SigninPreferencesManager.getInstance().setLegacySyncAccountEmail(null);

        if (mSignOutState.mSignOutCallback != null) {
            mSignOutState.mSignOutCallback.preWipeData();
        }
        switch (mSignOutState.mDataWipeAction) {
            case SignOutState.DataWipeAction.WIPE_SIGNIN_DATA_ONLY:
                SigninManagerImplJni.get().wipeGoogleServiceWorkerCaches(
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
     * Contains all the state needed for signin. This forces signin flow state to be
     * cleared atomically, and all final fields to be set upon initialization.
     */
    private static class SignInState {
        private final @SigninAccessPoint Integer mAccessPoint;
        private final boolean mShouldTurnSyncOn;
        final SignInCallback mCallback;

        /**
         * Contains the basic account information, which is available immediately at the start of
         * the sign-in operation.
         *
         */
        final Account mAccount;

        /**
         * Contains the full Core account info, which can be retrieved only once account seeding is
         * complete
         */
        @Nullable
        CoreAccountInfo mCoreAccountInfo;

        /**
         * State for the sign-in flow that doesn't enable sync.
         *
         * @param accessPoint {@link SigninAccessPoint} that has initiated the sign-in.
         * @param account The account to sign in to.
         * @param callback Called when the sign-in process finishes or is cancelled. Can be null.
         */
        static SignInState createForSignin(@SigninAccessPoint int accessPoint, Account account,
                @Nullable SignInCallback callback) {
            return new SignInState(accessPoint, account, callback, false);
        }

        /**
         * State for the sync consent flow.
         *
         * @param accessPoint {@link SigninAccessPoint} that has initiated the sign-in.
         * @param account The account to sign in to.
         * @param callback Called when the sign-in process finishes or is cancelled. Can be null.
         */
        static SignInState createForSigninAndEnableSync(@SigninAccessPoint int accessPoint,
                Account account, @Nullable SignInCallback callback) {
            return new SignInState(accessPoint, account, callback, true);
        }

        private SignInState(@SigninAccessPoint Integer accessPoint, Account account,
                @Nullable SignInCallback callback, boolean shouldTurnSyncOn) {
            assert account != null : "Account must be set and valid to progress.";
            mAccessPoint = accessPoint;
            mAccount = account;
            mCallback = callback;
            mShouldTurnSyncOn = shouldTurnSyncOn;
        }

        /**
         * Getter for the access point that initiated sync consent flow. Shouldn't be called if
         * {@link #shouldTurnSyncOn()} is false.
         */
        @SigninAccessPoint
        int getAccessPoint() {
            assert mAccessPoint != null : "Not going to enable sync - no access point!";
            return mAccessPoint;
        }

        /**
         * Whether this sign-in flow should also turn on sync.
         */
        boolean shouldTurnSyncOn() {
            return mShouldTurnSyncOn;
        }
    }

    /**
     * Contains all the state needed for sign out. Like SignInState, this forces flow state to be
     * cleared atomically, and all final fields to be set upon initialization.
     */
    private static class SignOutState {
        @IntDef({DataWipeAction.WIPE_SIGNIN_DATA_ONLY, DataWipeAction.WIPE_SYNC_DATA_ONLY,
                DataWipeAction.WIPE_ALL_PROFILE_DATA})
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
         * @param shouldWipeUserData Flag to wipe user data as requested by the user and enforced
         *         for managed users.
         */
        SignOutState(
                @Nullable SignOutCallback signOutCallback, @DataWipeAction int dataWipeAction) {
            this.mSignOutCallback = signOutCallback;
            this.mDataWipeAction = dataWipeAction;
        }
    }

    @NativeMethods
    interface Natives {
        boolean isSigninAllowedByPolicy(long nativeSigninManagerAndroid);

        boolean isForceSigninEnabled(long nativeSigninManagerAndroid);

        String extractDomainName(String email);

        void fetchAndApplyCloudPolicy(
                long nativeSigninManagerAndroid, CoreAccountInfo account, Runnable callback);

        void stopApplyingCloudPolicy(long nativeSigninManagerAndroid);

        void isAccountManaged(long nativeSigninManagerAndroid, CoreAccountInfo account,
                Callback<Boolean> callback);

        String getManagementDomain(long nativeSigninManagerAndroid);

        void wipeProfileData(long nativeSigninManagerAndroid, Runnable callback);

        void wipeGoogleServiceWorkerCaches(long nativeSigninManagerAndroid, Runnable callback);
    }
}
