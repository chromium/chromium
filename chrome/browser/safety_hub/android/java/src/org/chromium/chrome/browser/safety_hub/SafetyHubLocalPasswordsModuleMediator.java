// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import android.content.Context;
import android.view.View;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.safety_hub.SafetyHubLocalPasswordsDataSource.ModuleType;
import org.chromium.chrome.browser.safety_hub.SafetyHubModuleMediator.ModuleOption;
import org.chromium.chrome.browser.safety_hub.SafetyHubModuleMediator.ModuleState;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Mediator for the Safety Hub local password module. Populates the {@link
 * SafetyHubExpandablePreference} with the user's passwords state, including compromised, weak and
 * reused. It gets notified of changes of local passwords and their state by {@link
 * SafetyHubLocalPasswordsDataSource}, and updates the preference to reflect these.
 */
public class SafetyHubLocalPasswordsModuleMediator
        implements SafetyHubModuleMediator, SafetyHubLocalPasswordsDataSource.Observer {
    private final SafetyHubExpandablePreference mPreference;
    private final SafetyHubModuleMediatorDelegate mMediatorDelegate;
    private final SafetyHubModuleDelegate mModuleDelegate;

    private SafetyHubLocalPasswordsDataSource mLocalPasswordsDataSource;
    private SafetyHubModuleHelper mModuleHelper;
    private PropertyModel mModel;

    SafetyHubLocalPasswordsModuleMediator(
            SafetyHubExpandablePreference preference,
            SafetyHubLocalPasswordsDataSource localPasswordsDataSource,
            SafetyHubModuleMediatorDelegate mediatorDelegate,
            SafetyHubModuleDelegate moduleDelegate) {
        mPreference = preference;
        mLocalPasswordsDataSource = localPasswordsDataSource;
        mMediatorDelegate = mediatorDelegate;
        mModuleDelegate = moduleDelegate;
    }

    @Override
    public void setUpModule() {
        assert ChromeFeatureList.isEnabled(ChromeFeatureList.SAFETY_HUB_LOCAL_PASSWORDS_MODULE);
        mModel =
                new PropertyModel.Builder(SafetyHubModuleProperties.ALL_KEYS)
                        .with(SafetyHubModuleProperties.IS_VISIBLE, true)
                        .build();

        PropertyModelChangeProcessor.create(
                mModel, mPreference, SafetyHubModuleViewBinder::bindProperties);

        mLocalPasswordsDataSource.setObserver(this);
        mLocalPasswordsDataSource.setUp();

        if (mLocalPasswordsDataSource.maybeTriggerPasswordCheckup()) {
            mModuleHelper =
                    new SafetyHubLocalPasswordsCheckingModuleHelper(mPreference.getContext());
        }
    }

    @Override
    public void destroy() {
        if (mLocalPasswordsDataSource != null) {
            mLocalPasswordsDataSource.destroy();
            mLocalPasswordsDataSource = null;
        }
    }

    @Override
    public void updateModule() {
        if (isLoading()) {
            updatePreference();
        } else {
            mLocalPasswordsDataSource.updateState();
        }
    }

    private SafetyHubModuleHelper getModuleHelper(@ModuleType int moduleType) {
        Context context = mPreference.getContext();

        switch (moduleType) {
            case ModuleType.UNAVAILABLE_PASSWORDS:
                return new SafetyHubLocalPasswordsUnavailableAllPasswordsModuleHelper(
                        context, mModuleDelegate);
            case ModuleType.NO_SAVED_PASSWORDS:
                return new SafetyHubLocalPasswordsNoPasswordsModuleHelper(context, mModuleDelegate);
            case ModuleType.HAS_COMPROMISED_PASSWORDS:
                return new SafetyHubLocalPasswordsHasCompromisedPasswordsModuleHelper(
                        context,
                        mModuleDelegate,
                        mLocalPasswordsDataSource.getCompromisedPasswordCount());
            case ModuleType.NO_COMPROMISED_PASSWORDS:
                return new SafetyHubLocalPasswordsNoCompromisedPasswordsModuleHelper(
                        context, mModuleDelegate);
            case ModuleType.HAS_WEAK_PASSWORDS:
                return new SafetyHubLocalPasswordsHasWeakPasswordsModuleHelper(
                        context, mModuleDelegate, mLocalPasswordsDataSource.getWeakPasswordCount());
            case ModuleType.HAS_REUSED_PASSWORDS:
                return new SafetyHubLocalPasswordsHasReusedPasswordsModuleHelper(
                        context,
                        mModuleDelegate,
                        mLocalPasswordsDataSource.getReusedPasswordCount());
            default:
                throw new IllegalArgumentException();
        }
    }

    private void updateModule(@ModuleType int moduleType) {
        mModuleHelper = getModuleHelper(moduleType);
        updatePreference();
    }

    private void updatePreference() {
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
        mModel.set(SafetyHubModuleProperties.HAS_PROGRESS_BAR, isLoading());
    }

    @Override
    public void setExpandState(boolean expanded) {
        mModel.set(SafetyHubModuleProperties.IS_EXPANDED, expanded);
    }

    @Override
    public @ModuleState int getModuleState() {
        // TODO(crbug.com/388788969): Decide on a proper state while the module is still loading.
        if (mModuleHelper == null) {
            return ModuleState.UNAVAILABLE;
        }
        return mModuleHelper.getModuleState();
    }

    @Override
    public @ModuleOption int getOption() {
        return ModuleOption.LOCAL_PASSWORDS;
    }

    @Override
    public boolean isManaged() {
        return mLocalPasswordsDataSource.isManaged();
    }

    public void triggerNewCredentialFetch() {
        mLocalPasswordsDataSource.triggerNewCredentialFetch();
    }

    @Override
    public void stateChanged(@ModuleType int moduleType) {
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
