// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import android.os.Bundle;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.preference.Preference;

import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.favicon.LargeIconBridge;

/**
 * Safety Hub subpage that displays a list of all revoked permissions alongside their supported
 * actions.
 */
public class SafetyHubPermissionsFragment extends SafetyHubSubpageFragment
        implements Preference.OnPreferenceClickListener, UnusedSitePermissionsBridge.Observer {
    private UnusedSitePermissionsBridge mUnusedSitePermissionsBridge;
    private LargeIconBridge mLargeIconBridge;
    private boolean mPermissionsRevocationConfirmed;

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

        if (mPermissionsRevocationConfirmed) {
            PermissionsData[] permissionsDataList =
                    mUnusedSitePermissionsBridge.getRevokedPermissions();
            mUnusedSitePermissionsBridge.clearRevokedPermissionsReviewList();
            showSnackbarOnLastFocusedActivity(
                    getString(
                            R.string.safety_hub_multiple_permissions_snackbar,
                            permissionsDataList.length),
                    Snackbar.UMA_SAFETY_HUB_REGRANT_MULTIPLE_PERMISSIONS,
                    new SnackbarManager.SnackbarController() {
                        @Override
                        public void onAction(Object actionData) {
                            mUnusedSitePermissionsBridge.restoreRevokedPermissionsReviewList(
                                    (PermissionsData[]) actionData);
                        }
                    },
                    permissionsDataList);
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
                        }
                    },
                    permissionsData);
        }
        return false;
    }

    @Override
    public void revokedPermissionsChanged() {
        updatePreferenceList();
    }

    @Override
    protected void updatePreferenceList() {
        if (mLargeIconBridge == null) {
            mLargeIconBridge = new LargeIconBridge(getProfile());
        }
        mPreferenceList.removeAll();

        PermissionsData[] permissionsDataList =
                mUnusedSitePermissionsBridge.getRevokedPermissions();
        for (PermissionsData permissionsData : permissionsDataList) {
            SafetyHubPermissionsPreference preference =
                    new SafetyHubPermissionsPreference(
                            getContext(), permissionsData, mLargeIconBridge);
            preference.setOnPreferenceClickListener(this);
            mPreferenceList.addPreference(preference);
        }
    }

    @Override
    protected int getTitleId() {
        return R.string.safety_hub_permissions_page_title;
    }

    @Override
    protected int getHeaderId() {
        return R.string.safety_hub_permissions_page_header;
    }

    @Override
    protected void onBottomButtonClicked() {
        mPermissionsRevocationConfirmed = true;
        getActivity().finish();
    }
}
