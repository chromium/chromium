// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import static org.chromium.build.NullUtil.assertNonNull;

import android.os.Bundle;

import androidx.annotation.VisibleForTesting;
import androidx.preference.Preference;
import androidx.preference.PreferenceCategory;

import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.components.browser_ui.settings.SettingsFragment;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.favicon.LargeIconBridge;

import java.util.List;

/** Fragment for listing and managing permissions for Glic actor login. */
@NullMarked
public class GlicActorLoginPermissionsFragment extends ChromeBaseSettingsFragment {

    private static final String CATEGORY_KEY = "actor_login_permissions_category";
    private static final String DESCRIPTION_KEY = "actor_login_permissions_description";

    private PreferenceCategory mCategory;
    private Preference mDescriptionPref;
    private LargeIconBridge mLargeIconBridge;
    private final SettableMonotonicObservableSupplier<String> mPageTitle =
            ObservableSuppliers.createMonotonic();

    @Override
    public void onCreatePreferences(@Nullable Bundle savedInstanceState, @Nullable String rootKey) {
        SettingsUtils.addPreferencesFromResource(this, R.xml.glic_actor_login_permissions);
        mPageTitle.set(getString(R.string.settings_glic_actor_login_permissions_section_title));
        mCategory = assertNonNull(findPreference(CATEGORY_KEY));
        mDescriptionPref = assertNonNull(mCategory.findPreference(DESCRIPTION_KEY));
        mLargeIconBridge = new LargeIconBridge(getProfile());

        // TODO(https://crbug.com/500353055): fetch permissions and set empty state if necessary
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        mLargeIconBridge.destroy();
    }

    @VisibleForTesting
    void populatePermissions(List<ActorLoginPermission> permissions) {
        mCategory.removeAll();
        mCategory.addPreference(mDescriptionPref);
        for (ActorLoginPermission permission : permissions) {
            ActorLoginPermissionPreference pref =
                    new ActorLoginPermissionPreference(
                            getContext(), permission, mLargeIconBridge, this::onRevokeClicked);
            mCategory.addPreference(pref);
        }
        notifyPreferencesUpdated();
    }

    // TODO(https://crbug.com/500353055): show warning dialog
    private void onRevokeClicked() {}

    @Override
    public MonotonicObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }

    @Override
    public @SettingsFragment.AnimationType int getAnimationType() {
        return SettingsFragment.AnimationType.PROPERTY;
    }
}
