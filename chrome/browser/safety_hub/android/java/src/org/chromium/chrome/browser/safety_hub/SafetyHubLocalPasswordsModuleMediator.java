// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

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

    private SafetyHubLocalPasswordsDataSource mLocalPasswordsDataSource;
    private PropertyModel mModel;

    SafetyHubLocalPasswordsModuleMediator(
            SafetyHubExpandablePreference preference,
            SafetyHubLocalPasswordsDataSource localPasswordsDataSource,
            SafetyHubModuleMediatorDelegate mediatorDelegate) {
        mPreference = preference;
        mLocalPasswordsDataSource = localPasswordsDataSource;
        mMediatorDelegate = mediatorDelegate;
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
        // TODO(crbug.com/388788969): Show the loading indicator if the checkup is ran.
        mLocalPasswordsDataSource.maybeTriggerPasswordCheckup();
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
        mLocalPasswordsDataSource.updateState();
    }

    // TODO(crbug.com/388788969): Set all other model properties based on the value of `moduleType`.
    @SuppressWarnings("unused")
    private void updateModule(@ModuleType int moduleType) {
        mModel.set(SafetyHubModuleProperties.ORDER, getOrder());
        mModel.set(SafetyHubModuleProperties.ICON, getIcon(mPreference.getContext()));
    }

    @Override
    public void setExpandState(boolean expanded) {
        mModel.set(SafetyHubModuleProperties.IS_EXPANDED, expanded);
    }

    @Override
    public @ModuleState int getModuleState() {
        return ModuleState.UNAVAILABLE;
    }

    @Override
    public @ModuleOption int getOption() {
        return ModuleOption.LOCAL_PASSWORDS;
    }

    @Override
    public boolean isManaged() {
        return mLocalPasswordsDataSource.isManaged();
    }

    @Override
    public void stateChanged(@ModuleType int moduleType) {
        updateModule(moduleType);
        mMediatorDelegate.onUpdateNeeded();
    }
}
