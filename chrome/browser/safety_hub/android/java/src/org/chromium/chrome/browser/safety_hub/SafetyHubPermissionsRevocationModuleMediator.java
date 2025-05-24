// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import static org.chromium.chrome.browser.safety_hub.SafetyHubMetricUtils.maybeRecordAbusiveNotificationRevokedInteraction;
import static org.chromium.chrome.browser.safety_hub.SafetyHubMetricUtils.recordRevokedPermissionsInteraction;

import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.safety_hub.SafetyHubMetricUtils.PermissionsModuleInteractions;
import org.chromium.chrome.browser.safety_hub.SafetyHubModuleMediator.ModuleOption;
import org.chromium.chrome.browser.safety_hub.SafetyHubModuleMediator.ModuleState;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.site_settings.SiteSettings;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Mediator for the Safety Hub permissions revocation module. Populates the {@link
 * SafetyHubExpandablePreference} with the unused site permissions service state. It also listens to
 * changes of this service, and updates the preference to reflect these.
 */
@NullMarked
public class SafetyHubPermissionsRevocationModuleMediator
        implements SafetyHubModuleMediator, UnusedSitePermissionsBridge.Observer {
    private final UnusedSitePermissionsBridge mUnusedSitePermissionsBridge;
    private final SafetyHubExpandablePreference mPreference;
    private final SafetyHubModuleMediatorDelegate mDelegate;

    private PropertyModel mModel;
    private int mRevokedPermissionsCount;

    SafetyHubPermissionsRevocationModuleMediator(
            SafetyHubExpandablePreference preference,
            SafetyHubModuleMediatorDelegate delegate,
            UnusedSitePermissionsBridge unusedSitePermissionsBridge) {
        mPreference = preference;
        mUnusedSitePermissionsBridge = unusedSitePermissionsBridge;
        mDelegate = delegate;
    }

    @Override
    public void setUpModule() {
        mModel =
                new PropertyModel.Builder(SafetyHubModuleProperties.ALL_KEYS)
                        .with(SafetyHubModuleProperties.IS_VISIBLE, true)
                        .build();

        PropertyModelChangeProcessor.create(
                mModel, mPreference, SafetyHubModuleViewBinder::bindProperties);

        mUnusedSitePermissionsBridge.addObserver(this);
        mRevokedPermissionsCount = getRevokedPermissionsCount();
    }

    @Override
    public void destroy() {
        mUnusedSitePermissionsBridge.removeObserver(this);
    }

    @Override
    public void revokedPermissionsChanged() {
        updateModule();
        mDelegate.onUpdateNeeded();
    }

    @Override
    public void updateModule() {
        mRevokedPermissionsCount = getRevokedPermissionsCount();
        assert mRevokedPermissionsCount >= 0
                : "Negative revoked permissions count detected in"
                        + " SafetyHubPermissionsRevocationModuleMediator";

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
        return SafetyHubUtils.getPermissionsModuleState(mRevokedPermissionsCount);
    }

    @Override
    public @ModuleOption int getOption() {
        return ModuleOption.UNUSED_PERMISSIONS;
    }

    @Override
    public boolean isManaged() {
        return false;
    }

    public int getRevokedPermissionsCount() {
        assert mUnusedSitePermissionsBridge != null
                : "A null UnusedSitePermissionsBridge was detected in"
                        + " SafetyHubPermissionsRevocationModuleMediator";
        return mUnusedSitePermissionsBridge.getRevokedPermissions().length;
    }

    private String getTitle() {
        if (mRevokedPermissionsCount == 0) {
            return mPreference.getContext().getString(R.string.safety_hub_permissions_ok_title);
        }
        return mPreference
                .getContext()
                .getResources()
                .getQuantityString(
                        R.plurals.safety_hub_permissions_warning_title,
                        mRevokedPermissionsCount,
                        mRevokedPermissionsCount);
    }

    private String getSummary() {
        if (mRevokedPermissionsCount == 0) {
            return mPreference.getContext().getString(R.string.safety_hub_permissions_ok_summary);
        }
        return mPreference.getContext().getString(R.string.safety_hub_permissions_warning_summary);
    }

    private @Nullable String getPrimaryButtonText() {
        return mRevokedPermissionsCount == 0
                ? null
                : mPreference.getContext().getString(R.string.got_it);
    }

    private View.@Nullable OnClickListener getPrimaryButtonListener() {
        if (mRevokedPermissionsCount == 0) {
            return null;
        }
        return v -> {
            PermissionsData[] permissionsDataList =
                    mUnusedSitePermissionsBridge.getRevokedPermissions();
            mUnusedSitePermissionsBridge.clearRevokedPermissionsReviewList();
            mDelegate.showSnackbarForModule(
                    mPreference
                            .getContext()
                            .getResources()
                            .getQuantityString(
                                    R.plurals.safety_hub_multiple_permissions_snackbar,
                                    permissionsDataList.length,
                                    permissionsDataList.length),
                    Snackbar.UMA_SAFETY_HUB_REGRANT_MULTIPLE_PERMISSIONS,
                    new SnackbarManager.SnackbarController() {
                        @Override
                        public void onAction(@Nullable Object actionData) {
                            mUnusedSitePermissionsBridge.restoreRevokedPermissionsReviewList(
                                    (PermissionsData[]) actionData);
                            recordRevokedPermissionsInteraction(
                                    PermissionsModuleInteractions.UNDO_ACKNOWLEDGE_ALL);
                            maybeRecordAbusiveNotificationRevokedInteraction(
                                    (PermissionsData[]) actionData,
                                    PermissionsModuleInteractions.UNDO_ACKNOWLEDGE_ALL);
                        }
                    },
                    permissionsDataList);
            recordRevokedPermissionsInteraction(PermissionsModuleInteractions.ACKNOWLEDGE_ALL);
            maybeRecordAbusiveNotificationRevokedInteraction(
                    permissionsDataList, PermissionsModuleInteractions.ACKNOWLEDGE_ALL);
        };
    }

    private String getSecondaryButtonText() {
        if (mRevokedPermissionsCount == 0) {
            return mPreference
                    .getContext()
                    .getString(R.string.safety_hub_go_to_site_settings_button);
        }
        return mPreference.getContext().getString(R.string.safety_hub_view_sites_button);
    }

    private View.OnClickListener getSecondaryButtonListener() {
        if (mRevokedPermissionsCount == 0) {
            return v -> {
                mDelegate.startSettingsForModule(SiteSettings.class);
                recordRevokedPermissionsInteraction(PermissionsModuleInteractions.GO_TO_SETTINGS);
            };
        }
        return v -> {
            mDelegate.startSettingsForModule(SafetyHubPermissionsFragment.class);
            recordRevokedPermissionsInteraction(PermissionsModuleInteractions.OPEN_REVIEW_UI);
        };
    }
}
