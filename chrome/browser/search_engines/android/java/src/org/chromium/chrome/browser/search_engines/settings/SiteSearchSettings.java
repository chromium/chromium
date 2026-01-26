// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.settings;

import android.os.Bundle;

import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.search_engines.R;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.components.browser_ui.settings.SettingsFragment;

/**
 * Fragment for the "Manage search engines and site search" settings page. Displays lists of
 * standard search engines and custom site search shortcuts.
 */
@NullMarked
public class SiteSearchSettings extends ChromeBaseSettingsFragment {
    // TODO(crbug.com/478726836): See if this needs to be added to the search index
    @Override
    public void onCreatePreferences(
            @Nullable Bundle savedInstanceState, @Nullable String rootKey) {}

    @Override
    public MonotonicObservableSupplier<String> getPageTitle() {
        String title = getString(R.string.manage_search_engines_and_site_search);
        return new ObservableSupplierImpl<>(title);
    }

    @Override
    public @SettingsFragment.AnimationType int getAnimationType() {
        return SettingsFragment.AnimationType.PROPERTY;
    }
}
