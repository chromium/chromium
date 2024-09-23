// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import android.content.Context;
import android.os.Bundle;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.preference.Preference;
import androidx.preference.PreferenceScreen;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.components.favicon.LargeIconBridge;

import java.util.List;

/** Fragment to display all the allowed Fledge sites. */
public class FledgeAllSitesFragment extends PrivacySandboxSettingsBaseFragment
        implements Preference.OnPreferenceClickListener {
    private PreferenceScreen mPreferenceScreen;
    private LargeIconBridge mLargeIconBridge;
    private final ObservableSupplierImpl<String> mPageTitle = new ObservableSupplierImpl<>();

    @Override
    public void onCreatePreferences(@Nullable Bundle bundle, @Nullable String s) {
        super.onCreatePreferences(bundle, s);
        mPageTitle.set(getString(R.string.settings_fledge_all_sites_sub_page_title));
        mPreferenceScreen = getPreferenceManager().createPreferenceScreen(getStyledContext());
        setPreferenceScreen(mPreferenceScreen);
    }

    @Override
    public ObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }

    @Override
    public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);

        // Disable animations of preference changes.
        getListView().setItemAnimator(null);
    }

    @Override
    public void onResume() {
        super.onResume();
        getPrivacySandboxBridge().getFledgeJoiningEtldPlusOneForDisplay(this::populateSites);
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
                    .setFledgeJoiningAllowed(((FledgePreference) preference).getSite(), false);
            mPreferenceScreen.removePreference(preference);

            showSnackbar(
                    R.string.settings_fledge_page_block_site_snackbar,
                    null,
                    Snackbar.TYPE_ACTION,
                    Snackbar.UMA_PRIVACY_SANDBOX_REMOVE_SITE,
                    /* actionStringResId= */ 0,
                    /* multiLine= */ true);
            RecordUserAction.record("Settings.PrivacySandbox.Fledge.SiteRemoved");
            return true;
        }

        return false;
    }

    private void populateSites(List<String> allSites) {
        if (mLargeIconBridge == null) {
            mLargeIconBridge = new LargeIconBridge(getProfile());
        }

        mPreferenceScreen.removeAll();
        for (String site : allSites) {
            FledgePreference preference =
                    new FledgePreference(getStyledContext(), site, mLargeIconBridge);
            preference.setImage(
                    R.drawable.btn_close,
                    getResources()
                            .getString(R.string.settings_fledge_page_block_site_a11y_label, site));
            preference.setDividerAllowedBelow(false);
            preference.setOnPreferenceClickListener(this);
            mPreferenceScreen.addPreference(preference);
        }
    }

    private Context getStyledContext() {
        return getPreferenceManager().getContext();
    }
}
