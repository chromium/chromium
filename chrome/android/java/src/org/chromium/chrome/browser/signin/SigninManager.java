// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.accounts.Account;
import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.Context;
import android.os.Handler;

import org.chromium.base.ActivityState;
import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.base.Promise;
import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.externalauth.ExternalAuthUtils;
import org.chromium.chrome.browser.externalauth.UserRecoverableErrorHandler;
import org.chromium.chrome.browser.sync.SyncUserDataWiper;
import org.chromium.components.signin.AccountIdProvider;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.ChromeSigninController;
import org.chromium.components.sync.AndroidSyncSettings;

import javax.annotation.Nullable;

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
public class SigninManager implements AccountTrackerService.OnSystemAccountsSeededListener {
    private static final String TAG = "SigninManager";

    @SuppressLint("StaticFieldLeak")
    private static SigninManager sSigninManager;
    private static int sSignInAccessPoint = SigninAccessPoint.UNKNOWN;

    private final Context mContext;
    private final long mNativeSigninManagerAndroid;

    /** Tracks whether the First Run check has been completed.
     *
     * A new sign-in can not be started while this is pending, to prevent the
     * pending check from eventually starting a 2nd sign-in.
     */
    private boolean mFirstRunCheckIsPending = true;

    private final ObserverList<SignInStateObserver> mSignInStateObservers = new ObserverList<>();

    private final ObserverList<SignInAllowedObserver> mSignInAllowedObservers =
            new ObserverList<>();

    /**
    * Will be set during the sign in process, and nulled out when there is not a pending sign in.
    * Needs to be null checked after ever async entry point because it can be nulled out at any time
    * by system accounts changing.
    */
    private SignInState mSignInState;

    private boolean mSigninAllowedByPolicy;

    /**
     * Set during sign-out process and nulled out once complete. Helps to atomically gather/clear
     * various sign-out state.
     */
    private SignOutState mSignOutState;

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
     * Hooks for wiping data during sign out.
     */
    public interface WipeDataHooks {
        /**
         * Called before data is wiped.
         */
        void preWipeData();

        /**
         * Called after data is wiped.
         */
        void postWipeData();
    }

    /**
     * Contains all the state needed for signin. This forces signin flow state to be
     * cleared atomically, and all final fields to be set upon initialization.
     */
    private static class SignInState {
        public final Account account;
        public final Activity activity;
        public final SignInCallback callback;

        /**
         * If the system accounts need to be seeded, the sign in flow will block for that to occur.
         * This boolean should be set to true during that time and then reset back to false
         * afterwards. This allows the manager to know if it should progress the flow when the
         * account tracker broadcasts updates.
         */
        public boolean blockedOnAccountSeeding;

        /**
         * @param account The account to sign in to.
         * @param activity Reference to the UI to use for dialogs. Null means forced signin.
         * @param callback Called when the sign-in process finishes or is cancelled. Can be null.
         */
        public SignInState(
                Account account, @Nullable Activity activity, @Nullable SignInCallback callback) {
            this.account = account;
            this.activity = activity;
            this.callback = callback;
        }

        /**
         * Returns whether this is an interactive sign-in flow.
         */
        public boolean isInteractive() {
            return activity != null;
        }

        /**
         * Returns whether the sign-in flow activity was set but is no longer visible to the user.
         */
        private boolean isActivityInvisible() {
            return activity != null
                    && (ApplicationStatus.getStateForActivity(activity) == ActivityState.STOPPED
                               || ApplicationStatus.getStateForActivity(activity)
                                       == ActivityState.DESTROYED);
        }
    }

    /**
     * Contains all the state needed for sign out. Like SignInState, this forces flow state to be
     * cleared atomically, and all final fields to be set upon initialization.
     */
    private static class SignOutState {
        public final Runnable callback;
        public final WipeDataHooks wipeDataHooks;
        public final String managementDomain;

        /**
         * @param callback Called after sign-out finishes and all data has been cleared.
         * @param wipeDataHooks Hooks to call before/after data wiping phase of sign-out.
         * @param managementDomain Domain when account is managed.
         */
        public SignOutState(@Nullable Runnable callback, @Nullable WipeDataHooks wipeDataHooks,
                @Nullable String managementDomain) {
            this.callback = callback;
            this.wipeDataHooks = wipeDataHooks;
            this.managementDomain = managementDomain;
        }
    }

    /**
     * A helper method for retrieving the application-wide SigninManager.
     * <p/>
     * Can only be accessed on the main thread.
     *
     * @return a singleton instance of the SigninManager.
     */
    public static SigninManager get() {
        ThreadUtils.assertOnUiThread();
        if (sSigninManager == null) {
            sSigninManager = new SigninManager();
        }
        return sSigninManager;
    }

