// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import android.content.Context;

import org.chromium.base.CallbackController;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
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

    private final SafetyHubAccountPasswordsDataSource mAccountPasswordsDataSource;
    private final SafetyHubLocalPasswordsDataSource mLocalPasswordsDataSource;

    private @Nullable SafetyHubModuleHelper mModuleHelper;

    private boolean mAccountPasswordsReturned;
    private boolean mLocalPasswordsReturned;

    private @IndicatorState int mIndicatorState = IndicatorState.IDLE;
    // Callback when the minimum time showing the loading indicator has elapsed.
    private @Nullable CallbackController mMinLoadingCallbackController;
    // Callback when the maximum time showing the loading indicator has elapsed.
    private @Nullable CallbackController mMaxLoadingCallbackController;

    private boolean mStateChangedCalled;
    private boolean mOrderUpdated;

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

        boolean accountPasswordsCheckupTriggered =
                mAccountPasswordsDataSource.maybeTriggerPasswordCheckup();
        boolean localPasswordsCheckupTriggered =
                mLocalPasswordsDataSource.maybeTriggerPasswordCheckup();

        if (accountPasswordsCheckupTriggered || localPasswordsCheckupTriggered) {
            mIndicatorState = IndicatorState.SHOWING_INDICATOR;
            mModuleHelper =
                    new SafetyHubPasswordsCheckingModuleHelper(
                            mPreference.getContext(), /* onlyLoadingLocalPasswords= */ false);

            mMinLoadingCallbackController = new CallbackController();
            PostTask.postDelayedTask(
                    TaskTraits.UI_DEFAULT,
                    mMinLoadingCallbackController.makeCancelable(this::onMinimumLoadingTimeElapsed),
                    LOADING_MIN_TIME_MS);

            mMaxLoadingCallbackController = new CallbackController();
            PostTask.postDelayedTask(
                    TaskTraits.UI_DEFAULT,
                    mMaxLoadingCallbackController.makeCancelable(this::onMaxLoadingTimeElapsed),
                    getLoadingMaxTime());
        }
    }

    @Override
    public void destroy() {
        if (mMinLoadingCallbackController != null) {
            mMinLoadingCallbackController.destroy();
            mMinLoadingCallbackController = null;
        }
        maybeCancelMaxLoadingCallback();

        if (mAccountPasswordsDataSource != null) {
            mAccountPasswordsDataSource.destroy();
        }
        if (mLocalPasswordsDataSource != null) {
            mLocalPasswordsDataSource.destroy();
        }
    }

    @Override
    public void updateModule() {
        if (isLoading()) {
            updatePreference();
            return;
        }
        mAccountPasswordsReturned = false;
        mLocalPasswordsReturned = false;
        mAccountPasswordsDataSource.updateState();
        mLocalPasswordsDataSource.updateState();
    }

    private void maybeCancelMaxLoadingCallback() {
        if (mMaxLoadingCallbackController != null) {
            mMaxLoadingCallbackController.destroy();
            mMaxLoadingCallbackController = null;
        }
    }

    private void onMinimumLoadingTimeElapsed() {
        mIndicatorState = IndicatorState.WAITING_FOR_RESULTS;
        if (mStateChangedCalled) {
            mAccountPasswordsDataSource.updateState();
            mLocalPasswordsDataSource.updateState();
        }
    }

    private void onMaxLoadingTimeElapsed() {
        // The callback that triggers this method is canceled if any result is returned. As such,
        // the UI will always be in the loading state when this method is ran.
        assert isLoading();

        // As the max loading time has elapsed, then show the user that no checkup is possible to be
        // performed at this time.
        localPasswordsStateChanged(
                SafetyHubLocalPasswordsDataSource.ModuleType.UNAVAILABLE_PASSWORDS);
        accountPasswordsStateChanged(
                SafetyHubAccountPasswordsDataSource.ModuleType.UNAVAILABLE_PASSWORDS);
    }

    private SafetyHubModuleHelper getModuleHelper(
            @SafetyHubAccountPasswordsDataSource.ModuleType int accountModuleType,
            @SafetyHubLocalPasswordsDataSource.ModuleType int localModuleType) {
        Context context = mPreference.getContext();

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
                        == SafetyHubAccountPasswordsDataSource.ModuleType.UNAVAILABLE_PASSWORDS
                || accountModuleType
                        == SafetyHubAccountPasswordsDataSource.ModuleType
                                .UNAVAILABLE_COMPROMISED_NO_WEAK_REUSED_PASSWORDS
                || localModuleType
                        == SafetyHubLocalPasswordsDataSource.ModuleType.UNAVAILABLE_PASSWORDS) {
            return new SafetyHubUnavailablePasswordsModuleHelper(
                    context,
                    mModuleDelegate,
                    /* unavailableAccountPasswords= */ true,
                    /* unavailableLocalPasswords= */ true);
        }

        if (accountModuleType == SafetyHubAccountPasswordsDataSource.ModuleType.HAS_REUSED_PASSWORDS
                || localModuleType
                        == SafetyHubLocalPasswordsDataSource.ModuleType.HAS_REUSED_PASSWORDS) {
            return new SafetyHubReusedPasswordsModuleHelper(
                    context,
                    mModuleDelegate,
                    mAccountPasswordsDataSource.getReusedPasswordCount(),
                    mLocalPasswordsDataSource.getReusedPasswordCount(),
                    /* unifiedModule= */ true);
        }

        if (accountModuleType == SafetyHubAccountPasswordsDataSource.ModuleType.HAS_WEAK_PASSWORDS
                || localModuleType
                        == SafetyHubLocalPasswordsDataSource.ModuleType.HAS_WEAK_PASSWORDS) {
            return new SafetyHubWeakPasswordsModuleHelper(
                    context,
                    mModuleDelegate,
                    mAccountPasswordsDataSource.getWeakPasswordCount(),
                    mLocalPasswordsDataSource.getWeakPasswordCount(),
                    /* unifiedModule= */ true);
        }

        if (accountModuleType
                        == SafetyHubAccountPasswordsDataSource.ModuleType.NO_COMPROMISED_PASSWORDS
                || localModuleType
                        == SafetyHubLocalPasswordsDataSource.ModuleType.NO_COMPROMISED_PASSWORDS) {
            return new SafetyHubNoCompromisedPasswordsModuleHelper(
                    context,
                    mModuleDelegate,
                    mAccountPasswordsDataSource.getAccountEmail(),
                    /* unifiedModule= */ true);
        }

        // By reaching this point, all other states have been exhausted.
        assert (accountModuleType
                                == SafetyHubAccountPasswordsDataSource.ModuleType.NO_SAVED_PASSWORDS
                        || accountModuleType
                                == SafetyHubAccountPasswordsDataSource.ModuleType.SIGNED_OUT)
                && localModuleType
                        == SafetyHubLocalPasswordsDataSource.ModuleType.NO_SAVED_PASSWORDS;

        return new SafetyHubNoSavedPasswordsModuleHelper(
                context,
                mModuleDelegate,
                /* noAccountPasswords= */ true,
                /* noLocalPasswords= */ true);
    }

    private void updateModule(
            @SafetyHubAccountPasswordsDataSource.ModuleType int accountModuleType,
            @SafetyHubLocalPasswordsDataSource.ModuleType int localModuleType) {
        mModuleHelper = getModuleHelper(accountModuleType, localModuleType);
        updatePreference();
    }

    private void updatePreference() {
        if (mModuleHelper == null) {
            return;
        }
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

        mModel.set(SafetyHubModuleProperties.ICON, getIcon(mPreference.getContext()));
        mModel.set(SafetyHubModuleProperties.HAS_PROGRESS_BAR, isLoading());

        // Only update the order one time to avoid the module jumping.
        if (!mOrderUpdated) {
            mOrderUpdated = true;
            mModel.set(SafetyHubModuleProperties.ORDER, getOrder());
        }
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

        mStateChangedCalled = true;
        // As a result is available, cancel the callback for when the maximum time showing the
        // loading indicator has elapsed.
        maybeCancelMaxLoadingCallback();

        // Loading indicator has not been shown long enough, delay showing the results until a later
        // date.
        if (mIndicatorState == IndicatorState.SHOWING_INDICATOR) {
            return;
        }
        mIndicatorState = IndicatorState.IDLE;

        updateModule(
                mAccountPasswordsDataSource.getModuleType(),
                mLocalPasswordsDataSource.getModuleType());
        mMediatorDelegate.onUpdateNeeded();
    }
}
