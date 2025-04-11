// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.safety_hub.SafetyHubModuleMediator.ModuleOption;
import org.chromium.chrome.browser.safety_hub.SafetyHubModuleMediator.ModuleState;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Mediator for the Safety Hub passwords module. Populates the {@link SafetyHubExpandablePreference}
 * with the user's local and account passwords state, including compromised, weak and reused.
 */
public class SafetyHubPasswordsModuleMediator implements SafetyHubModuleMediator {
    private final SafetyHubExpandablePreference mPreference;
    private final SafetyHubModuleDelegate mModuleDelegate;

    private SafetyHubModuleHelper mModuleHelper;
    private PropertyModel mModel;

    SafetyHubPasswordsModuleMediator(
            SafetyHubExpandablePreference preference, SafetyHubModuleDelegate moduleDelegate) {
        mPreference = preference;
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

        // TODO(crbug.com/407748843): Update the preferences according to state of local and account
        // passwords.
        mModuleHelper =
                new SafetyHubAccountPasswordsUnavailableAllPasswordsModuleHelper(
                        mPreference.getContext(), mModuleDelegate);
    }

    @Override
    public void destroy() {}

    @Override
    public void updateModule() {
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

        mModel.set(SafetyHubModuleProperties.ORDER, getOrder());
        mModel.set(SafetyHubModuleProperties.ICON, getIcon(mPreference.getContext()));
    }

    @Override
    public void setExpandState(boolean expanded) {
        mModel.set(SafetyHubModuleProperties.IS_EXPANDED, expanded);
    }

    @Override
    public @ModuleState int getModuleState() {
        return mModuleHelper.getModuleState();
    }

    @Override
    public @ModuleOption int getOption() {
        return ModuleOption.UNIFIED_PASSWORDS;
    }

    @Override
    public boolean isManaged() {
        return false;
    }
}
