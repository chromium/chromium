// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import static org.chromium.chrome.browser.safety_hub.SafetyHubMetricUtils.recordDashboardInteractions;

import android.view.View;

import org.chromium.base.ApkInfo;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.omaha.UpdateStatusProvider;
import org.chromium.chrome.browser.safety_hub.SafetyHubMetricUtils.DashboardInteractions;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Mediator for the Safety Hub update check module. Populates the {@link
 * SafetyHubExpandablePreference} with the update check state. It also listens to changes of this
 * state, and updates the preference to reflect these.
 */
@NullMarked
public class SafetyHubUpdateCheckModuleMediator
        implements SafetyHubModuleMediator, SafetyHubFetchService.Observer {
    private final SafetyHubFetchService mSafetyHubFetchService;
    private final SafetyHubExpandablePreference mPreference;
    private final SafetyHubModuleMediatorDelegate mMediatorDelegate;
    private final SafetyHubModuleDelegate mModuleDelegate;

    private PropertyModel mModel;

    private UpdateStatusProvider.@Nullable UpdateStatus mUpdateStatus;

    SafetyHubUpdateCheckModuleMediator(
            SafetyHubExpandablePreference preference,
            SafetyHubModuleMediatorDelegate delegate,
            SafetyHubModuleDelegate moduleDelegate,
            SafetyHubFetchService safetyHubFetchService) {
        mPreference = preference;
        mMediatorDelegate = delegate;
        mModuleDelegate = moduleDelegate;
        mSafetyHubFetchService = safetyHubFetchService;
    }

    @Override
    public void setUpModule() {
        mModel =
                new PropertyModel.Builder(SafetyHubModuleProperties.ALL_KEYS)
                        .with(SafetyHubModuleProperties.IS_VISIBLE, true)
                        .build();

        PropertyModelChangeProcessor.create(
                mModel, mPreference, SafetyHubModuleViewBinder::bindProperties);

        mSafetyHubFetchService.addObserver(this);
        mUpdateStatus = getUpdateStatus();
    }

    @Override
    public void destroy() {
        mSafetyHubFetchService.removeObserver(this);
    }

    @Override
    public void updateStatusChanged() {
        updateModule();
        mMediatorDelegate.onUpdateNeeded();
    }

    @Override
    public void localPasswordCountsChanged() {
        // no-op.
    }

    @Override
    public void accountPasswordCountsChanged() {
        // no-op.
    }

    @Override
    public void updateModule() {
        mUpdateStatus = getUpdateStatus();

        mModel.set(SafetyHubModuleProperties.TITLE, getTitle());
        mModel.set(SafetyHubModuleProperties.SUMMARY, getSummary());
        mModel.set(SafetyHubModuleProperties.PRIMARY_BUTTON_TEXT, getPrimaryButtonText());
        mModel.set(SafetyHubModuleProperties.SECONDARY_BUTTON_TEXT, getSecondaryButtonText());
        mModel.set(SafetyHubModuleProperties.PRIMARY_BUTTON_LISTENER, getPrimaryButtonListener());
        mModel.set(
                SafetyHubModuleProperties.SECONDARY_BUTTON_LISTENER, getSecondaryButtonListener());
        mModel.set(SafetyHubModuleProperties.ORDER, getOrder());
        mModel.set(SafetyHubModuleProperties.ICON, getIcon(mPreference.getContext()));
    }

    @Override
    public void setExpandState(boolean expanded) {
        mModel.set(SafetyHubModuleProperties.IS_EXPANDED, expanded);
    }

    @Override
    public @ModuleState int getModuleState() {
        return SafetyHubUtils.getUpdateCheckModuleState(mUpdateStatus);
    }

    @Override
    public @ModuleOption int getOption() {
        return ModuleOption.UPDATE_CHECK;
    }

    @Override
    public boolean isManaged() {
        return false;
    }

    public UpdateStatusProvider.@Nullable UpdateStatus getUpdateStatus() {
        return mSafetyHubFetchService.getUpdateStatus();
    }

    private String getTitle() {
        if (mUpdateStatus == null) {
            return mPreference.getContext().getString(R.string.safety_hub_update_unavailable_title);
        }

        switch (mUpdateStatus.updateState) {
            case UpdateStatusProvider.UpdateState.UNSUPPORTED_OS_VERSION:
                return mPreference.getContext().getString(R.string.menu_update_unsupported);
            case UpdateStatusProvider.UpdateState.UPDATE_AVAILABLE:
                return mPreference.getContext().getString(R.string.safety_check_updates_outdated);
            case UpdateStatusProvider.UpdateState.NONE:
                return mPreference.getContext().getString(R.string.safety_check_updates_updated);
            default:
                throw new IllegalArgumentException();
        }
    }

    private @Nullable String getSummary() {
        if (mUpdateStatus == null) {
            return mPreference.getContext().getString(R.string.safety_hub_unavailable_summary);
        }

        switch (mUpdateStatus.updateState) {
            case UpdateStatusProvider.UpdateState.UNSUPPORTED_OS_VERSION:
                return mPreference
                        .getContext()
                        .getString(R.string.menu_update_unsupported_summary_default);
            case UpdateStatusProvider.UpdateState.UPDATE_AVAILABLE:
                return mPreference
                        .getContext()
                        .getString(R.string.safety_hub_updates_outdated_summary);
            case UpdateStatusProvider.UpdateState.NONE:
                String currentVersion = ApkInfo.getPackageVersionName();
                if (currentVersion != null && !currentVersion.isEmpty()) {
                    return mPreference
                            .getContext()
                            .getString(R.string.safety_hub_version_summary, currentVersion);
                }
                return null;
            default:
                throw new IllegalArgumentException();
        }
    }

    private @Nullable String getPrimaryButtonText() {
        if (mUpdateStatus == null) {
            return null;
        }

        switch (mUpdateStatus.updateState) {
            case UpdateStatusProvider.UpdateState.UPDATE_AVAILABLE:
                return mPreference.getContext().getString(R.string.menu_update);
            case UpdateStatusProvider.UpdateState.UNSUPPORTED_OS_VERSION:
            case UpdateStatusProvider.UpdateState.NONE:
                return null;
            default:
                throw new IllegalArgumentException();
        }
    }

    private View.@Nullable OnClickListener getPrimaryButtonListener() {
        if (mUpdateStatus == null) {
            return null;
        }

        switch (mUpdateStatus.updateState) {
            case UpdateStatusProvider.UpdateState.UPDATE_AVAILABLE:
                return v -> {
                    mModuleDelegate.openGooglePlayStore(mPreference.getContext());
                    recordDashboardInteractions(DashboardInteractions.OPEN_PLAY_STORE);
                };
            case UpdateStatusProvider.UpdateState.UNSUPPORTED_OS_VERSION:
            case UpdateStatusProvider.UpdateState.NONE:
                return null;
            default:
                throw new IllegalArgumentException();
        }
    }

    private @Nullable String getSecondaryButtonText() {
        if (mUpdateStatus == null) {
            return mPreference.getContext().getString(R.string.safety_hub_go_to_google_play_button);
        }

        switch (mUpdateStatus.updateState) {
            case UpdateStatusProvider.UpdateState.NONE:
                return mPreference
                        .getContext()
                        .getString(R.string.safety_hub_go_to_google_play_button);
            case UpdateStatusProvider.UpdateState.UNSUPPORTED_OS_VERSION:
            case UpdateStatusProvider.UpdateState.UPDATE_AVAILABLE:
                return null;
            default:
                throw new IllegalArgumentException();
        }
    }

    private View.@Nullable OnClickListener getSecondaryButtonListener() {
        if (mUpdateStatus == null) {
            return v -> {
                mModuleDelegate.openGooglePlayStore(mPreference.getContext());
                recordDashboardInteractions(DashboardInteractions.OPEN_PLAY_STORE);
            };
        }

        switch (mUpdateStatus.updateState) {
            case UpdateStatusProvider.UpdateState.NONE:
                return v -> {
                    mModuleDelegate.openGooglePlayStore(mPreference.getContext());
                    recordDashboardInteractions(DashboardInteractions.OPEN_PLAY_STORE);
                };
            case UpdateStatusProvider.UpdateState.UNSUPPORTED_OS_VERSION:
            case UpdateStatusProvider.UpdateState.UPDATE_AVAILABLE:
                return null;
            default:
                throw new IllegalArgumentException();
        }
    }
}
