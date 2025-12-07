// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import static org.chromium.chrome.browser.safety_hub.SafetyHubMetricUtils.recordDashboardInteractions;

import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingState;
import org.chromium.chrome.browser.safe_browsing.settings.SafeBrowsingSettingsFragment;
import org.chromium.chrome.browser.safety_hub.SafetyHubMetricUtils.DashboardInteractions;
import org.chromium.chrome.browser.safety_hub.SafetyHubModuleMediator.ModuleOption;
import org.chromium.chrome.browser.safety_hub.SafetyHubModuleMediator.ModuleState;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Mediator for the Safety Hub safe browsing module. Populates the {@link
 * SafetyHubExpandablePreference} with the browser's safe browsing state.
 */
@NullMarked
public class SafetyHubSafeBrowsingModuleMediator implements SafetyHubModuleMediator {
    private final Profile mProfile;
    private final SafetyHubExpandablePreference mPreference;
    private final SafetyHubModuleMediatorDelegate mDelegate;

    private PropertyModel mModel;
    private @SafeBrowsingState int mSafeBrowsingState;

    SafetyHubSafeBrowsingModuleMediator(
            SafetyHubExpandablePreference preference,
            SafetyHubModuleMediatorDelegate delegate,
            Profile profile) {
        mPreference = preference;
        mDelegate = delegate;
        mProfile = profile;
    }

    @Override
    public void setUpModule() {
        mModel =
                new PropertyModel.Builder(SafetyHubModuleProperties.ALL_KEYS)
                        .with(SafetyHubModuleProperties.IS_VISIBLE, true)
                        .build();

        PropertyModelChangeProcessor.create(
                mModel, mPreference, SafetyHubModuleViewBinder::bindProperties);
        mSafeBrowsingState = getSafeBrowsingState();
    }

    @Override
    public void destroy() {
        // no-op.
    }

    @Override
    public void updateModule() {
        mSafeBrowsingState = getSafeBrowsingState();

        mModel.set(SafetyHubModuleProperties.TITLE, getTitle());
        mModel.set(SafetyHubModuleProperties.SUMMARY, getSummary());
        mModel.set(SafetyHubModuleProperties.PRIMARY_BUTTON_TEXT, getPrimaryButtonText());
        mModel.set(SafetyHubModuleProperties.PRIMARY_BUTTON_LISTENER, getPrimaryButtonListener());
        mModel.set(SafetyHubModuleProperties.SECONDARY_BUTTON_TEXT, getSecondaryButtonText());
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
        return SafetyHubUtils.getSafeBrowsingModuleState(mSafeBrowsingState);
    }

    @Override
    public @ModuleOption int getOption() {
        return ModuleOption.SAFE_BROWSING;
    }

    @Override
    public boolean isManaged() {
        return SafetyHubUtils.isSafeBrowsingManaged(mProfile);
    }

    public @SafeBrowsingState int getSafeBrowsingState() {
        return SafetyHubUtils.getSafeBrowsingState(mProfile);
    }

    private @Nullable String getTitle() {
        switch (mSafeBrowsingState) {
            case SafeBrowsingState.STANDARD_PROTECTION:
                return mPreference
                        .getContext()
                        .getString(R.string.safety_hub_safe_browsing_on_title);
            case SafeBrowsingState.ENHANCED_PROTECTION:
                return mPreference
                        .getContext()
                        .getString(R.string.safety_hub_safe_browsing_enhanced_title);
            case SafeBrowsingState.NO_SAFE_BROWSING:
                return mPreference
                        .getContext()
                        .getString(R.string.prefs_safe_browsing_no_protection_summary);
            default:
                assert false : "Should not be reached.";
                return null;
        }
    }

    private @Nullable String getSummary() {
        switch (mSafeBrowsingState) {
            case SafeBrowsingState.STANDARD_PROTECTION:
                return mPreference
                        .getContext()
                        .getString(
                                isManaged()
                                        ? R.string.safety_hub_safe_browsing_on_summary_managed
                                        : R.string.safety_hub_safe_browsing_on_summary);
            case SafeBrowsingState.ENHANCED_PROTECTION:
                return mPreference
                        .getContext()
                        .getString(
                                isManaged()
                                        ? R.string.safety_hub_safe_browsing_enhanced_summary_managed
                                        : R.string.safety_hub_safe_browsing_enhanced_summary);
            case SafeBrowsingState.NO_SAFE_BROWSING:
                return mPreference
                        .getContext()
                        .getString(
                                isManaged()
                                        ? R.string.safety_hub_safe_browsing_off_summary_managed
                                        : R.string.safety_hub_safe_browsing_off_summary);
            default:
                assert false : "Should not be reached.";
                return null;
        }
    }

    private @Nullable String getPrimaryButtonText() {
        if (mSafeBrowsingState == SafeBrowsingState.NO_SAFE_BROWSING && !isManaged()) {
            return mPreference.getContext().getString(R.string.safety_hub_turn_on_button);
        }

        return null;
    }

    private View.@Nullable OnClickListener getPrimaryButtonListener() {
        if (mSafeBrowsingState == SafeBrowsingState.NO_SAFE_BROWSING && !isManaged()) {
            return v -> {
                mDelegate.startSettingsForModule(SafeBrowsingSettingsFragment.class);
                recordDashboardInteractions(DashboardInteractions.GO_TO_SAFE_BROWSING_SETTINGS);
            };
        }

        return null;
    }

    private @Nullable String getSecondaryButtonText() {
        if (mSafeBrowsingState == SafeBrowsingState.NO_SAFE_BROWSING || isManaged()) {
            return null;
        }

        return mPreference
                .getContext()
                .getString(R.string.safety_hub_go_to_security_settings_button);
    }

    private View.@Nullable OnClickListener getSecondaryButtonListener() {
        if (mSafeBrowsingState == SafeBrowsingState.NO_SAFE_BROWSING || isManaged()) {
            return null;
        }

        return v -> {
            mDelegate.startSettingsForModule(SafeBrowsingSettingsFragment.class);
            recordDashboardInteractions(DashboardInteractions.GO_TO_SAFE_BROWSING_SETTINGS);
        };
    }
}
