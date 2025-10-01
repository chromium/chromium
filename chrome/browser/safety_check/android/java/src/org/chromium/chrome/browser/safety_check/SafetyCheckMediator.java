// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_check;

import static org.chromium.chrome.browser.pwd_check_wrapper.PasswordCheckController.getAccountNameForPasswordStorageType;
import static org.chromium.chrome.browser.safety_check.PasswordsCheckPreferenceProperties.passwordsStateFromPasswordCheckResult;
import static org.chromium.chrome.browser.safety_check.PasswordsCheckPreferenceProperties.passwordsStateToNative;

import android.content.Intent;
import android.net.Uri;
import android.os.Handler;
import android.os.SystemClock;
import android.text.format.DateUtils;
import android.view.View;

import androidx.annotation.IntDef;
import androidx.preference.Preference;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.BuildConfig;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.password_manager.ManagePasswordsReferrer;
import org.chromium.chrome.browser.password_manager.PasswordCheckReferrer;
import org.chromium.chrome.browser.password_manager.PasswordManagerHelper;
import org.chromium.chrome.browser.password_manager.PasswordStoreBridge;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.pwd_check_wrapper.PasswordCheckController;
import org.chromium.chrome.browser.pwd_check_wrapper.PasswordCheckController.PasswordCheckResult;
import org.chromium.chrome.browser.pwd_check_wrapper.PasswordCheckController.PasswordStorageType;
import org.chromium.chrome.browser.pwd_check_wrapper.PasswordCheckControllerFactory;
import org.chromium.chrome.browser.safe_browsing.metrics.SettingsAccessPoint;
import org.chromium.chrome.browser.safe_browsing.settings.SafeBrowsingSettingsFragment;
import org.chromium.chrome.browser.safety_check.PasswordsCheckPreferenceProperties.PasswordsState;
import org.chromium.chrome.browser.safety_check.SafetyCheckProperties.SafeBrowsingState;
import org.chromium.chrome.browser.safety_check.SafetyCheckProperties.UpdatesState;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.components.browser_ui.settings.SettingsCustomTabLauncher;
import org.chromium.components.sync.SyncService;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.ref.WeakReference;

@NullMarked
class SafetyCheckMediator {
    /**
     * The minimal amount of time to show the checking state. This needs to be non-zero to make it
     * seem like the browser is doing work. This is different from the standard guideline of 500ms
     * because of the UX guidance for Safety check on mobile and to be consistent with the Desktop
     * counterpart (also 1s there).
     */
    private static final int CHECKING_MIN_DURATION_MS = 1000;

    /** Time after which the null-states will be shown: 10 minutes. */
    private static final long RESET_TO_NULL_AFTER_MS = 10 * DateUtils.MINUTE_IN_MILLIS;

    private static final String SAFETY_CHECK_INTERACTIONS = "Settings.SafetyCheck.Interactions";

    /** Model representing the current state of the update and safe browsing checks. */
    private PropertyModel mSafetyCheckModel;

    /**
     * Model representing the current state of the password check of passwords from the account
     * storage.
     */
    private @Nullable PropertyModel mPasswordsCheckAccountStorageModel;

    /**
     * Model representing the current state of the password check of passwords from the local
     * storage.
     */
    private @Nullable PropertyModel mPasswordsCheckLocalStorageModel;

    /** Client to interact with Omaha for the updates check. */
    private SafetyCheckUpdatesDelegate mUpdatesClient;

    /** Provides access to C++ APIs. */
    private final SafetyCheckBridge mBridge;

    /** Async logic for password check. */
    private boolean mShowSafePasswordState;

    /** Handles the password check. Contains the logic for both UPM and non-UPM password check. */
    private final PasswordCheckController mPasswordCheckController;

    private final PasswordManagerHelper mPasswordManagerHelper;

    /**
     * Used to open a p-link help center article in a custom tab. Needed by the password manager
     * settings.
     */
    private final SettingsCustomTabLauncher mSettingsCustomTabLauncher;

    private final ObservableSupplier<@Nullable ModalDialogManager> mModalDialogManagerSupplier;

    /** Callbacks and related objects to show the checking state for at least 1 second. */
    private Handler mHandler;

    /** Stores the callback updating the safe browsing check state in the UI after the delay. */
    private @Nullable Runnable mRunnableSafeBrowsing;

