// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import android.content.Context;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.safety_hub.SafetyHubModuleMediator.ModuleOption;
import org.chromium.chrome.browser.safety_hub.SafetyHubModuleMediator.ModuleState;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Mediator for the Safety Hub passwords module. Populates the {@link SafetyHubExpandablePreference}
 * with the user's local and account passwords state, including compromised, weak and reused.
 */
@NullMarked
public class SafetyHubPasswordsModuleMediator
        implements SafetyHubModuleMediator,
                SafetyHubAccountPasswordsDataSource.Observer,
                SafetyHubLocalPasswordsDataSource.Observer {
    private final SafetyHubExpandablePreference mPreference;
    private final SafetyHubModuleMediatorDelegate mMediatorDelegate;
    private final SafetyHubModuleDelegate mModuleDelegate;
    private final PropertyModel mModel;

    private SafetyHubAccountPasswordsDataSource mAccountPasswordsDataSource;
    private SafetyHubLocalPasswordsDataSource mLocalPasswordsDataSource;
    private @Nullable SafetyHubModuleHelper mModuleHelper;

    private boolean mAccountPasswordsReturned;
    private boolean mLocalPasswordsReturned;

    SafetyHubPasswordsModuleMediator(
            SafetyHubExpandablePreference preference,
            SafetyHubAccountPasswordsDataSource accountPasswordsDataSource,
            SafetyHubLocalPasswordsDataSource localPasswordsDataSource,
            SafetyHubModuleMediatorDelegate mediatorDelegate,
            SafetyHubModuleDelegate moduleDelegate) {
        mPreference = preference;
        mAccountPasswordsDataSource = accountPasswordsDataSource;
        mLocalPasswordsDataSource = localPasswordsDataSource;
        mMediatorDelegate = mediatorDelegate;
        mModuleDelegate = moduleDelegate;
        mModel = new PropertyModel.Builder(SafetyHubModuleProperties.ALL_KEYS).build();
    }

    @Override
    public void setUpModule() {
        assert ChromeFeatureList.isEnabled(ChromeFeatureList.SAFETY_HUB_LOCAL_PASSWORDS_MODULE);

        mModel.set(SafetyHubModuleProperties.IS_VISIBLE, true);
        PropertyModelChangeProcessor.create(
                mModel, mPreference, SafetyHubModuleViewBinder::bindProperties);

        mAccountPasswordsDataSource.addObserver(this);
        mLocalPasswordsDataSource.addObserver(this);
        mAccountPasswordsDataSource.setUp();
        mLocalPasswordsDataSource.setUp();

        // TODO(crbug.com/407927786): Add loading UI if check is triggered and trigger account
        // password checkup.
        mLocalPasswordsDataSource.maybeTriggerPasswordCheckup();
    }

    @Override
    public void destroy() {
        mAccountPasswordsDataSource.destroy();
        mLocalPasswordsDataSource.destroy();
        mModuleHelper = null;
    }

    @Override
    public void updateModule() {
        mAccountPasswordsReturned = false;
        mLocalPasswordsReturned = false;
        mAccountPasswordsDataSource.updateState();
        mLocalPasswordsDataSource.updateState();
    }

    private SafetyHubModuleHelper getModuleHelper(
            @SafetyHubAccountPasswordsDataSource.ModuleType int accountModuleType,
            @SafetyHubLocalPasswordsDataSource.ModuleType int localModuleType) {
        Context context = mPreference.getContext();

        // TODO(crbug.com/407930886): Add all states for account and local passwords.
        if (accountModuleType
                        == SafetyHubAccountPasswordsDataSource.ModuleType.HAS_COMPROMISED_PASSWORDS
                || localModuleType
                        == SafetyHubLocalPasswordsDataSource.ModuleType.HAS_COMPROMISED_PASSWORDS) {
            return new SafetyHubCompromisedPasswordsModuleHelper(
                    context,
                    mModuleDelegate,
                    mAccountPasswordsDataSource.getCompromisedPasswordCount(),
                    mLocalPasswordsDataSource.getCompromisedPasswordCount(),
                    /* unifiedModule= */ true);
        }

        if (accountModuleType
                        == SafetyHubAccountPasswordsDataSource.ModuleType.NO_COMPROMISED_PASSWORDS
                && localModuleType
                        == SafetyHubLocalPasswordsDataSource.ModuleType.NO_COMPROMISED_PASSWORDS) {
            return new SafetyHubNoCompromisedPasswordsModuleHelper(
                    context,
                    mModuleDelegate,
                    mAccountPasswordsDataSource.getAccountEmail(),
                    /* unifiedModule= */ true);
        }

        return new SafetyHubAccountPasswordsUnavailableAllPasswordsModuleHelper(
                context, mModuleDelegate);
    }

    private void updateModule(
            @SafetyHubAccountPasswordsDataSource.ModuleType int accountModuleType,
            @SafetyHubLocalPasswordsDataSource.ModuleType int localModuleType) {
        mModuleHelper = getModuleHelper(accountModuleType, localModuleType);

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
        if (mModuleHelper == null) {
            return ModuleState.UNAVAILABLE;
        }
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

    @Override
    public void accountPasswordsStateChanged(
            @SafetyHubAccountPasswordsDataSource.ModuleType int moduleType) {
        mAccountPasswordsReturned = true;
        maybeUpdateModule();
    }

    @Override
    public void localPasswordsStateChanged(
            @SafetyHubLocalPasswordsDataSource.ModuleType int moduleType) {
        mLocalPasswordsReturned = true;
        maybeUpdateModule();
    }

    private void maybeUpdateModule() {
        if (!mAccountPasswordsReturned || !mLocalPasswordsReturned) {
            return;
        }

        updateModule(
                mAccountPasswordsDataSource.getModuleType(),
                mLocalPasswordsDataSource.getModuleType());
        mMediatorDelegate.onUpdateNeeded();
    }
}
