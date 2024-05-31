// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import android.os.Bundle;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.preference.Preference;
import androidx.preference.PreferenceCategory;

import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.components.browser_ui.settings.SettingsUtils;

public class SafetyHubPermissionsFragment extends ChromeBaseSettingsFragment
        implements Preference.OnPreferenceClickListener, UnusedSitePermissionsBridge.Observer {
    private static final String PERMISSIONS_LIST_PREFERENCE = "permissions_list";

    private UnusedSitePermissionsBridge mUnusedSitePermissionsBridge;
    private PreferenceCategory mPermissionsListCategory;

    @Override
    public void onCreatePreferences(@Nullable Bundle bundle, @Nullable String s) {
        SettingsUtils.addPreferencesFromResource(this, R.xml.safety_hub_permissions_preferences);
        getActivity().setTitle(R.string.safety_hub_permissions_page_title);

        mUnusedSitePermissionsBridge = UnusedSitePermissionsBridge.getForProfile(getProfile());
        mUnusedSitePermissionsBridge.addObserver(this);
        mPermissionsListCategory = findPreference(PERMISSIONS_LIST_PREFERENCE);
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
    }

    @Override
    public boolean onPreferenceClick(@NonNull Preference preference) {
        if (preference instanceof SafetyHubPermissionsPreference) {
            PermissionsData permissionsData =
                    ((SafetyHubPermissionsPreference) preference).getPermissionsData();
            mUnusedSitePermissionsBridge.regrantPermissions(permissionsData.getOrigin());
        }
        return false;
    }

    @Override
    public void revokedPermissionsChanged() {
        updatePreferenceList();
    }

    private void updatePreferenceList() {
        mPermissionsListCategory.removeAll();

        PermissionsData[] permissionsDataList =
                mUnusedSitePermissionsBridge.getRevokedPermissions();
        for (PermissionsData permissionsData : permissionsDataList) {
            SafetyHubPermissionsPreference preference =
                    new SafetyHubPermissionsPreference(getContext(), permissionsData);
            preference.setOnPreferenceClickListener(this);
            mPermissionsListCategory.addPreference(preference);
        }
    }
}