    @VisibleForTesting
    SigninManager() {
        ThreadUtils.assertOnUiThread();
        mContext = ContextUtils.getApplicationContext();
        mNativeSigninManagerAndroid = nativeInit();
        mSigninAllowedByPolicy = nativeIsSigninAllowedByPolicy(mNativeSigninManagerAndroid);

        AccountTrackerService.get().addSystemAccountsSeededListener(this);
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
        return !ApiCompatibilityUtils.isDemoUser(mContext)
                && !ExternalAuthUtils.getInstance().isGooglePlayServicesMissing(mContext);
    }

    /**
     * @return Whether force sign-in is enabled by policy.
     */
    public boolean isForceSigninEnabled() {
        return nativeIsForceSigninEnabled(mNativeSigninManagerAndroid);
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
        new Handler().post(() -> {
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
        if (mSignInState != null && mSignInState.blockedOnAccountSeeding) {
            mSignInState.blockedOnAccountSeeding = false;
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
     * @param activity The activity used to launch UI prompts, or null for a forced signin.
     * @param callback Optional callback for when the sign-in process is finished.
     */
    public void signIn(
            Account account, @Nullable Activity activity, @Nullable SignInCallback callback) {
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

        mSignInState = new SignInState(account, activity, callback);
        notifySignInAllowedChanged();

        progressSignInFlowSeedSystemAccounts();
    }

    /**
     * Same as above but retrieves the Account object for the given accountName.
     */
    public void signIn(String accountName, @Nullable final Activity activity,
            @Nullable final SignInCallback callback) {
        AccountManagerFacade.get().getAccountFromName(
                accountName, account -> signIn(account, activity, callback));
    }

    private void progressSignInFlowSeedSystemAccounts() {
        if (AccountTrackerService.get().checkAndSeedSystemAccounts()) {
            progressSignInFlowCheckPolicy();
        } else if (AccountIdProvider.getInstance().canBeUsed()) {
            mSignInState.blockedOnAccountSeeding = true;
        } else {
            Activity activity = mSignInState.activity;
            UserRecoverableErrorHandler errorHandler = activity != null
                    ? new UserRecoverableErrorHandler.ModalDialog(activity, !isForceSigninEnabled())
                    : new UserRecoverableErrorHandler.SystemNotification();
            ExternalAuthUtils.getInstance().canUseGooglePlayServices(errorHandler);
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

        if (mSignInState.isActivityInvisible()) {
            abortSignIn();
            return;
        }

        if (!nativeShouldLoadPolicyForUser(mSignInState.account.name)) {
            // Proceed with the sign-in flow without checking for policy if it can be determined
            // that this account can't have management enabled based on the username.
            finishSignIn();
            return;
        }

        Log.d(TAG, "Checking if account has policy management enabled");
        // This will call back to onPolicyCheckedBeforeSignIn.
        nativeCheckPolicyBeforeSignIn(mNativeSigninManagerAndroid, mSignInState.account.name);
    }

    @CalledByNative
    private void onPolicyCheckedBeforeSignIn(String managementDomain) {
        assert mSignInState != null;

        if (managementDomain == null) {
            Log.d(TAG, "Account doesn't have policy");
            finishSignIn();
            return;
        }

        if (mSignInState.isActivityInvisible()) {
            abortSignIn();
            return;
        }

        // The user has already been notified that they are signing into a managed account.
        // This will call back to onPolicyFetchedBeforeSignIn.
        nativeFetchPolicyBeforeSignIn(mNativeSigninManagerAndroid);
    }

    @CalledByNative
    private void onPolicyFetchedBeforeSignIn() {
        // Policy has been fetched for the user and is being enforced; features like sync may now
        // be disabled by policy, and the rest of the sign-in flow can be resumed.
        finishSignIn();
    }

    private void finishSignIn() {
        // This method should be called at most once per sign-in flow.
        assert mSignInState != null;

        // Tell the native side that sign-in has completed.
        nativeOnSignInCompleted(mNativeSigninManagerAndroid, mSignInState.account.name);

        // Cache the signed-in account name. This must be done after the native call, otherwise
        // sync tries to start without being signed in natively and crashes.
        ChromeSigninController.get().setSignedInAccountName(mSignInState.account.name);
        AndroidSyncSettings.updateAccount(mSignInState.account);
        AndroidSyncSettings.enableChromeSync();

        if (mSignInState.callback != null) {
            mSignInState.callback.onSignInComplete();
        }

        // Trigger token requests via native.
        logInSignedInUser();

        if (mSignInState.isInteractive()) {
            // If signin was a user action, record that it succeeded.
            RecordUserAction.record("Signin_Signin_Succeed");
            logSigninCompleteAccessPoint();
            // Log signin in reason as defined in signin_metrics.h. Right now only
            // SIGNIN_PRIMARY_ACCOUNT available on Android.
            RecordHistogram.recordEnumeratedHistogram("Signin.SigninReason",
                    SigninReason.SIGNIN_PRIMARY_ACCOUNT, SigninReason.MAX);
        }

        Log.d(TAG, "Signin completed.");
        mSignInState = null;
        notifySignInAllowedChanged();

        for (SignInStateObserver observer : mSignInStateObservers) {
            observer.onSignedIn();
        }
    }

    /**
     * Invokes signOut and returns a {@link Promise} that will be fulfilled on completion.
     * This is equivalent to calling {@link #signOut(@SignoutReason int signoutSource, Runnable
     * callback)} with a callback that fulfills the returned {@link Promise}.
     */
    public Promise<Void> signOutPromise(@SignoutReason int signoutSource) {
        final Promise<Void> promise = new Promise<>();
        signOut(signoutSource, () -> promise.fulfill(null));

        return promise;
    }

    /**
     * Invokes signOut with no callback or wipeDataHooks.
     */
    public void signOut(@SignoutReason int signoutSource) {
        signOut(signoutSource, null, null);
    }

    /**
     * Invokes signOut() with no wipeDataHooks.
     */
    public void signOut(@SignoutReason int signoutSource, Runnable callback) {
        signOut(signoutSource, callback, null);
    }

    /**
     * Signs out of Chrome.
     * <p/>
     * This method clears the signed-in username, stops sync and sends out a
     * sign-out notification on the native side.
     *
     * @param signoutSource describes the event driving the signout (e.g.
     *         {@link SignoutReason.USER_CLICKED_SIGNOUT_SETTINGS}).
     * @param callback Will be invoked after sign-out completes, if not null.
     * @param wipeDataHooks Hooks to call before/after data wiping phase of sign-out.
     */
    public void signOut(
            @SignoutReason int signoutSource, Runnable callback, WipeDataHooks wipeDataHooks) {
        // Only one signOut at a time!
        assert mSignOutState == null;

        // Grab the management domain before nativeSignOut() potentially clears it.
        mSignOutState = new SignOutState(callback, wipeDataHooks, getManagementDomain());

        Log.d(TAG, "Signing out, managementDomain: " + mSignOutState.managementDomain);

        // User data will be wiped in resetAccountData(), called from onNativeSignOut().
        nativeSignOut(mNativeSigninManagerAndroid, signoutSource);
    }

    /**
     * Returns the management domain if the signed in account is managed, otherwise returns null.
     */
    public String getManagementDomain() {
        return nativeGetManagementDomain(mNativeSigninManagerAndroid);
    }

    public void logInSignedInUser() {
        nativeLogInSignedInUser(mNativeSigninManagerAndroid);
    }

    public void clearLastSignedInUser() {
        nativeClearLastSignedInUser(mNativeSigninManagerAndroid);
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

        if (signInState.callback != null) {
            signInState.callback.onSignInAborted();
        }

        nativeAbortSignIn(mNativeSigninManagerAndroid);

        Log.d(TAG, "Signin flow aborted.");
        notifySignInAllowedChanged();
    }

    @VisibleForTesting
    @CalledByNative
    void onNativeSignOut() {
        if (mSignOutState == null) {
            // TODO(https://crbug.com/873671): Management domain is not captured in signOut() for
            // sign-outs that are initiated from the native side. But grabbing it here may be too
            // late! The management domain may be already cleared due to race condition with
            // sign-out observers on the native side.
            mSignOutState = new SignOutState(null, null, getManagementDomain());
        }

        Log.d(TAG, "Native signed out, managementDomain: " + mSignOutState.managementDomain);

        // Native sign-out must happen before resetting the account so data is deleted correctly.
        // http://crbug.com/589028
        resetAccountData();
    }

    /**
     * Called AFTER native sign-out is complete, this method clears various
     * account and profile data associated with the previous signin.
     */
    void resetAccountData() {
        // Should be set at beginning of sign-out flow.
        assert mSignOutState != null;

        ChromeSigninController.get().setSignedInAccountName(null);
        AndroidSyncSettings.updateAccount(null);

        if (mSignOutState.managementDomain != null) {
            wipeProfileData();
        } else {
            wipeGoogleServiceWorkerCaches();
        }

        AccountTrackerService.get().invalidateAccountSeedStatus(true);
    }

    private void wipeProfileData() {
        // Should be set at start of sign-out flow.
        assert mSignOutState != null;

        if (mSignOutState.wipeDataHooks != null) mSignOutState.wipeDataHooks.preWipeData();
        // This will call back to onProfileDataWiped().
        nativeWipeProfileData(mNativeSigninManagerAndroid);
    }

    private void wipeGoogleServiceWorkerCaches() {
        // Should be set at start of sign-out flow.
        assert mSignOutState != null;

        if (mSignOutState.wipeDataHooks != null) mSignOutState.wipeDataHooks.preWipeData();
        // This will call back to onProfileDataWiped().
        nativeWipeGoogleServiceWorkerCaches(mNativeSigninManagerAndroid);
    }

    /**
     * Convenience method to return a Promise to be fulfilled when the user's sync data has been
     * wiped if the parameter is true, or an already fulfilled Promise if the parameter is false.
     */
    public static Promise<Void> wipeSyncUserDataIfRequired(boolean required) {
        if (required) {
            return SyncUserDataWiper.wipeSyncUserData();
        } else {
            return Promise.fulfilled(null);
        }
    }

    @CalledByNative
    private void onProfileDataWiped() {
        // Should be set at start of sign-out flow.
        assert mSignOutState != null;

        if (mSignOutState.wipeDataHooks != null) mSignOutState.wipeDataHooks.postWipeData();
        finishSignOut();
    }

    private void finishSignOut() {
        // Should be set at start of sign-out flow.
        assert mSignOutState != null;

        if (mSignOutState.callback != null) {
            new Handler().post(mSignOutState.callback);
        }
        mSignOutState = null;

        for (SignInStateObserver observer : mSignInStateObservers) {
            observer.onSignedOut();
        }
    }

    /**
     * @return Whether there is a signed in account on the native side.
     */
    public boolean isSignedInOnNative() {
        return nativeIsSignedInOnNative(mNativeSigninManagerAndroid);
    }

    @CalledByNative
    private void onSigninAllowedByPolicyChanged(boolean newSigninAllowedByPolicy) {
        mSigninAllowedByPolicy = newSigninAllowedByPolicy;
        notifySignInAllowedChanged();
    }

    /**
     * Performs an asynchronous check to see if the user is a managed user.
     * @param callback A callback to be called with true if the user is a managed user and false
     *         otherwise. May be called synchronously from this function.
     */
    public static void isUserManaged(String email, final Callback<Boolean> callback) {
        if (nativeShouldLoadPolicyForUser(email)) {
            nativeIsUserManaged(email, callback);
        } else {
            callback.onResult(false);
        }
    }

    public static String extractDomainName(String email) {
        return nativeExtractDomainName(email);
    }

    @VisibleForTesting
    public static void setInstanceForTesting(SigninManager signinManager) {
        sSigninManager = signinManager;
    }

    // Native methods.
    private static native String nativeExtractDomainName(String email);
    private static native boolean nativeShouldLoadPolicyForUser(String username);
    private static native void nativeIsUserManaged(String username, Callback<Boolean> callback);
    @VisibleForTesting
    native long nativeInit();
    @VisibleForTesting
    native boolean nativeIsSigninAllowedByPolicy(long nativeSigninManagerAndroid);
    @VisibleForTesting
    native boolean nativeIsForceSigninEnabled(long nativeSigninManagerAndroid);
    @VisibleForTesting
    native void nativeCheckPolicyBeforeSignIn(long nativeSigninManagerAndroid, String username);
    @VisibleForTesting
    native void nativeFetchPolicyBeforeSignIn(long nativeSigninManagerAndroid);
    @VisibleForTesting
    native void nativeAbortSignIn(long nativeSigninManagerAndroid);
    @VisibleForTesting
    native void nativeOnSignInCompleted(long nativeSigninManagerAndroid, String username);
    @VisibleForTesting
    native void nativeSignOut(long nativeSigninManagerAndroid, @SignoutReason int reason);
    @VisibleForTesting
    native String nativeGetManagementDomain(long nativeSigninManagerAndroid);
    @VisibleForTesting
    native void nativeWipeProfileData(long nativeSigninManagerAndroid);
    @VisibleForTesting
    native void nativeWipeGoogleServiceWorkerCaches(long nativeSigninManagerAndroid);
    @VisibleForTesting
    native void nativeClearLastSignedInUser(long nativeSigninManagerAndroid);
    @VisibleForTesting
    native void nativeLogInSignedInUser(long nativeSigninManagerAndroid);
    @VisibleForTesting
    native boolean nativeIsSignedInOnNative(long nativeSigninManagerAndroid);
}
