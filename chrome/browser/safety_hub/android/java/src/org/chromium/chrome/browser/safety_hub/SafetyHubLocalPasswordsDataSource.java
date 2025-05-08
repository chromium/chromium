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
import org.chromium.components.prefs.PrefService;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * DataSource for the Safety Hub local password module. Listens to changes of local passwords and
 * their state, and notifies its observer of the current module type.
 */
@NullMarked
public class SafetyHubLocalPasswordsDataSource
        implements SafetyHubFetchService.Observer, PasswordStoreBridge.PasswordStoreObserver {
    interface Observer {
        void localPasswordsStateChanged(@ModuleType int moduleType);
    }

    /**
     * This should match the default value for {@link
     * org.chromium.chrome.browser.preferences.Pref.LOCAL_BREACHED_CREDENTIALS_COUNT}.
     */
    private static final int INVALID_BREACHED_CREDENTIALS_COUNT = -1;

    // Represents the type of local password module.
    @IntDef({
        ModuleType.UNAVAILABLE_PASSWORDS,
        ModuleType.NO_SAVED_PASSWORDS,
        ModuleType.HAS_COMPROMISED_PASSWORDS,
        ModuleType.NO_COMPROMISED_PASSWORDS,
        ModuleType.HAS_WEAK_PASSWORDS,
        ModuleType.HAS_REUSED_PASSWORDS,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface ModuleType {
        int UNAVAILABLE_PASSWORDS = 0;
        int NO_SAVED_PASSWORDS = 1;
        int HAS_COMPROMISED_PASSWORDS = 2;
        int NO_COMPROMISED_PASSWORDS = 3;
        int HAS_WEAK_PASSWORDS = 4;
        int HAS_REUSED_PASSWORDS = 5;
    };

    private final SafetyHubModuleDelegate mModuleDelegate;
    private final PrefService mPrefService;
    private final SafetyHubFetchService mSafetyHubFetchService;
    private final @Nullable PasswordStoreBridge mPasswordStoreBridge;
    private final ObserverList<Observer> mObservers;

    private int mCompromisedPasswordCount;
    private int mReusedPasswordCount;
    private int mWeakPasswordCount;
    private @ModuleType int mModuleType;

    private boolean mSavedPasswordsAvailable;
    private boolean mLocalPasswordCountsAvailable;

    SafetyHubLocalPasswordsDataSource(
            SafetyHubModuleDelegate moduleDelegate,
            PrefService prefService,
            SafetyHubFetchService safetyHubFetchService,
            @Nullable PasswordStoreBridge passwordStoreBridge) {
        mModuleDelegate = moduleDelegate;
        mPrefService = prefService;
        mSafetyHubFetchService = safetyHubFetchService;
        mPasswordStoreBridge = passwordStoreBridge;
        mObservers = new ObserverList<>();
    }

    public void setUp() {
        mSafetyHubFetchService.addObserver(this);
        if (mPasswordStoreBridge != null) {
            mPasswordStoreBridge.addObserver(this, /* callImmediatelyIfReady= */ true);
        }
    }

    /**
     * Attempts to trigger a password check in the background.
     *
     * @return {@code true} if the checkup will be performed. Otherwise, returns {@code false}, e.g.
     *     when the last checkup results are within the cool down period.
     */
    public boolean maybeTriggerPasswordCheckup() {
        // After triggering the checkup, this data source will be notified of
        // changes to the count values via @{link localPasswordCountsChanged}.
        return mSafetyHubFetchService.runLocalPasswordCheckup();
    }

    public void updateState() {
        if (!canUpdateState()) {
            return;
        }

        updateCompromisedPasswordCount();
        updateReusedPasswordCount();
        updateWeakPasswordCount();
        mModuleType = calculateModuleType();

        for (Observer observer : mObservers) {
            observer.localPasswordsStateChanged(mModuleType);
        }
    }

    public void addObserver(Observer observer) {
        mObservers.addObserver(observer);
    }

    public @ModuleType int getModuleType() {
        return mModuleType;
    }

    // Returns the password module type according to the application state.
    private @ModuleType int calculateModuleType() {
        assert canUpdateState();

        if (getTotalPasswordCount() == 0) {
            return ModuleType.NO_SAVED_PASSWORDS;
        }
        if (mCompromisedPasswordCount == INVALID_BREACHED_CREDENTIALS_COUNT) {
            return ModuleType.UNAVAILABLE_PASSWORDS;
        }
        if (mCompromisedPasswordCount > 0) {
            return ModuleType.HAS_COMPROMISED_PASSWORDS;
        }
        if (ChromeFeatureList.sSafetyHubWeakAndReusedPasswords.isEnabled()) {
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

    private boolean canUpdateState() {
        return mSavedPasswordsAvailable && mLocalPasswordCountsAvailable;
    }

    private void updateCompromisedPasswordCount() {
        assert mPrefService != null
                : "A null PrefService was detected in SafetyHubLocalPasswordsDataSource";
        mCompromisedPasswordCount = mPrefService.getInteger(Pref.LOCAL_BREACHED_CREDENTIALS_COUNT);
    }

    public int getReusedPasswordCount() {
        return mReusedPasswordCount;
    }

    private void updateReusedPasswordCount() {
        assert mPrefService != null
                : "A null PrefService was detected in SafetyHubLocalPasswordsDataSource";
        mReusedPasswordCount = mPrefService.getInteger(Pref.LOCAL_REUSED_CREDENTIALS_COUNT);
    }

    public int getWeakPasswordCount() {
        return mWeakPasswordCount;
    }

    private void updateWeakPasswordCount() {
        assert mPrefService != null
                : "A null PrefService was detected in SafetyHubLocalPasswordsDataSource";
        mWeakPasswordCount = mPrefService.getInteger(Pref.LOCAL_WEAK_CREDENTIALS_COUNT);
    }

    private int getTotalPasswordCount() {
        assert mModuleDelegate != null
                : "A null ModuleDelegate was detected in SafetyHubLocalPasswordsDataSource";
        return mModuleDelegate.getLocalPasswordsCount(mPasswordStoreBridge);
    }

    public void destroy() {
        mSafetyHubFetchService.removeObserver(this);
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
        // no-op.
    }

    @Override
    public void localPasswordCountsChanged() {
        mLocalPasswordCountsAvailable = true;
        updateState();
    }

    @Override
    public void onSavedPasswordsChanged(int count) {
        mSavedPasswordsAvailable = true;
        updateState();
    }

    @Override
    public void onEdit(PasswordStoreCredential credential) {
        // no-op.
    }

    public boolean isManaged() {
        return mPrefService.isManagedPreference(Pref.CREDENTIALS_ENABLE_SERVICE)
                && !passwordSavingEnabled();
    }

    private boolean passwordSavingEnabled() {
        return mPrefService.getBoolean(Pref.CREDENTIALS_ENABLE_SERVICE);
    }

    public void triggerNewCredentialFetch() {
        mSafetyHubFetchService.fetchLocalCredentialsCount();
    }
}
