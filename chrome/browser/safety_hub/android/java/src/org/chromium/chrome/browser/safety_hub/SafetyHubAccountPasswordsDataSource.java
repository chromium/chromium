// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import androidx.annotation.IntDef;

import org.chromium.base.ObserverList;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.password_manager.PasswordStoreBridge;
import org.chromium.chrome.browser.password_manager.PasswordStoreCredential;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.components.prefs.PrefService;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * DataSource for the Safety Hub password module. Listens to changes of passwords and their state,
 * and notifies its observer of the current module type.
 */
@NullMarked
public class SafetyHubAccountPasswordsDataSource
        implements SafetyHubFetchService.Observer,
                PasswordStoreBridge.PasswordStoreObserver,
                SigninManager.SignInStateObserver {
    interface Observer {
        void accountPasswordsStateChanged(@ModuleType int moduleType);
    }

    /**
     * This should match the default value for {@link
     * org.chromium.chrome.browser.preferences.Pref.BREACHED_CREDENTIALS_COUNT}.
     */
    private static final int INVALID_BREACHED_CREDENTIALS_COUNT = -1;

    // Represents the type of password module.
    @IntDef({
        ModuleType.SIGNED_OUT,
        ModuleType.UNAVAILABLE_PASSWORDS,
        ModuleType.NO_SAVED_PASSWORDS,
        ModuleType.HAS_COMPROMISED_PASSWORDS,
        ModuleType.NO_COMPROMISED_PASSWORDS,
        ModuleType.HAS_WEAK_PASSWORDS,
        ModuleType.HAS_REUSED_PASSWORDS,
        ModuleType.UNAVAILABLE_COMPROMISED_NO_WEAK_REUSED_PASSWORDS
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface ModuleType {
        int SIGNED_OUT = 0;
        int UNAVAILABLE_PASSWORDS = 1;
        int NO_SAVED_PASSWORDS = 2;
        int HAS_COMPROMISED_PASSWORDS = 3;
        int NO_COMPROMISED_PASSWORDS = 4;
        int HAS_WEAK_PASSWORDS = 5;
        int HAS_REUSED_PASSWORDS = 6;
        int UNAVAILABLE_COMPROMISED_NO_WEAK_REUSED_PASSWORDS = 7;
    };

    private final Profile mProfile;
    private final PrefService mPrefService;
    private final SafetyHubFetchService mSafetyHubFetchService;
    private @Nullable final SigninManager mSigninManager;
    private final SafetyHubModuleDelegate mModuleDelegate;
    private final ObserverList<Observer> mObservers;

    private @Nullable PasswordStoreBridge mPasswordStoreBridge;

    private int mCompromisedPasswordCount;
    private int mWeakPasswordCount;
    private int mReusedPasswordCount;
    private @ModuleType int mModuleType;

    SafetyHubAccountPasswordsDataSource(
            SafetyHubModuleDelegate moduleDelegate,
            PrefService prefService,
            SafetyHubFetchService safetyHubFetchService,
            @Nullable SigninManager signinManager,
            Profile profile) {
        mPrefService = prefService;
        mSafetyHubFetchService = safetyHubFetchService;
        mModuleDelegate = moduleDelegate;
        mProfile = profile;
        mSigninManager = signinManager;
        mObservers = new ObserverList<>();
    }

    public void setUp() {
        mSafetyHubFetchService.addObserver(this);

        if (isSignedIn()) {
            mPasswordStoreBridge = new PasswordStoreBridge(mProfile);
            if (mPasswordStoreBridge != null) {
                mPasswordStoreBridge.addObserver(this, true);
            }
        }
        assert mSigninManager != null : "SigninManager should not be null.";
        mSigninManager.addSignInStateObserver(this);
    }

    public void updateState() {
        updateCompromisedPasswordCount();
        updateReusedPasswordCount();
        updateWeakPasswordCount();
        mModuleType = calculateModuleType();

        for (Observer observer : mObservers) {
            observer.accountPasswordsStateChanged(mModuleType);
        }
    }

    public void addObserver(Observer observer) {
        mObservers.addObserver(observer);
    }

    public @ModuleType int getModuleType() {
        return mModuleType;
    }

    // Calculates the password module type according to the application state.
    private @ModuleType int calculateModuleType() {
        boolean isWeakAndReusedFeatureEnabled =
                ChromeFeatureList.sSafetyHubWeakAndReusedPasswords.isEnabled();

        if (!isSignedIn()) {
            assert mCompromisedPasswordCount == INVALID_BREACHED_CREDENTIALS_COUNT;
            return ModuleType.SIGNED_OUT;
        }
        if (getTotalPasswordCount() == 0) {
            return ModuleType.NO_SAVED_PASSWORDS;
        }
        if (mCompromisedPasswordCount == INVALID_BREACHED_CREDENTIALS_COUNT) {
            if (isWeakAndReusedFeatureEnabled
                    && mWeakPasswordCount == 0
                    && mReusedPasswordCount == 0) {
                return ModuleType.UNAVAILABLE_COMPROMISED_NO_WEAK_REUSED_PASSWORDS;
            }

            return ModuleType.UNAVAILABLE_PASSWORDS;
        }
        if (mCompromisedPasswordCount > 0) {
            return ModuleType.HAS_COMPROMISED_PASSWORDS;
        }
        if (isWeakAndReusedFeatureEnabled) {
            // Reused passwords take priority over the weak passwords count.
            if (mReusedPasswordCount > 0) {
                return ModuleType.HAS_REUSED_PASSWORDS;
            }
            if (mWeakPasswordCount > 0) {
                return ModuleType.HAS_WEAK_PASSWORDS;
            }
        }

        // If both reused passwords and weak passwords counts are invalid, ignore them in favour
        // of showing the compromised passwords count.
        return ModuleType.NO_COMPROMISED_PASSWORDS;
    }

    public int getCompromisedPasswordCount() {
        return mCompromisedPasswordCount;
    }

    private void updateCompromisedPasswordCount() {
        assert mPrefService != null
                : "A null PrefService was detected in" + " SafetyHubAccountPasswordsDataSource";
        mCompromisedPasswordCount = mPrefService.getInteger(Pref.BREACHED_CREDENTIALS_COUNT);
    }

    public int getWeakPasswordCount() {
        return mWeakPasswordCount;
    }

    private void updateWeakPasswordCount() {
        assert mPrefService != null
                : "A null PrefService was detected in" + " SafetyHubAccountPasswordsDataSource";
        mWeakPasswordCount = mPrefService.getInteger(Pref.WEAK_CREDENTIALS_COUNT);
    }

    public int getReusedPasswordCount() {
        return mReusedPasswordCount;
    }

    private void updateReusedPasswordCount() {
        assert mPrefService != null
                : "A null PrefService was detected in" + " SafetyHubAccountPasswordsDataSource";
        mReusedPasswordCount = mPrefService.getInteger(Pref.REUSED_CREDENTIALS_COUNT);
    }

    private int getTotalPasswordCount() {
        assert mModuleDelegate != null
                : "A null ModuleDelegate was detected in" + " SafetyHubAccountPasswordsDataSource";
        return mModuleDelegate.getAccountPasswordsCount(mPasswordStoreBridge);
    }

    private boolean isSignedIn() {
        assert mProfile != null
                : "A null Profile was detected in" + " SafetyHubAccountPasswordsDataSource";
        return SafetyHubUtils.isSignedIn(mProfile);
    }

    public @Nullable String getAccountEmail() {
        assert mProfile != null
                : "A null Profile was detected in" + " SafetyHubAccountPasswordsDataSource";
        return SafetyHubUtils.getAccountEmail(mProfile);
    }

    public void destroy() {
        mSafetyHubFetchService.removeObserver(this);
        if (mSigninManager != null) {
            mSigninManager.removeSignInStateObserver(this);
        }
        if (mPasswordStoreBridge != null) {
            mPasswordStoreBridge.removeObserver(this);
        }
        mObservers.clear();
    }

    @Override
    public void updateStatusChanged() {
        // no-op.
    }

    @Override
    public void accountPasswordCountsChanged() {
        updateState();
    }

    @Override
    public void localPasswordCountsChanged() {
        // no-op.
    }

    @Override
    public void onSavedPasswordsChanged(int count) {
        updateState();
    }

    @Override
    public void onSignedIn() {
        if (mPasswordStoreBridge == null) {
            mPasswordStoreBridge = new PasswordStoreBridge(mProfile);
            mPasswordStoreBridge.addObserver(this, true);
        }
        updateState();
    }

    @Override
    public void onEdit(PasswordStoreCredential credential) {
        // no-op.
    }

    @Override
    public void onSignedOut() {
        if (mPasswordStoreBridge != null) {
            mPasswordStoreBridge.removeObserver(this);
            mPasswordStoreBridge = null;
        }
        updateState();
    }

    public boolean isManaged() {
        assert mPrefService != null
                : "A null PrefService was detected in" + " SafetyHubAccountPasswordsModuleMediator";
        return mPrefService.isManagedPreference(Pref.CREDENTIALS_ENABLE_SERVICE)
                && !passwordSavingEnabled();
    }

    private boolean passwordSavingEnabled() {
        assert mPrefService != null
                : "A null PrefService was detected in" + " SafetyHubAccountPasswordsModuleMediator";
        return mPrefService.getBoolean(Pref.CREDENTIALS_ENABLE_SERVICE);
    }

    public void triggerNewCredentialFetch() {
        mSafetyHubFetchService.fetchAccountCredentialsCount(success -> {});
    }
}
