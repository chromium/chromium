// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

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
public class SafetyHubLocalPasswordsDataSource
        implements SafetyHubFetchService.Observer, PasswordStoreBridge.PasswordStoreObserver {
    interface Observer {
        void stateChanged(@ModuleType int moduleType);
    }

    // Represents the type of local password module.
    @IntDef({
        ModuleType.UNAVAILABLE_PASSWORDS,
        ModuleType.NO_SAVED_PASSWORDS,
        ModuleType.HAS_COMPROMISED_PASSWORDS
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface ModuleType {
        int UNAVAILABLE_PASSWORDS = 0;
        int NO_SAVED_PASSWORDS = 1;
        int HAS_COMPROMISED_PASSWORDS = 2;
    };

    @NonNull private final SafetyHubModuleDelegate mModuleDelegate;
    @NonNull private final PrefService mPrefService;
    @NonNull private final SafetyHubFetchService mSafetyHubFetchService;
    @Nullable private final PasswordStoreBridge mPasswordStoreBridge;

    private Observer mObserver;

    private int mCompromisedPasswordCount;

    SafetyHubLocalPasswordsDataSource(
            @NonNull SafetyHubModuleDelegate moduleDelegate,
            @NonNull PrefService prefService,
            @NonNull SafetyHubFetchService safetyHubFetchService,
            @Nullable PasswordStoreBridge passwordStoreBridge) {
        mModuleDelegate = moduleDelegate;
        mPrefService = prefService;
        mSafetyHubFetchService = safetyHubFetchService;
        mPasswordStoreBridge = passwordStoreBridge;
    }

    public void setUp() {
        mSafetyHubFetchService.addObserver(this);
        if (mPasswordStoreBridge != null) {
            mPasswordStoreBridge.addObserver(this, true);
        }
    }

    public boolean maybeTriggerPasswordCheckup() {
        // TODO(crbug.com/388788969): Only trigger the checkup if it can be ran.
        // After triggering the checkup, this data source will be notified of
        // changes to the count values via @{link localPasswordCountsChanged}.
        mSafetyHubFetchService.runLocalPasswordCheckup();
        return true;
    }

    public void updateState() {
        // TODO(crbug.com/388788969): Update password counts.
        updateCompromisedPasswordCount();

        if (mObserver != null) {
            mObserver.stateChanged(getModuleType());
        }
    }

    public void setObserver(Observer observer) {
        mObserver = observer;
    }

    // Returns the password module type according to the application state.
    private @ModuleType int getModuleType() {
        // TODO(crbug.com/388788969): Add more module types.
        if (getTotalPasswordCount() == 0) {
            return ModuleType.NO_SAVED_PASSWORDS;
        }
        if (mCompromisedPasswordCount > 0) {
            return ModuleType.HAS_COMPROMISED_PASSWORDS;
        }
        return ModuleType.UNAVAILABLE_PASSWORDS;
    }

    public int getCompromisedPasswordCount() {
        return mCompromisedPasswordCount;
    }

    private void updateCompromisedPasswordCount() {
        assert mPrefService != null
                : "A null PrefService was detected in SafetyHubLocalPasswordsDataSource";
        mCompromisedPasswordCount = mPrefService.getInteger(Pref.LOCAL_BREACHED_CREDENTIALS_COUNT);
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
        mObserver = null;
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
        updateState();
    }

    @Override
    public void onSavedPasswordsChanged(int count) {
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
}
