// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import android.content.Context;
import android.view.View;

import org.chromium.base.CallbackController;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
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
@NullMarked
public class SafetyHubLocalPasswordsModuleMediator
        implements SafetyHubModuleMediator, SafetyHubLocalPasswordsDataSource.Observer {

    private final SafetyHubExpandablePreference mPreference;
    private final SafetyHubModuleMediatorDelegate mMediatorDelegate;
    private final SafetyHubModuleDelegate mModuleDelegate;
    private final PropertyModel mModel;

    private final SafetyHubLocalPasswordsDataSource mLocalPasswordsDataSource;
    private @Nullable SafetyHubModuleHelper mModuleHelper;

    private @IndicatorState int mIndicatorState = IndicatorState.IDLE;
    // Callback when the minimum time showing the loading indicator has elapsed.
    private @Nullable CallbackController mMinLoadingCallbackController;
    // Callback when the maximum time showing the loading indicator has elapsed.
    private @Nullable CallbackController mMaxLoadingCallbackController;

    private boolean mStateChangedCalled;
    private boolean mOrderUpdated;

    SafetyHubLocalPasswordsModuleMediator(
            SafetyHubExpandablePreference preference,
            SafetyHubLocalPasswordsDataSource localPasswordsDataSource,
            SafetyHubModuleMediatorDelegate mediatorDelegate,
            SafetyHubModuleDelegate moduleDelegate) {
        mPreference = preference;
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

        mLocalPasswordsDataSource.addObserver(this);
        mLocalPasswordsDataSource.setUp();

        // TODO(crbug.com/407931779): Remove triggering checkup along with loading UI once this
        // module is only used on the passwords subpage.
        if (mLocalPasswordsDataSource.maybeTriggerPasswordCheckup()) {
            mIndicatorState = IndicatorState.SHOWING_INDICATOR;
            mModuleHelper =
                    new SafetyHubPasswordsCheckingModuleHelper(
                            mPreference.getContext(), /* onlyLoadingLocalPasswords= */ true);

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

        if (mLocalPasswordsDataSource != null) {
            mLocalPasswordsDataSource.destroy();
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

    private void maybeCancelMaxLoadingCallback() {
        if (mMaxLoadingCallbackController != null) {
            mMaxLoadingCallbackController.destroy();
            mMaxLoadingCallbackController = null;
        }
    }

    private void onMinimumLoadingTimeElapsed() {
        mIndicatorState = IndicatorState.WAITING_FOR_RESULTS;
        if (mStateChangedCalled) {
            mLocalPasswordsDataSource.updateState();
        }
    }

    private void onMaxLoadingTimeElapsed() {
        // The callback that triggers this method is canceled if any result is returned. As such,
        // the UI will always be in the loading state when this method is ran.
        assert isLoading();

        // As the max loading time has elapsed, then show the user that no checkup is possible to be
        // performed at this time.
        localPasswordsStateChanged(ModuleType.UNAVAILABLE_PASSWORDS);
    }

    private SafetyHubModuleHelper getModuleHelper(@ModuleType int moduleType) {
        Context context = mPreference.getContext();

        switch (moduleType) {
            case ModuleType.UNAVAILABLE_PASSWORDS:
                return new SafetyHubUnavailablePasswordsModuleHelper(
                        context,
                        mModuleDelegate,
                        /* unavailableAccountPasswords= */ false,
                        /* unavailableLocalPasswords= */ true);
            case ModuleType.NO_SAVED_PASSWORDS:
                return new SafetyHubNoSavedPasswordsModuleHelper(
                        context,
                        mModuleDelegate,
                        /* noAccountPasswords= */ false,
                        /* noLocalPasswords= */ true);
            case ModuleType.HAS_COMPROMISED_PASSWORDS:
                return new SafetyHubCompromisedPasswordsModuleHelper(
                        context,
                        mModuleDelegate,
                        /* accountCompromisedPasswordsCount= */ 0,
                        mLocalPasswordsDataSource.getCompromisedPasswordCount(),
                        /* unifiedModule= */ false);
            case ModuleType.NO_COMPROMISED_PASSWORDS:
                return new SafetyHubNoCompromisedPasswordsModuleHelper(
                        context, mModuleDelegate, /* account= */ null, /* unifiedModule= */ false);
            case ModuleType.HAS_WEAK_PASSWORDS:
                return new SafetyHubWeakPasswordsModuleHelper(
                        context,
                        mModuleDelegate,
                        /* accountWeakPasswordsCount= */ 0,
                        mLocalPasswordsDataSource.getWeakPasswordCount(),
                        /* unifiedModule= */ false);
            case ModuleType.HAS_REUSED_PASSWORDS:
                return new SafetyHubReusedPasswordsModuleHelper(
                        context,
                        mModuleDelegate,
                        /* accountReusedPasswordsCount= */ 0,
                        mLocalPasswordsDataSource.getReusedPasswordCount(),
                        /* unifiedModule= */ false);
            default:
                throw new IllegalArgumentException();
        }
    }

    private void updateModule(@ModuleType int moduleType) {
        mModuleHelper = getModuleHelper(moduleType);
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

        if (isManaged()) {
            overridePreferenceForManaged();
        }

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
    public void localPasswordsStateChanged(@ModuleType int moduleType) {
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
