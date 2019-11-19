// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.accounts.Account;

import androidx.annotation.MainThread;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.browser.externalauth.ExternalAuthUtils;
import org.chromium.components.signin.AccountIdProvider;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountTrackerService;
import org.chromium.components.signin.ChromeSigninController;
import org.chromium.components.signin.identitymanager.ClearAccountsAction;
import org.chromium.components.signin.identitymanager.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.identitymanager.IdentityMutator;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.signin.metrics.SigninReason;
import org.chromium.components.signin.metrics.SignoutDelete;
import org.chromium.components.signin.metrics.SignoutReason;
import org.chromium.components.sync.AndroidSyncSettings;
import org.chromium.content_public.browser.UiThreadTaskTraits;

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
 * See chrome/browser/signin/signin_manager_android.h for more details.
 */
public class SigninManager
        implements AccountTrackerService.OnSystemAccountsSeededListener, IdentityManager.Observer {
    private static final String TAG = "SigninManager";

    /**
     * A SignInStateObserver is notified when the user signs in to or out of Chrome.
     */
    public interface SignInStateObserver {
        /**
         * Invoked when the user has signed in to Chrome.
         */
        void onSignedIn();

        /**
         * Invoked when the user has signed out of Chrome.
         */
        void onSignedOut();
    }

    /**
     * SignInAllowedObservers will be notified once signing-in becomes allowed or disallowed.
     */
    public interface SignInAllowedObserver {
        /**
         * Invoked once all startup checks are done and signing-in becomes allowed, or disallowed.
         */
        void onSignInAllowedChanged();
    }

    /**
     * Callbacks for the sign-in flow.
     */
    public interface SignInCallback {
        /**
         * Invoked after sign-in is completed successfully.
         */
        void onSignInComplete();

        /**
         * Invoked if the sign-in processes does not complete for any reason.
         */
        void onSignInAborted();
    }

    /**
     * Callbacks for the sign-out flow.
     */
    public interface SignOutCallback {
        /**
         * Called before the data wiping is started.
         */
        default void preWipeData() {}

        /**
         * Called after the data is wiped.
         */
        void signOutComplete();
    }

    /**
     * Contains all the state needed for signin. This forces signin flow state to be
     * cleared atomically, and all final fields to be set upon initialization.
     */
    private static class SignInState {
        final Account mAccount;
        final SignInCallback mCallback;

        /**
         * If the system accounts need to be seeded, the sign in flow will block for that to occur.
         * This boolean should be set to true during that time and then reset back to false
         * afterwards. This allows the manager to know if it should progress the flow when the
         * account tracker broadcasts updates.
         */
        boolean mBlockedOnAccountSeeding;

        /**
         * Contains the full Core account info, which can be retrieved only once account seeding is
         * complete
         */
        CoreAccountInfo mCoreAccountInfo;

        /**
         * @param account The account to sign in to.
         * @param callback Called when the sign-in process finishes or is cancelled. Can be null.
         */
        SignInState(Account account, @Nullable SignInCallback callback) {
            this.mAccount = account;
            this.mCallback = callback;
        }
    }

    /**
     * Contains all the state needed for sign out. Like SignInState, this forces flow state to be
     * cleared atomically, and all final fields to be set upon initialization.
     */
    private static class SignOutState {
        final @Nullable SignOutCallback mSignOutCallback;
        final boolean mShouldWipeUserData;

        /**
         * @param signOutCallback Hooks to call before/after data wiping phase of sign-out.
         * @param shouldWipeUserData Flag to wipe user data as requested by the user and enforced
         *         for managed users.
         */
        SignOutState(@Nullable SignOutCallback signOutCallback, boolean shouldWipeUserData) {
            this.mSignOutCallback = signOutCallback;
            this.mShouldWipeUserData = shouldWipeUserData;
        }
    }

    private static int sSignInAccessPoint = SigninAccessPoint.UNKNOWN;

    /**
     * Address of the native Signin Manager android.
     * This is not final, as destroy() updates this.
     */
    private long mNativeSigninManagerAndroid;
    private final AccountTrackerService mAccountTrackerService;
    private final IdentityManager mIdentityManager;
    private final IdentityMutator mIdentityMutator;
    private final AndroidSyncSettings mAndroidSyncSettings;
    private final ObserverList<SignInStateObserver> mSignInStateObservers = new ObserverList<>();
    private final ObserverList<SignInAllowedObserver> mSignInAllowedObservers =
            new ObserverList<>();
    private List<Runnable> mCallbacksWaitingForPendingOperation = new ArrayList<>();
    private boolean mSigninAllowedByPolicy;

    /**
     * Tracks whether the First Run check has been completed.
     *
     * A new sign-in can not be started while this is pending, to prevent the
     * pending check from eventually starting a 2nd sign-in.
     */
    private boolean mFirstRunCheckIsPending = true;

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

    /**
     * Called by native to create an instance of SigninManager.
     * @param nativeSigninManagerAndroid A pointer to native's SigninManagerAndroid.
     */
    @CalledByNative
    private static SigninManager create(long nativeSigninManagerAndroid,
            AccountTrackerService accountTrackerService, IdentityManager identityManager,
            IdentityMutator identityMutator) {
        assert nativeSigninManagerAndroid != 0;
        assert accountTrackerService != null;
        assert identityManager != null;
        assert identityMutator != null;
        return new SigninManager(nativeSigninManagerAndroid, accountTrackerService, identityManager,
                identityMutator, AndroidSyncSettings.get());
    }

    @VisibleForTesting
    SigninManager(long nativeSigninManagerAndroid, AccountTrackerService accountTrackerService,
            IdentityManager identityManager, IdentityMutator identityMutator,
            AndroidSyncSettings androidSyncSettings) {
        ThreadUtils.assertOnUiThread();
        assert androidSyncSettings != null;
        mNativeSigninManagerAndroid = nativeSigninManagerAndroid;
        mAccountTrackerService = accountTrackerService;
        mIdentityManager = identityManager;
        mIdentityMutator = identityMutator;
        mAndroidSyncSettings = androidSyncSettings;

        mSigninAllowedByPolicy =
                SigninManagerJni.get().isSigninAllowedByPolicy(mNativeSigninManagerAndroid);

        mAccountTrackerService.addSystemAccountsSeededListener(this);
        mIdentityManager.addObserver(this);

        reloadAllAccountsFromSystem();
    }

    /**
     * Triggered during SigninManagerAndroidWrapper's KeyedService::Shutdown.
     * Drop references with external services and native.
     */
    @CalledByNative
    public void destroy() {
        mIdentityManager.removeObserver(this);
        mAccountTrackerService.removeSystemAccountsSeededListener(this);
        mNativeSigninManagerAndroid = 0;
    }

    /**
    * Log the access point when the user see the view of choosing account to sign in.
    * @param accessPoint the enum value of AccessPoint defined in signin_metrics.h.
    */
    public static void logSigninStartAccessPoint(int accessPoint) {
        RecordHistogram.recordEnumeratedHistogram(
                "Signin.SigninStartedAccessPoint", accessPoint, SigninAccessPoint.MAX);
        sSignInAccessPoint = accessPoint;
    }

    private void logSigninCompleteAccessPoint() {
        RecordHistogram.recordEnumeratedHistogram(
                "Signin.SigninCompletedAccessPoint", sSignInAccessPoint, SigninAccessPoint.MAX);
        sSignInAccessPoint = SigninAccessPoint.UNKNOWN;
    }

    /**
     * Returns the IdentityManager used by SigninManager.
     */
    public IdentityManager getIdentityManager() {
        return mIdentityManager;
    }

    /**
     * Notifies the SigninManager that the First Run check has completed.
     *
     * The user will be allowed to sign-in once this is signaled.
     */
    public void onFirstRunCheckDone() {
        mFirstRunCheckIsPending = false;

        if (isSignInAllowed()) {
            notifySignInAllowedChanged();
        }
    }

    /**
     * Returns true if signin can be started now.
     */
    public boolean isSignInAllowed() {
        return !mFirstRunCheckIsPending && mSignInState == null && mSigninAllowedByPolicy
                && ChromeSigninController.get().getSignedInUser() == null && isSigninSupported();
    }

    /**
     * Returns true if signin is disabled by policy.
     */
    public boolean isSigninDisabledByPolicy() {
        return !mSigninAllowedByPolicy;
    }

    /**
     * @return Whether true if the current user is not demo user and the user has a reasonable
     *         Google Play Services installed.
     */
    public boolean isSigninSupported() {
        return !ApiCompatibilityUtils.isDemoUser() && isGooglePlayServicesPresent()
                && !SigninManagerJni.get().isMobileIdentityConsistencyEnabled();
    }

    /**
     * @return Whether force sign-in is enabled by policy.
     */
    public boolean isForceSigninEnabled() {
        return SigninManagerJni.get().isForceSigninEnabled(mNativeSigninManagerAndroid);
    }

    /**
     * Registers a SignInStateObserver to be notified when the user signs in or out of Chrome.
     */
    public void addSignInStateObserver(SignInStateObserver observer) {
        mSignInStateObservers.addObserver(observer);
    }

    /**
     * Unregisters a SignInStateObserver to be notified when the user signs in or out of Chrome.
     */
    public void removeSignInStateObserver(SignInStateObserver observer) {
        mSignInStateObservers.removeObserver(observer);
    }

    public void addSignInAllowedObserver(SignInAllowedObserver observer) {
        mSignInAllowedObservers.addObserver(observer);
    }

    public void removeSignInAllowedObserver(SignInAllowedObserver observer) {
        mSignInAllowedObservers.removeObserver(observer);
    }

    private void notifySignInAllowedChanged() {
        PostTask.postTask(UiThreadTaskTraits.DEFAULT, () -> {
            for (SignInAllowedObserver observer : mSignInAllowedObservers) {
                observer.onSignInAllowedChanged();
            }
        });
    }

    /**
    * Continue pending sign in after system accounts have been seeded into AccountTrackerService.
    */
    @Override
    public void onSystemAccountsSeedingComplete() {
        if (mSignInState != null && mSignInState.mBlockedOnAccountSeeding) {
            mSignInState.mBlockedOnAccountSeeding = false;
            progressSignInFlowCheckPolicy();
        }
    }

    /**
    * Clear pending sign in when system accounts in AccountTrackerService were refreshed.
    */
    @Override
    public void onSystemAccountsChanged() {
        if (mSignInState != null) {
            abortSignIn();
        }
    }

    /**
     * Starts the sign-in flow, and executes the callback when finished.
     *
     * If an activity is provided, it is considered an "interactive" sign-in and the user can be
     * prompted to confirm various aspects of sign-in using dialogs inside the activity.
     * The sign-in flow goes through the following steps:
     *
     *   - Wait for AccountTrackerService to be seeded.
     *   - If interactive, confirm the account change with the user.
     *   - Wait for policy to be checked for the account.
     *   - If interactive and the account is managed, warn the user.
     *   - If managed, wait for the policy to be fetched.
     *   - Complete sign-in with the native SigninManager and kick off token requests.
     *   - Call the callback if provided.
     *
     * @param account The account to sign in to.
     * @param callback Optional callback for when the sign-in process is finished.
     */
    // TODO(crbug.com/1002056) SigninManager.Signin should use CoreAccountInfo as a parameter.
    public void signIn(Account account, @Nullable SignInCallback callback) {
        if (account == null) {
            Log.w(TAG, "Ignoring sign-in request due to null account.");
            if (callback != null) callback.onSignInAborted();
            return;
        }

        if (mSignInState != null) {
            Log.w(TAG, "Ignoring sign-in request as another sign-in request is pending.");
            if (callback != null) callback.onSignInAborted();
            return;
        }

        if (mFirstRunCheckIsPending) {
            Log.w(TAG, "Ignoring sign-in request until the First Run check completes.");
            if (callback != null) callback.onSignInAborted();
            return;
        }

        mSignInState = new SignInState(account, callback);
        notifySignInAllowedChanged();

        progressSignInFlowSeedSystemAccounts();
    }

    /**
     * Same as above but retrieves the Account object for the given accountName.
     */
    // TODO(crbug.com/1002056) SigninManager.Signin should use CoreAccountInfo as a parameter.
    public void signIn(String accountName, @Nullable final SignInCallback callback) {
        AccountManagerFacade.get().getAccountFromName(
                accountName, account -> signIn(account, callback));
    }

    private void progressSignInFlowSeedSystemAccounts() {
        if (mAccountTrackerService.checkAndSeedSystemAccounts()) {
            progressSignInFlowCheckPolicy();
        } else if (AccountIdProvider.getInstance().canBeUsed()) {
            mSignInState.mBlockedOnAccountSeeding = true;
        } else {
            Log.w(TAG, "Cancelling the sign-in process as Google Play services is unavailable");
            abortSignIn();
        }
    }

    /**
     * Continues the signin flow by checking if there is a policy that the account is subject to.
     */
    private void progressSignInFlowCheckPolicy() {
        if (mSignInState == null) {
            Log.w(TAG, "Ignoring sign in progress request as no pending sign in.");
            return;
        }
        // TODO(crbug.com/1002056) When changing SignIn signature to use CoreAccountInfo, change the
        // following line to use it instead of retrieving from IdentityManager.
        mSignInState.mCoreAccountInfo =
                mIdentityManager.findExtendedAccountInfoForAccountWithRefreshTokenByEmailAddress(
                        mSignInState.mAccount.name);

        // CoreAccountInfo must be set and valid to progress
        assert mSignInState.mCoreAccountInfo != null;

        Log.d(TAG, "Checking if account has policy management enabled");
        fetchAndApplyCloudPolicy(
                mSignInState.mCoreAccountInfo, this::finishSignInAfterPolicyEnforced);
    }

    /**
     * Finishes the sign-in flow. If the user is managed, the policy should be fetched and enforced
     * before calling this method.
     */
    @VisibleForTesting
    void finishSignInAfterPolicyEnforced() {
        // This method should be called at most once per sign-in flow.
        assert mSignInState != null && mSignInState.mCoreAccountInfo != null;

        // The user should not be already signed in
        assert !mIdentityManager.hasPrimaryAccount();

        if (!mIdentityMutator.setPrimaryAccount(mSignInState.mCoreAccountInfo.getId())) {
            Log.w(TAG, "Failed to set the PrimaryAccount in IdentityManager, aborting signin");
            abortSignIn();
            return;
        }

        // Cache the signed-in account name. This must be done after the native call, otherwise
        // sync tries to start without being signed in natively and crashes.
        ChromeSigninController.get().setSignedInAccountName(
                mSignInState.mCoreAccountInfo.getName());
        enableSync(mSignInState.mCoreAccountInfo.getAccount());

        if (mSignInState.mCallback != null) {
            mSignInState.mCallback.onSignInComplete();
        }

        // Trigger token requests via identity mutator.
        reloadAllAccountsFromSystem();

        RecordUserAction.record("Signin_Signin_Succeed");
        logSigninCompleteAccessPoint();
        // Log signin in reason as defined in signin_metrics.h. Right now only
        // SIGNIN_PRIMARY_ACCOUNT available on Android.
        RecordHistogram.recordEnumeratedHistogram(
                "Signin.SigninReason", SigninReason.SIGNIN_PRIMARY_ACCOUNT, SigninReason.MAX);

        Log.d(TAG, "Signin completed.");
        mSignInState = null;
        notifyCallbacksWaitingForOperation();
        notifySignInAllowedChanged();

        for (SignInStateObserver observer : mSignInStateObservers) {
            observer.onSignedIn();
        }
    }

    /**
     * Implements {@link IdentityManager.Observer}: take action when primary account is set.
     * Simply verify that the request is ongoing (mSignInState != null), as only SigninManager
     * should update IdentityManager. This is triggered by the call to
     * IdentityMutator.setPrimaryAccount
     */
    @VisibleForTesting
    @Override
    public void onPrimaryAccountSet(CoreAccountInfo account) {
        assert mSignInState != null;
    }

    /**
     * Returns true if a sign-in or sign-out operation is in progress. See also
     * {@link #runAfterOperationInProgress}.
     */
    @MainThread
    public boolean isOperationInProgress() {
        ThreadUtils.assertOnUiThread();
        return mSignInState != null || mSignOutState != null;
    }

    /**
     * Schedules the runnable to be invoked after currently ongoing a sign-in or sign-out operation
     * is finished. If there's no operation is progress, posts the callback to the UI thread right
     * away. See also {@link #isOperationInProgress}.
     */
    @MainThread
    public void runAfterOperationInProgress(Runnable runnable) {
        ThreadUtils.assertOnUiThread();
        if (isOperationInProgress()) {
            mCallbacksWaitingForPendingOperation.add(runnable);
            return;
        }
        PostTask.postTask(UiThreadTaskTraits.DEFAULT, runnable);
    }

    private void notifyCallbacksWaitingForOperation() {
        ThreadUtils.assertOnUiThread();
        for (Runnable callback : mCallbacksWaitingForPendingOperation) {
            PostTask.postTask(UiThreadTaskTraits.DEFAULT, callback);
        }
        mCallbacksWaitingForPendingOperation.clear();
    }

    /**
     * Invokes signOut with no callback.
     */
    public void signOut(@SignoutReason int signoutSource) {
        signOut(signoutSource, null, false);
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
    public void signOut(@SignoutReason int signoutSource, SignOutCallback signOutCallback,
            boolean forceWipeUserData) {
        // Only one signOut at a time!
        assert mSignOutState == null;

        // Grab the management domain before nativeSignOut() potentially clears it.
        String managementDomain = getManagementDomain();
        mSignOutState =
                new SignOutState(signOutCallback, forceWipeUserData || managementDomain != null);
        Log.d(TAG, "Signing out, management domain: " + managementDomain);

        // User data will be wiped in disableSyncAndWipeData(), called from
        // onPrimaryAccountcleared().
        mIdentityMutator.clearPrimaryAccount(ClearAccountsAction.DEFAULT, signoutSource,
                // Always use IGNORE_METRIC for the profile deletion argument. Chrome
                // Android has just a single-profile which is never deleted upon
                // sign-out.
                SignoutDelete.IGNORE_METRIC);
    }

    /**
     * Returns the management domain if the signed in account is managed, otherwise returns null.
     */
    public String getManagementDomain() {
        return SigninManagerJni.get().getManagementDomain(mNativeSigninManagerAndroid);
    }

    /**
     * Reloads accounts from system within IdentityManager.
     */
    void reloadAllAccountsFromSystem() {
        mIdentityMutator.reloadAllAccountsFromSystemWithPrimaryAccount(
                mIdentityManager.getPrimaryAccountId());
    }

    /**
     * Aborts the current sign in.
     *
     * Package protected to allow dialog fragments to abort the signin flow.
     */
    void abortSignIn() {
        // Ensure this function can only run once per signin flow.
        SignInState signInState = mSignInState;
        assert signInState != null;
        mSignInState = null;
        notifyCallbacksWaitingForOperation();

        if (signInState.mCallback != null) {
            signInState.mCallback.onSignInAborted();
        }

        stopApplyingCloudPolicy();

        Log.d(TAG, "Signin flow aborted.");
        notifySignInAllowedChanged();
    }

    @Override
    public void onPrimaryAccountCleared(CoreAccountInfo account) {
        if (mSignOutState == null) {
            // mSignOutState can only be null when the sign out is triggered by
            // native (since otherwise SigninManager.signOut would have created
            // it). As sign out from native can only happen from policy code,
            // the account is managed and the user data must be wiped.
            mSignOutState = new SignOutState(null, true);
        }

        Log.d(TAG, "On native signout, wipe user data: " + mSignOutState.mShouldWipeUserData);

        // Native sign-out must happen before resetting the account so data is deleted correctly.
        // http://crbug.com/589028
        ChromeSigninController.get().setSignedInAccountName(null);
        if (mSignOutState.mSignOutCallback != null) mSignOutState.mSignOutCallback.preWipeData();
        disableSyncAndWipeData(mSignOutState.mShouldWipeUserData, this::finishSignOut);
        mAccountTrackerService.invalidateAccountSeedStatus(true);
    }

    void finishSignOut() {
        // Should be set at start of sign-out flow.
        assert mSignOutState != null;

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
    public void isAccountManaged(String email, final Callback<Boolean> callback) {
        assert email != null;
        CoreAccountInfo account =
                mIdentityManager.findExtendedAccountInfoForAccountWithRefreshTokenByEmailAddress(
                        email);
        assert account != null;
        SigninManagerJni.get().isAccountManaged(mNativeSigninManagerAndroid, account, callback);
    }

    public static String extractDomainName(String email) {
        return SigninManagerJni.get().extractDomainName(email);
    }

    private boolean isGooglePlayServicesPresent() {
        return !ExternalAuthUtils.getInstance().isGooglePlayServicesMissing(
                ContextUtils.getApplicationContext());
    }

    private void fetchAndApplyCloudPolicy(CoreAccountInfo account, final Runnable callback) {
        SigninManagerJni.get().fetchAndApplyCloudPolicy(
                mNativeSigninManagerAndroid, account, callback);
    }

    private void stopApplyingCloudPolicy() {
        SigninManagerJni.get().stopApplyingCloudPolicy(mNativeSigninManagerAndroid);
    }

    private void enableSync(Account account) {
        // Cache the signed-in account name. This must be done after the native call, otherwise
        // sync tries to start without being signed in the native code and crashes.
        mAndroidSyncSettings.updateAccount(account);
        mAndroidSyncSettings.enableChromeSync();
    }

    private void disableSyncAndWipeData(
            boolean shouldWipeUserData, final Runnable wipeDataCallback) {
        mAndroidSyncSettings.updateAccount(null);
        if (shouldWipeUserData) {
            SigninManagerJni.get().wipeProfileData(mNativeSigninManagerAndroid, wipeDataCallback);
        } else {
            SigninManagerJni.get().wipeGoogleServiceWorkerCaches(
                    mNativeSigninManagerAndroid, wipeDataCallback);
        }
    }

    @VisibleForTesting
    IdentityMutator getIdentityMutator() {
        return mIdentityMutator;
    }

    // Native methods.
    @NativeMethods
    interface Natives {
        boolean isSigninAllowedByPolicy(long nativeSigninManagerAndroid);

        boolean isForceSigninEnabled(long nativeSigninManagerAndroid);

        String extractDomainName(String email);

        boolean isMobileIdentityConsistencyEnabled();

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
