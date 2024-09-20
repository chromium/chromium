// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import android.app.Activity;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;

import org.chromium.base.IntentUtils;
import org.chromium.base.TimeUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.sync.TrustedVaultClient;
import org.chromium.chrome.browser.sync.ui.SyncTrustedVaultProxyActivity;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.sync.TrustedVaultUserActionTriggerForUMA;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.base.WindowAndroid;

import java.util.concurrent.TimeUnit;

/** The bridge provides a way to interact with the Android sign in flow. */
public class PasswordManagerErrorMessageHelperBridge {
    @VisibleForTesting
    static final long MINIMAL_INTERVAL_BETWEEN_PROMPTS_MS =
            TimeUnit.MILLISECONDS.convert(24, TimeUnit.HOURS);

    @VisibleForTesting
    static final long MINIMAL_INTERVAL_TO_SYNC_ERROR_MS =
            TimeUnit.MILLISECONDS.convert(30, TimeUnit.MINUTES);

    /**
     * Checks whether the right amount of time has passed since the last error UI messages were
     * shown.
     *
     * <p>The error UI should be shown at least {@link #MINIMAL_INTERVAL_BETWEEN_PROMPTS_MS} from
     * the previous one and at least {@link #MINIMAL_INTERVAL_TO_SYNC_ERROR_MS} from the last sync
     * error UI.
     *
     * @return whether the UI can be shown given the conditions above.
     */
    @CalledByNative
    static boolean shouldShowSignInErrorUI(Profile profile) {
        final IdentityManager identityManager =
                IdentityServicesProvider.get().getIdentityManager(profile);
        if (identityManager == null) return false;

        // It is possible that the account is removed from Chrome between the password manager
        // calling the Google Play Services backend and Chrome receiving the reply. In that
        // case, the error is no longer relevant/fixable.
        if (identityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN) == null) return false;

        PrefService prefService = UserPrefs.get(profile);
        long lastShownTimestamp =
                Long.valueOf(prefService.getString(Pref.UPM_ERROR_UI_SHOWN_TIMESTAMP));
        long lastShownSyncErrorTimestamp =
                ChromeSharedPreferences.getInstance()
                        .readLong(ChromePreferenceKeys.SYNC_ERROR_MESSAGE_SHOWN_AT_TIME, 0);
        long currentTime = TimeUtils.currentTimeMillis();
        return (currentTime - lastShownTimestamp > MINIMAL_INTERVAL_BETWEEN_PROMPTS_MS)
                && (currentTime - lastShownSyncErrorTimestamp) > MINIMAL_INTERVAL_TO_SYNC_ERROR_MS;
    }

    /**
     * Checks whether the right amount of time has passed since the last error UI messages were
     * shown.
     *
     * <p>The error UI should be shown at least {@link #MINIMAL_INTERVAL_BETWEEN_PROMPTS_MS} from
     * the previous one.
     *
     * @return whether the UI can be shown given the conditions above.
     */
    @CalledByNative
    static boolean shouldShowUpdateGMSCoreErrorUI(Profile profile) {
        PrefService prefService = UserPrefs.get(profile);
        long lastShownTimestamp =
                Long.valueOf(prefService.getString(Pref.UPM_ERROR_UI_SHOWN_TIMESTAMP));
        long currentTime = TimeUtils.currentTimeMillis();
        return currentTime - lastShownTimestamp > MINIMAL_INTERVAL_BETWEEN_PROMPTS_MS;
    }

    /** Saves the timestamp in ms since UNIX epoch at which the error UI was shown. */
    @CalledByNative
    static void saveErrorUiShownTimestamp(Profile profile) {
        PrefService prefService = UserPrefs.get(profile);
        prefService.setString(
                Pref.UPM_ERROR_UI_SHOWN_TIMESTAMP, Long.toString(TimeUtils.currentTimeMillis()));
    }

    /**
     * Starts the Android process to update credentials for the primary account in Chrome. This
     * method will only work for users that have been previously signed in Chrome on the device.
     */
    @CalledByNative
    static void startUpdateAccountCredentialsFlow(WindowAndroid windowAndroid, Profile profile) {
        final CoreAccountInfo primaryAccountInfo =
                IdentityServicesProvider.get()
                        .getIdentityManager(profile)
                        .getPrimaryAccountInfo(ConsentLevel.SIGNIN);
        // If the account has been removed before calling this method, there are no credentials to
        // update.
        if (primaryAccountInfo == null) return;
        final Activity activity = windowAndroid.getActivity().get();
        AccountManagerFacadeProvider.getInstance()
                .updateCredentials(
                        CoreAccountInfo.getAndroidAccountFrom(primaryAccountInfo),
                        activity,
                        (success) -> {
                            RecordHistogram.recordBooleanHistogram(
                                    "PasswordManager.UPMUpdateSignInCredentialsSucces", success);
                        });
    }

    /**
     * Starts the Android process to retrieve encryption keys in Chrome. This method will only work for users that have been previously syncing in Chrome.
     */
    @CalledByNative
    static void startTrustedVaultKeyRetrievalFlow(WindowAndroid windowAndroid, Profile profile) {
        final CoreAccountInfo primaryAccountInfo =
                SyncServiceFactory.getForProfile(profile).getAccountInfo();
        // If the account has been removed before calling this method, there is nothing to do.
        if (primaryAccountInfo == null) return;
        final Activity activity = windowAndroid.getActivity().get();

        TrustedVaultClient.get()
                .createKeyRetrievalIntent(primaryAccountInfo)
                .then(
                        (intent) -> {
                            var action =
                                    TrustedVaultUserActionTriggerForUMA
                                            .PASSWORD_MANAGER_ERROR_MESSAGE;
                            var proxyIntent =
                                    SyncTrustedVaultProxyActivity.createKeyRetrievalProxyIntent(
                                            intent, action);
                            IntentUtils.safeStartActivity(activity, proxyIntent);
                        });
    }

    /** Starts the Google Play services page where the user can choose to update GMSCore. */
    @CalledByNative
    static void launchGmsUpdate(WindowAndroid windowAndroid) {
        assert windowAndroid.getActivity().get() != null;
        Activity activity = windowAndroid.getActivity().get();
        GmsUpdateLauncher.launch(activity);
    }
}
