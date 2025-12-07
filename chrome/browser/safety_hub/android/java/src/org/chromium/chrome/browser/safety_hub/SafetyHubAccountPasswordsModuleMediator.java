// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import android.content.Context;
import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.safety_hub.SafetyHubAccountPasswordsDataSource.ModuleType;
import org.chromium.chrome.browser.safety_hub.SafetyHubModuleMediator.ModuleOption;
import org.chromium.chrome.browser.safety_hub.SafetyHubModuleMediator.ModuleState;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Mediator for the Safety Hub password module. Populates the {@link SafetyHubExpandablePreference}
 * with the user's passwords state, including compromised, weak and reused. It gets notified of
 * changes of passwords and their state by {@link SafetyHubAccountPasswordsDataSource}, and updates
 * the preference to reflect these.
 */
@NullMarked
public class SafetyHubAccountPasswordsModuleMediator
        implements SafetyHubModuleMediator, SafetyHubAccountPasswordsDataSource.Observer {
    private final SafetyHubExpandablePreference mPreference;
    private final SafetyHubModuleMediatorDelegate mMediatorDelegate;
    private final SafetyHubModuleDelegate mModuleDelegate;
    private final PropertyModel mModel;

    private final SafetyHubAccountPasswordsDataSource mAccountPasswordsDataSource;
    private @Nullable SafetyHubModuleHelper mModuleHelper;

    SafetyHubAccountPasswordsModuleMediator(
            SafetyHubExpandablePreference preference,
            SafetyHubAccountPasswordsDataSource accountPasswordsDataSource,
            SafetyHubModuleMediatorDelegate mediatorDelegate,
            SafetyHubModuleDelegate moduleDelegate) {
        mPreference = preference;
        mAccountPasswordsDataSource = accountPasswordsDataSource;
        mMediatorDelegate = mediatorDelegate;
        mModuleDelegate = moduleDelegate;
        mModel = new PropertyModel.Builder(SafetyHubModuleProperties.ALL_KEYS).build();
    }

    @Override
    public void setUpModule() {
        mModel.set(SafetyHubModuleProperties.IS_VISIBLE, true);
        PropertyModelChangeProcessor.create(
                mModel, mPreference, SafetyHubModuleViewBinder::bindProperties);

        mAccountPasswordsDataSource.addObserver(this);
        mAccountPasswordsDataSource.setUp();
    }

    @Override
    public void destroy() {
        if (mAccountPasswordsDataSource != null) {
            mAccountPasswordsDataSource.destroy();
        }
        mModuleHelper = null;
    }

    @Override
    public void updateModule() {
        mAccountPasswordsDataSource.updateState();
    }

    private SafetyHubModuleHelper getModuleHelper(@ModuleType int moduleType) {
        Context context = mPreference.getContext();
        switch (moduleType) {
            case ModuleType.SIGNED_OUT:
                return new SafetyHubAccountPasswordsSignedOutModuleHelper(context, mModuleDelegate);
            case ModuleType.UNAVAILABLE_PASSWORDS:
                return new SafetyHubUnavailablePasswordsModuleHelper(
                        context,
                        mModuleDelegate,
                        /* unavailableAccountPasswords= */ true,
                        /* unavailableLocalPasswords= */ false);
            case ModuleType.NO_SAVED_PASSWORDS:
                return new SafetyHubNoSavedPasswordsModuleHelper(
                        context,
                        mModuleDelegate,
                        /* noAccountPasswords= */ true,
                        /* noLocalPasswords= */ false);
            case ModuleType.HAS_COMPROMISED_PASSWORDS:
                return new SafetyHubCompromisedPasswordsModuleHelper(
                        context,
                        mModuleDelegate,
                        mAccountPasswordsDataSource.getCompromisedPasswordCount(),
                        /* localCompromisedPasswordsCount= */ 0,
                        /* unifiedModule= */ false);
            case ModuleType.NO_COMPROMISED_PASSWORDS:
                return new SafetyHubNoCompromisedPasswordsModuleHelper(
                        context,
                        mModuleDelegate,
                        mAccountPasswordsDataSource.getAccountEmail(),
                        /* unifiedModule= */ false);
            case ModuleType.HAS_WEAK_PASSWORDS:
                return new SafetyHubWeakPasswordsModuleHelper(
                        context,
                        mModuleDelegate,
                        mAccountPasswordsDataSource.getWeakPasswordCount(),
                        /* localWeakPasswordsCount= */ 0,
                        /* unifiedModule= */ false);
            case ModuleType.HAS_REUSED_PASSWORDS:
                return new SafetyHubReusedPasswordsModuleHelper(
                        context,
                        mModuleDelegate,
                        mAccountPasswordsDataSource.getReusedPasswordCount(),
                        /* localReusedPasswordsCount= */ 0,
                        /* unifiedModule= */ false);
            case ModuleType.UNAVAILABLE_COMPROMISED_NO_WEAK_REUSED_PASSWORDS:
                return new SafetyHubUnavailableAccountCompromisedPasswordsModuleHelper(
                        context, mModuleDelegate);
            default:
                throw new IllegalArgumentException();
        }
    }

    private void updateModule(@ModuleType int moduleType) {
        mModuleHelper = getModuleHelper(moduleType);

        mModel.set(SafetyHubModuleProperties.TITLE, mModuleHelper.getTitle());
        mModel.set(SafetyHubModuleProperties.SUMMARY, mModuleHelper.getSummary());
        mModel.set(
                SafetyHubModuleProperties.PRIMARY_BUTTON_TEXT,
                mModuleHelper.getPrimaryButtonText());
        mModel.set(
                SafetyHubModuleProperties.SECONDARY_BUTTON_TEXT,
                mModuleHelper.getSecondaryButtonText());
        mModel.set(
                SafetyHubModuleProperties.PRIMARY_BUTTON_LISTENER,
                mModuleHelper.getPrimaryButtonListener());
        mModel.set(
                SafetyHubModuleProperties.SECONDARY_BUTTON_LISTENER,
                mModuleHelper.getSecondaryButtonListener());

        if (isManaged()) {
            overridePreferenceForManaged();
        }

        mModel.set(SafetyHubModuleProperties.ORDER, getOrder());
        mModel.set(SafetyHubModuleProperties.ICON, getIcon(mPreference.getContext()));
    }

    @Override
    public void setExpandState(boolean expanded) {
        mModel.set(SafetyHubModuleProperties.IS_EXPANDED, expanded);
    }

    @Override
    public @ModuleState int getModuleState() {
        if (mModuleHelper == null) {
            return ModuleState.UNAVAILABLE;
        }

        return mModuleHelper.getModuleState();
    }

    @Override
    public @ModuleOption int getOption() {
        return ModuleOption.ACCOUNT_PASSWORDS;
    }

    @Override
    public boolean isManaged() {
        return mAccountPasswordsDataSource.isManaged();
    }

    public void triggerNewCredentialFetch() {
        mAccountPasswordsDataSource.triggerNewCredentialFetch();
    }

    @Override
    public void accountPasswordsStateChanged(@ModuleType int moduleType) {
        updateModule(moduleType);
        mMediatorDelegate.onUpdateNeeded();
    }

    // Overrides the summary and primary button fields of `preference` if passwords are controlled
    // by a policy.
    private void overridePreferenceForManaged() {
        assert isManaged();
        mPreference.setSummary(
                mPreference
                        .getContext()
                        .getString(R.string.safety_hub_no_passwords_summary_managed));
        String primaryButtonText = mPreference.getPrimaryButtonText();
        View.OnClickListener primaryButtonListener = mPreference.getPrimaryButtonClickListener();
        if (primaryButtonText != null) {
            assert mPreference.getSecondaryButtonText() == null;
            mPreference.setSecondaryButtonText(primaryButtonText);
            mPreference.setSecondaryButtonClickListener(primaryButtonListener);
            mPreference.setPrimaryButtonText(null);
            mPreference.setPrimaryButtonClickListener(null);
        }
    }
}
