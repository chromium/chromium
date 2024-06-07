// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.preference.Preference;
import androidx.preference.PreferenceCategory;

import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.ui.widget.ButtonCompat;

public class SafetyHubPermissionsFragment extends SafetyHubBaseFragment
        implements Preference.OnPreferenceClickListener, UnusedSitePermissionsBridge.Observer {
    private static final String PERMISSIONS_LIST_PREFERENCE = "permissions_list";

    private UnusedSitePermissionsBridge mUnusedSitePermissionsBridge;
    private LargeIconBridge mLargeIconBridge;
    private PreferenceCategory mPermissionsListCategory;
    private ButtonCompat mBottomButton;
    private boolean mPermissionsRevocationConfirmed;

    @Override
    public void onCreatePreferences(@Nullable Bundle bundle, @Nullable String s) {
        SettingsUtils.addPreferencesFromResource(this, R.xml.safety_hub_permissions_preferences);
        getActivity().setTitle(R.string.safety_hub_permissions_page_title);

        mUnusedSitePermissionsBridge = UnusedSitePermissionsBridge.getForProfile(getProfile());
        mUnusedSitePermissionsBridge.addObserver(this);
        mPermissionsListCategory = findPreference(PERMISSIONS_LIST_PREFERENCE);
    }

    @NonNull
    @Override
    public View onCreateView(
            @NonNull LayoutInflater inflater,
            @Nullable ViewGroup container,
            @Nullable Bundle savedInstanceState) {
        LinearLayout view =
                (LinearLayout) super.onCreateView(inflater, container, savedInstanceState);
        LinearLayout bottomView =
                (LinearLayout) inflater.inflate(R.layout.safety_hub_bottom_elements, view, false);
        mBottomButton = bottomView.findViewById(R.id.safety_hub_permissions_button);
        mBottomButton.setOnClickListener(
                new View.OnClickListener() {
                    @Override
                    public void onClick(View view) {
                        mPermissionsRevocationConfirmed = true;
                        getActivity().finish();
                    }
                });
        view.addView(bottomView);
        return view;
    }

    @Override
    public void onResume() {
        super.onResume();
        updatePreferenceList();
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

    private void updatePreferenceList() {
        if (mLargeIconBridge == null) {
            mLargeIconBridge = new LargeIconBridge(getProfile());
        }
        mPermissionsListCategory.removeAll();

        PermissionsData[] permissionsDataList =
                mUnusedSitePermissionsBridge.getRevokedPermissions();
        for (PermissionsData permissionsData : permissionsDataList) {
            SafetyHubPermissionsPreference preference =
                    new SafetyHubPermissionsPreference(
                            getContext(), permissionsData, mLargeIconBridge);
            preference.setOnPreferenceClickListener(this);
            mPermissionsListCategory.addPreference(preference);
        }
    }
}
