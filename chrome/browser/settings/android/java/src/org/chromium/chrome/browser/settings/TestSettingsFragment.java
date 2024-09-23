// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import android.os.Bundle;

import androidx.annotation.Nullable;
import androidx.preference.PreferenceFragmentCompat;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.components.browser_ui.settings.SettingsPage;

/** An embeddable settings fragment that has several preference inside. */
public class TestSettingsFragment extends PreferenceFragmentCompat implements SettingsPage {
    public static final String EXTRA_TITLE = "title";

    private final ObservableSupplierImpl<String> mPageTitle = new ObservableSupplierImpl<>();

    @Override
    public void onCreatePreferences(@Nullable Bundle bundle, @Nullable String s) {
        addPreferencesFromResource(R.xml.test_settings_fragment);

        String title = "test title";
        Bundle args = getArguments();
        if (args != null) {
            String extraTitle = args.getString(EXTRA_TITLE);
            if (extraTitle != null) {
                title = extraTitle;
            }
        }
        mPageTitle.set(title);
    }

    @Override
    public ObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }
}
