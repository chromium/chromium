// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import static org.chromium.chrome.browser.safety_hub.SafetyHubMetricUtils.recordRevokedPermissionsInteraction;

import android.os.Bundle;
import android.view.MenuItem;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.preference.Preference;

import org.chromium.chrome.browser.safety_hub.SafetyHubMetricUtils.PermissionsModuleInteractions;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.site_settings.SiteSettings;
import org.chromium.components.favicon.LargeIconBridge;

/**
 * Safety Hub subpage that displays a list of all revoked permissions alongside their supported
 * actions.
 */
public class SafetyHubPermissionsFragment extends SafetyHubSubpageFragment
        implements Preference.OnPreferenceClickListener, UnusedSitePermissionsBridge.Observer {
    private UnusedSitePermissionsBridge mUnusedSitePermissionsBridge;
    private LargeIconBridge mLargeIconBridge;

    @Override
    public void onCreatePreferences(@Nullable Bundle bundle, @Nullable String s) {
        super.onCreatePreferences(bundle, s);

        mUnusedSitePermissionsBridge = UnusedSitePermissionsBridge.getForProfile(getProfile());
        mUnusedSitePermissionsBridge.addObserver(this);
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        mUnusedSitePermissionsBridge.removeObserver(this);

        if (mLargeIconBridge != null) {
            mLargeIconBridge.destroy();
        }

        PermissionsData[] permissionsDataList =
                mUnusedSitePermissionsBridge.getRevokedPermissions();
        if (mBulkActionConfirmed && permissionsDataList.length != 0) {
            mUnusedSitePermissionsBridge.clearRevokedPermissionsReviewList();
            showSnackbarOnLastFocusedActivity(
                    getContext()
                            .getResources()
                            .getQuantityString(
                                    R.plurals.safety_hub_multiple_permissions_snackbar,
                                    permissionsDataList.length,
                                    permissionsDataList.length),
                    Snackbar.UMA_SAFETY_HUB_REGRANT_MULTIPLE_PERMISSIONS,
                    new SnackbarManager.SnackbarController() {
                        @Override
                        public void onAction(Object actionData) {
                            mUnusedSitePermissionsBridge.restoreRevokedPermissionsReviewList(
                                    (PermissionsData[]) actionData);
                            recordRevokedPermissionsInteraction(
                                    PermissionsModuleInteractions.UNDO_ACKNOWLEDGE_ALL);
                        }
                    },
                    permissionsDataList);
            recordRevokedPermissionsInteraction(PermissionsModuleInteractions.ACKNOWLEDGE_ALL);
        }
    }

    @Override
    public boolean onPreferenceClick(@NonNull Preference preference) {
        if (preference instanceof SafetyHubPermissionsPreference) {
            PermissionsData permissionsData =
                    ((SafetyHubPermissionsPreference) preference).getPermissionsData();
            mUnusedSitePermissionsBridge.regrantPermissions(permissionsData.getOrigin());
            showSnackbar(
                    getString(
                            R.string.safety_hub_single_permission_snackbar,
                            permissionsData.getOrigin()),
                    Snackbar.UMA_SAFETY_HUB_REGRANT_SINGLE_PERMISSION,
                    new SnackbarManager.SnackbarController() {
                        @Override
                        public void onAction(Object actionData) {
                            mUnusedSitePermissionsBridge.undoRegrantPermissions(
                                    (PermissionsData) actionData);
                            recordRevokedPermissionsInteraction(
                                    PermissionsModuleInteractions.UNDO_ALLOW_AGAIN);
                        }
                    },
                    permissionsData);
            recordRevokedPermissionsInteraction(PermissionsModuleInteractions.ALLOW_AGAIN);
        }
        return false;
    }

    @Override
    public void revokedPermissionsChanged() {
        updatePreferenceList();
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        if (item.getItemId() == R.id.safety_hub_subpage_menu_item) {
            startSettings(SiteSettings.class);
            recordRevokedPermissionsInteraction(PermissionsModuleInteractions.GO_TO_SETTINGS);
            return true;
        }
        return false;
    }

    @Override
    protected void updatePreferenceList() {
        if (mLargeIconBridge == null) {
            mLargeIconBridge = new LargeIconBridge(getProfile());
        }
        mPreferenceList.removeAll();

        PermissionsData[] permissionsDataList =
                mUnusedSitePermissionsBridge.getRevokedPermissions();
        mBottomButton.setEnabled(permissionsDataList.length != 0);
        for (PermissionsData permissionsData : permissionsDataList) {
            SafetyHubPermissionsPreference preference =
                    new SafetyHubPermissionsPreference(
                            getContext(), permissionsData, mLargeIconBridge);
            preference.setOnPreferenceClickListener(this);
            mPreferenceList.addPreference(preference);
        }
    }

    @Override
    protected @StringRes int getTitleId() {
        return R.string.safety_hub_permissions_page_title;
    }

    @Override
    protected @StringRes int getHeaderId() {
        return R.string.safety_hub_permissions_warning_summary;
    }

    @Override
    protected @StringRes int getButtonTextId() {
        return R.string.got_it;
    }

    @Override
    protected @StringRes int getMenuItemTextId() {
        return R.string.safety_hub_go_to_site_settings_button;
    }

    @Override
    protected @StringRes int getPermissionsListTextId() {
        return R.string.page_info_permissions_title;
    }
}
