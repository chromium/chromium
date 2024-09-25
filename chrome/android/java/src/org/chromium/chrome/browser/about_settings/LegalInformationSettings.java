// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.about_settings;

import android.os.Bundle;

import androidx.preference.PreferenceFragmentCompat;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.R;
import org.chromium.components.browser_ui.settings.EmbeddableSettingsPage;
import org.chromium.components.browser_ui.settings.SettingsUtils;

/** Fragment to display legal information about Chrome. */
public class LegalInformationSettings extends PreferenceFragmentCompat
        implements EmbeddableSettingsPage {
    private final ObservableSupplierImpl<String> mPageTitle = new ObservableSupplierImpl<>();

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String s) {
        SettingsUtils.addPreferencesFromResource(this, R.xml.legal_information_preferences);
        mPageTitle.set(getString(R.string.legal_information_title));
    }

    @Override
    public ObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }
}
