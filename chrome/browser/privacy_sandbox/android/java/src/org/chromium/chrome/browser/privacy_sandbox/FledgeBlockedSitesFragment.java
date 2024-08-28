// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import android.os.Bundle;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.preference.Preference;
import androidx.preference.PreferenceCategory;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.favicon.LargeIconBridge;

import java.util.List;

/** Fragment for the blocked Fledge sites. */
public class FledgeBlockedSitesFragment extends PrivacySandboxSettingsBaseFragment
        implements Preference.OnPreferenceClickListener {
    private static final String BLOCKED_SITES_PREFERENCE = "block_list";

    private PreferenceCategory mBlockedSitesCategory;
    private LargeIconBridge mLargeIconBridge;
    private final ObservableSupplierImpl<String> mPageTitle = new ObservableSupplierImpl<>();

    @Override
    public void onCreatePreferences(@Nullable Bundle bundle, @Nullable String s) {
        super.onCreatePreferences(bundle, s);
        mPageTitle.set(getString(R.string.settings_fledge_page_blocked_sites_sub_page_title));
        SettingsUtils.addPreferencesFromResource(this, R.xml.block_list_preference);

        mBlockedSitesCategory = findPreference(BLOCKED_SITES_PREFERENCE);
    }

    @Override
    public ObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }

    @Override
    public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);

        // Disable animations of preference changes
        getListView().setItemAnimator(null);
    }

    @Override
    public void onResume() {
        super.onResume();
        populateSites();
        updateBlockedSitesDescription();
    }

    @Override
    public void onDestroyView() {
        super.onDestroyView();
        if (mLargeIconBridge != null) {
            mLargeIconBridge.destroy();
        }
        mLargeIconBridge = null;
    }

    @Override
    public boolean onPreferenceClick(@NonNull Preference preference) {
        if (preference instanceof FledgePreference) {
            getPrivacySandboxBridge()
                    .setFledgeJoiningAllowed(((FledgePreference) preference).getSite(), true);
            mBlockedSitesCategory.removePreference(preference);
            updateBlockedSitesDescription();

            showSnackbar(
                    R.string.settings_fledge_page_add_site_snackbar,
                    null,
                    Snackbar.TYPE_ACTION,
                    Snackbar.UMA_PRIVACY_SANDBOX_ADD_SITE,
                    /* actionStringResId= */ 0,
                    /* multiLine= */ true);
            RecordUserAction.record("Settings.PrivacySandbox.Fledge.SiteAdded");
            return true;
        }

        return false;
    }

    private void populateSites() {
        if (mLargeIconBridge == null) {
            mLargeIconBridge = new LargeIconBridge(getProfile());
        }

        mBlockedSitesCategory.removeAll();
        List<String> blockedSites =
                getPrivacySandboxBridge().getBlockedFledgeJoiningTopFramesForDisplay();
        for (String site : blockedSites) {
            FledgePreference preference =
                    new FledgePreference(getContext(), site, mLargeIconBridge);
            preference.setImage(
                    R.drawable.ic_add,
                    getResources()
                            .getString(R.string.settings_fledge_page_allow_site_a11y_label, site));
            preference.setDividerAllowedBelow(false);
            preference.setOnPreferenceClickListener(this);
            mBlockedSitesCategory.addPreference(preference);
        }
    }

    private void updateBlockedSitesDescription() {
        mBlockedSitesCategory.setSummary(
                mBlockedSitesCategory.getPreferenceCount() == 0
                        ? R.string.settings_fledge_page_blocked_sites_description_empty
                        : R.string.settings_fledge_page_blocked_sites_description);
    }
}