    /** Stores the callback updating the updates check state in the UI after the delay. */
    private @Nullable Runnable mRunnableUpdates;

    private long mCheckStartTime = -1;

    /**
     * UMA histogram values for Safety check interactions. Some value don't apply to Android. Note:
     * this should stay in sync with SettingsSafetyCheckInteractions in enums.xml.
     */
    @IntDef({
        SafetyCheckInteractions.STARTED,
        SafetyCheckInteractions.UPDATES_RELAUNCH,
        SafetyCheckInteractions.PASSWORDS_MANAGE,
        SafetyCheckInteractions.SAFE_BROWSING_MANAGE,
        SafetyCheckInteractions.EXTENSIONS_REVIEW,
        // Deprecated in https://crrev.com/c/5113263
        SafetyCheckInteractions.CHROME_CLEANER_REBOOT,
        // Deprecated in https://crrev.com/c/5113263
        SafetyCheckInteractions.CHROME_CLEANER_REVIEW,
        SafetyCheckInteractions.PASSWORDS_MANAGE_THROUGH_CARET_NAVIGATION,
        SafetyCheckInteractions.SAFE_BROWSING_MANAGE_THROUGH_CARET_NAVIGATION,
        SafetyCheckInteractions.EXTENSIONS_REVIEW_THROUGH_CARET_NAVIGATION,
        SafetyCheckInteractions.MAX_VALUE
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface SafetyCheckInteractions {
        int STARTED = 0;
        int UPDATES_RELAUNCH = 1;
        int PASSWORDS_MANAGE = 2;
        int SAFE_BROWSING_MANAGE = 3;
        int EXTENSIONS_REVIEW = 4;
        int CHROME_CLEANER_REBOOT = 5;
        int CHROME_CLEANER_REVIEW = 6;
        int PASSWORDS_MANAGE_THROUGH_CARET_NAVIGATION = 7;
        int SAFE_BROWSING_MANAGE_THROUGH_CARET_NAVIGATION = 8;
        int EXTENSIONS_REVIEW_THROUGH_CARET_NAVIGATION = 9;
        // New elements go above.
        int MAX_VALUE = EXTENSIONS_REVIEW_THROUGH_CARET_NAVIGATION;
    }

    private final SharedPreferencesManager mPreferenceManager;
    private final @Nullable SyncService mSyncService;

    /**
     * Callback that gets invoked once the result of the updates check is available. Not inlined
     * because a {@link WeakReference} needs to be passed (the check is asynchronous).
     */
    private final Callback<Integer> mUpdatesCheckCallback =
            (status) -> {
                if (mHandler == null) return;

                setRunnableUpdates(
                        () -> {
                            if (mSafetyCheckModel != null) {
                                RecordHistogram.recordEnumeratedHistogram(
                                        "Settings.SafetyCheck.UpdatesResult",
                                        SafetyCheckProperties.updatesStateToNative(status),
                                        UpdateStatus.MAX_VALUE);
                                mSafetyCheckModel.set(SafetyCheckProperties.UPDATES_STATE, status);
                            }
                        });
            };

    /**
     * Creates a new instance given a model, an updates client, and a settings launcher.
     *
     * @param safetyCheckModel A model instance.
     * @param client An updates client.
     * @param passwordStoreBridge Provides access to stored passwords.
     * @param modalDialogManagerSupplier A supplier for the {@link ModalDialogManager}.
     */
    public SafetyCheckMediator(
            PropertyModel safetyCheckModel,
            @Nullable PropertyModel passwordsCheckAccountModel,
            @Nullable PropertyModel passwordsCheckLocalModel,
            SafetyCheckUpdatesDelegate client,
            SafetyCheckBridge bridge,
            @Nullable SyncService syncService,
            Handler handler,
            PasswordStoreBridge passwordStoreBridge,
            PasswordCheckControllerFactory passwordCheckControllerFactory,
            PasswordManagerHelper passwordManagerHelper,
            ObservableSupplier<@Nullable ModalDialogManager> modalDialogManagerSupplier,
            SettingsCustomTabLauncher settingsCustomTabLauncher) {
        mSafetyCheckModel = safetyCheckModel;
        mPasswordsCheckAccountStorageModel = passwordsCheckAccountModel;
        mPasswordsCheckLocalStorageModel = passwordsCheckLocalModel;
        mUpdatesClient = client;
        mBridge = bridge;
        mSyncService = syncService;
        mHandler = handler;
        mPreferenceManager = ChromeSharedPreferences.getInstance();
        mPasswordCheckController =
                passwordCheckControllerFactory.create(
                        syncService, passwordStoreBridge, passwordManagerHelper);
        mPasswordManagerHelper = passwordManagerHelper;
        mModalDialogManagerSupplier = modalDialogManagerSupplier;
        mSettingsCustomTabLauncher = settingsCustomTabLauncher;
        // Set the listener for clicking the updates element.
        mSafetyCheckModel.set(
                SafetyCheckProperties.UPDATES_CLICK_LISTENER,
                (Preference.OnPreferenceClickListener)
                        (p) -> {
                            if (!BuildConfig.IS_CHROME_BRANDED) {
                                return true;
                            }
                            // Open the Play Store page for the installed Chrome channel.
                            p.getContext().startActivity(createPlayStoreIntent());
                            return true;
                        });
        // Set the listener for clicking the Safe Browsing element.
        mSafetyCheckModel.set(
                SafetyCheckProperties.SAFE_BROWSING_CLICK_LISTENER,
                (Preference.OnPreferenceClickListener)
                        (p) -> {
                            // Record UMA metrics.
                            RecordUserAction.record("Settings.SafetyCheck.ManageSafeBrowsing");
                            RecordHistogram.recordEnumeratedHistogram(
                                    SAFETY_CHECK_INTERACTIONS,
                                    SafetyCheckInteractions.SAFE_BROWSING_MANAGE,
                                    SafetyCheckInteractions.MAX_VALUE);
                            // Open the Safe Browsing settings.
                            Intent intent =
                                    SettingsNavigationFactory.createSettingsNavigation()
                                            .createSettingsIntent(
                                                    p.getContext(),
                                                    SafeBrowsingSettingsFragment.class,
                                                    SafeBrowsingSettingsFragment.createArguments(
                                                            SettingsAccessPoint.SAFETY_CHECK));
                            p.getContext().startActivity(intent);
                            return true;
                        });
        // Set the listener for clicking the passwords element.
        updatePasswordElementClickDestination(PasswordStorageType.ACCOUNT_STORAGE);
        updatePasswordElementClickDestination(PasswordStorageType.LOCAL_STORAGE);
        // Set the listener for clicking the Check button.
        mSafetyCheckModel.set(
                SafetyCheckProperties.SAFETY_CHECK_BUTTON_CLICK_LISTENER,
                (View.OnClickListener) (v) -> performSafetyCheck());
        // Get the timestamp of the last run.
        mSafetyCheckModel.set(
                SafetyCheckProperties.LAST_RUN_TIMESTAMP,
                mPreferenceManager.readLong(
                        ChromePreferenceKeys.SETTINGS_SAFETY_CHECK_LAST_RUN_TIMESTAMP, 0));
    }

    private static Intent createPlayStoreIntent() {
        String chromeAppId = ContextUtils.getApplicationContext().getPackageName();
        return new Intent(
                Intent.ACTION_VIEW,
                Uri.parse(ContentUrlConstants.PLAY_STORE_URL_PREFIX + chromeAppId));
    }

    /**
     * Determines the initial state to show, triggering any fast checks if necessary based on the
     * last run timestamp.
     */
    public void setInitialState() {
        long currentTime = System.currentTimeMillis();
        long lastRun =
                mPreferenceManager.readLong(
                        ChromePreferenceKeys.SETTINGS_SAFETY_CHECK_LAST_RUN_TIMESTAMP, 0);
        if (currentTime - lastRun < RESET_TO_NULL_AFTER_MS) {
            mShowSafePasswordState = true;
            // Rerun the updates and Safe Browsing checks.
            mSafetyCheckModel.set(
                    SafetyCheckProperties.SAFE_BROWSING_STATE, SafeBrowsingState.CHECKING);
            mSafetyCheckModel.set(SafetyCheckProperties.UPDATES_STATE, UpdatesState.CHECKING);
            checkSafeBrowsing();
            mUpdatesClient.checkForUpdates(new WeakReference(mUpdatesCheckCallback));
        } else {
            mShowSafePasswordState = false;
            mSafetyCheckModel.set(
                    SafetyCheckProperties.SAFE_BROWSING_STATE, SafeBrowsingState.UNCHECKED);
            mSafetyCheckModel.set(SafetyCheckProperties.UPDATES_STATE, UpdatesState.UNCHECKED);
        }
        setPasswordsState(mPasswordsCheckAccountStorageModel, PasswordsState.CHECKING);
        setPasswordsState(mPasswordsCheckLocalStorageModel, PasswordsState.CHECKING);

        fetchPasswordsAndBreachedCredentials(PasswordStorageType.ACCOUNT_STORAGE);
        fetchPasswordsAndBreachedCredentials(PasswordStorageType.LOCAL_STORAGE);
    }

    private void setPasswordsState(
            @Nullable PropertyModel passwordsCheckModel, @PasswordsState int passwordsState) {
        if (passwordsCheckModel == null) return;

        passwordsCheckModel.set(PasswordsCheckPreferenceProperties.PASSWORDS_STATE, passwordsState);
    }

    /** Triggers all safety check child checks. */
    public void performSafetyCheck() {
        // Cancel pending delayed show callbacks if a new check is starting while any existing
        // elements are pending.
        mHandler.removeCallbacksAndMessages(null);
        // Record the start action in UMA.
        RecordUserAction.record("Settings.SafetyCheck.Start");
        // Record the start interaction in the histogram.
        RecordHistogram.recordEnumeratedHistogram(
                SAFETY_CHECK_INTERACTIONS,
                SafetyCheckInteractions.STARTED,
                SafetyCheckInteractions.MAX_VALUE);
        // Record the start time for tracking 1 second checking delay in the UI.
        mCheckStartTime = SystemClock.elapsedRealtime();
        // Record the absolute start time for showing when the last Safety check was performed.
        long currentTime = System.currentTimeMillis();
        mSafetyCheckModel.set(SafetyCheckProperties.LAST_RUN_TIMESTAMP, currentTime);
        mPreferenceManager.writeLong(
                ChromePreferenceKeys.SETTINGS_SAFETY_CHECK_LAST_RUN_TIMESTAMP, currentTime);
        // Increment the stored number of Safety check starts.
        mPreferenceManager.incrementInt(ChromePreferenceKeys.SETTINGS_SAFETY_CHECK_RUN_COUNTER);
        // Set the checking state for all elements.
        setPasswordsState(mPasswordsCheckAccountStorageModel, PasswordsState.CHECKING);
        setPasswordsState(mPasswordsCheckLocalStorageModel, PasswordsState.CHECKING);
        mSafetyCheckModel.set(
                SafetyCheckProperties.SAFE_BROWSING_STATE, SafeBrowsingState.CHECKING);
        mSafetyCheckModel.set(SafetyCheckProperties.UPDATES_STATE, UpdatesState.CHECKING);
        // Start all the checks.
        checkSafeBrowsing();
        checkPasswords(PasswordStorageType.ACCOUNT_STORAGE);
        checkPasswords(PasswordStorageType.LOCAL_STORAGE);
        mUpdatesClient.checkForUpdates(new WeakReference(mUpdatesCheckCallback));
    }

    /** Cancels any pending callbacks and registered observers. */
    @SuppressWarnings("NullAway")
    public void destroy() {
        // Removes all the callbacks from handler
        mHandler.removeCallbacksAndMessages(null);
        mUpdatesClient = null;
        mSafetyCheckModel = null;
        mPasswordsCheckAccountStorageModel = null;
        mPasswordsCheckLocalStorageModel = null;
        mHandler = null;
        mPasswordCheckController.destroy();
    }

    /**
     * Sets {@link mRunnableSafeBrowsing} and, if non-null, runs it with a delay. Will cancel any
     * outstanding callbacks set by previous calls to this method.
     */
    private void setRunnableSafeBrowsing(Runnable r) {
        if (mRunnableSafeBrowsing != null) {
            mHandler.removeCallbacks(mRunnableSafeBrowsing);
        }
        mRunnableSafeBrowsing = r;
        if (mRunnableSafeBrowsing != null) {
            mHandler.postDelayed(mRunnableSafeBrowsing, getModelUpdateDelay());
        }
    }

    /**
     * Sets {@link mRunnableUpdates} and, if non-null, runs it with a delay.
     * Will cancel any outstanding callbacks set by previous calls to this method.
     */
    private void setRunnableUpdates(Runnable r) {
        if (mRunnableUpdates != null) {
            mHandler.removeCallbacks(mRunnableUpdates);
        }
        mRunnableUpdates = r;
        if (mRunnableUpdates != null) {
            mHandler.postDelayed(mRunnableUpdates, getModelUpdateDelay());
        }
    }

    private void checkSafeBrowsing() {
        setRunnableSafeBrowsing(
                () -> {
                    if (mSafetyCheckModel != null) {
                        @SafeBrowsingStatus int status = mBridge.checkSafeBrowsing();
                        RecordHistogram.recordEnumeratedHistogram(
                                "Settings.SafetyCheck.SafeBrowsingResult",
                                status,
                                SafeBrowsingStatus.MAX_VALUE);
                        mSafetyCheckModel.set(
                                SafetyCheckProperties.SAFE_BROWSING_STATE,
                                SafetyCheckProperties.safeBrowsingStateFromNative(status));
                    }
                });
    }

    /** Called when all data is loaded. Determines if it needs to update the model. */
    private void determinePasswordStateOnLoadComplete(
            PasswordCheckResult passwordSafetyCheckResult,
            @PasswordStorageType int passwordStorageType,
            boolean isInitialLoad) {
        // Only delay updating the UI on the user-triggered check and not initially.
        if (isInitialLoad) {
            updatePasswordsStateOnDataLoaded(passwordSafetyCheckResult, passwordStorageType, true);
            return;
        }
        mHandler.postDelayed(
                () ->
                        updatePasswordsStateOnDataLoaded(
                                passwordSafetyCheckResult, passwordStorageType, false),
                getModelUpdateDelay());
    }

    /** Applies the results of the password check to the model. Only called when data is loaded. */
    private void updatePasswordsStateOnDataLoaded(
            PasswordCheckResult passwordSafetyCheckResult,
            @PasswordStorageType int passwordStorageType,
            boolean isInitialLoad) {
        PropertyModel passwordsCheckModel = getPasswordsCheckModelForStoreType(passwordStorageType);
        if (passwordsCheckModel == null) return;

        if (passwordSafetyCheckResult.getBreachedCount() != null) {
            passwordsCheckModel.set(
                    PasswordsCheckPreferenceProperties.COMPROMISED_PASSWORDS_COUNT,
                    passwordSafetyCheckResult.getBreachedCount());
        }

        @PasswordsState int passwordsState;
        if (isInitialLoad) {
            // Cannot show the safe state at the initial load if last run is older than 10 mins.
            passwordsState = getPasswordStateWhenInitialLoad(passwordSafetyCheckResult);
        } else {
            passwordsState = passwordsStateFromPasswordCheckResult(passwordSafetyCheckResult);
            RecordHistogram.recordEnumeratedHistogram(
                    "Settings.SafetyCheck.PasswordsResult2",
                    passwordsStateToNative(passwordsState),
                    PasswordsStatus.MAX_VALUE);
        }

        passwordsCheckModel.set(PasswordsCheckPreferenceProperties.PASSWORDS_STATE, passwordsState);
        updatePasswordElementClickDestination(passwordStorageType);
    }

    private @PasswordsState int getPasswordStateWhenInitialLoad(
            PasswordCheckResult passwordCheckResult) {
        if (passwordCheckResult.getBreachedCount() != null
                && passwordCheckResult.getBreachedCount() > 0) {
            return PasswordsState.COMPROMISED_EXIST;
        }
        @PasswordsState
        int passwordsState = passwordsStateFromPasswordCheckResult(passwordCheckResult);
        if (!mShowSafePasswordState) {
            return PasswordsState.UNCHECKED;
        }
        return passwordsState;
    }

    /**
     * @return The delay in ms for updating the model in the running state.
     */
    private long getModelUpdateDelay() {
        return Math.max(
                0, mCheckStartTime + CHECKING_MIN_DURATION_MS - SystemClock.elapsedRealtime());
    }

    /** Sets the destination of the click on the passwords element based on the current state. */
    private void updatePasswordElementClickDestination(
            @PasswordStorageType int passwordStorageType) {
        PropertyModel passwordsCheckModel = getPasswordsCheckModelForStoreType(passwordStorageType);
        if (passwordsCheckModel == null) return;

        @PasswordsState
        int state = passwordsCheckModel.get(PasswordsCheckPreferenceProperties.PASSWORDS_STATE);
        Preference.OnPreferenceClickListener listener = null;
        if (state == PasswordsState.UNCHECKED) {
            listener =
                    (p) -> {
                        String account =
                                getAccountNameForPasswordStorageType(
                                        passwordStorageType, mSyncService);
                        mPasswordManagerHelper.showPasswordSettings(
                                p.getContext(),
                                ManagePasswordsReferrer.SAFETY_CHECK,
                                mModalDialogManagerSupplier,
                                /* managePasskeys= */ false,
                                account,
                                mSettingsCustomTabLauncher);
                        return true;
                    };
        } else if (state == PasswordsState.COMPROMISED_EXIST || state == PasswordsState.SAFE) {
            listener =
                    (p) -> {
                        // Record UMA metrics.
                        RecordUserAction.record("Settings.SafetyCheck.ManagePasswords");
                        RecordHistogram.recordEnumeratedHistogram(
                                SAFETY_CHECK_INTERACTIONS,
                                SafetyCheckInteractions.PASSWORDS_MANAGE,
                                SafetyCheckInteractions.MAX_VALUE);
                        // Open the Password Check UI.
                        // This UI surface was deprecated for all use-cases apart form the
                        // PhishGuard dialog. The dialog option leading to SafetyCheck, implies
                        // that there are passwords saved in both local and account stores. This
                        // means that UPM
                        // is guaranteed to be available.
                        String account =
                                getAccountNameForPasswordStorageType(
                                        passwordStorageType, mSyncService);
                        mPasswordManagerHelper.showPasswordCheckup(
                                p.getContext(),
                                PasswordCheckReferrer.SAFETY_CHECK,
                                mModalDialogManagerSupplier,
                                account,
                                mSettingsCustomTabLauncher);
                        return true;
                    };
        }
        passwordsCheckModel.set(
                PasswordsCheckPreferenceProperties.PASSWORDS_CLICK_LISTENER, listener);
    }

    private void fetchPasswordsAndBreachedCredentials(
            @PasswordStorageType int passwordStorageType) {
        PropertyModel passwordCheckModel = getPasswordsCheckModelForStoreType(passwordStorageType);
        if (passwordCheckModel == null) return;

        WeakReference<SafetyCheckMediator> weakRef = new WeakReference(this);
        mPasswordCheckController
                .getBreachedCredentialsCount(passwordStorageType)
                .whenComplete(
                        (result, error) -> {
                            SafetyCheckMediator mediator = weakRef.get();
                            if (mediator == null) return;

                            if (error != null) {
                                mediator.onPasswordCheckFailed(error, passwordStorageType, true);
                            } else {
                                mediator.determinePasswordStateOnLoadComplete(
                                        result, passwordStorageType, true);
                            }
                        });
    }

    private void checkPasswords(@PasswordStorageType int passwordStorageType) {
        PropertyModel passwordCheckModel = getPasswordsCheckModelForStoreType(passwordStorageType);
        if (passwordCheckModel == null) return;

        WeakReference<SafetyCheckMediator> weakRef = new WeakReference(this);
        mPasswordCheckController
                .checkPasswords(passwordStorageType)
                .whenComplete(
                        (result, error) -> {
                            SafetyCheckMediator mediator = weakRef.get();
                            if (mediator == null) return;

                            if (error != null) {
                                mediator.onPasswordCheckFailed(error, passwordStorageType, false);
                            } else {
                                mediator.determinePasswordStateOnLoadComplete(
                                        result, passwordStorageType, false);
                            }
                        });
    }

    private @Nullable PropertyModel getPasswordsCheckModelForStoreType(
            @PasswordStorageType int passwordStoreType) {
        if (passwordStoreType == PasswordStorageType.ACCOUNT_STORAGE) {
            return mPasswordsCheckAccountStorageModel;
        }
        if (passwordStoreType == PasswordStorageType.LOCAL_STORAGE) {
            return mPasswordsCheckLocalStorageModel;
        }
        assert false : "Unknown password storage type";
        return null;
    }

    private void onPasswordCheckFailed(
            Throwable error, @PasswordStorageType int passwordStorageType, boolean isInitialLoad) {
        RecordHistogram.recordEnumeratedHistogram(
                "Settings.SafetyCheck.PasswordsResult2",
                PasswordsCheckPreferenceProperties.passwordsStateToNative(PasswordsState.ERROR),
                PasswordsStatus.MAX_VALUE);
        determinePasswordStateOnLoadComplete(
                new PasswordCheckResult(new Exception(error)), passwordStorageType, isInitialLoad);
    }
}
