// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings.startup;

import android.os.Bundle;

import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.multiwindow.TabbedStartupCoordinator;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.chrome.browser.settings.search.ChromeBaseSearchIndexProvider;
import org.chromium.components.browser_ui.settings.SettingsFragment;
import org.chromium.components.browser_ui.settings.SettingsUtils;

/** Settings fragment allowing users to configure Chrome's "On startup" behavior. */
@NullMarked
public class OnStartupSettingsFragment extends ChromeBaseSettingsFragment {
    public static final ChromeBaseSearchIndexProvider SEARCH_INDEX_DATA_PROVIDER =
            new ChromeBaseSearchIndexProvider(
                    OnStartupSettingsFragment.class.getName(),
                    ChromeBaseSearchIndexProvider.INDEX_OPT_OUT);

    /* package */ static final String PREF_RADIO_BUTTON_GROUP = "on_startup_radio_button_group";

    private final SettableMonotonicObservableSupplier<String> mPageTitle =
            ObservableSuppliers.createMonotonic();

    // ChromeBaseSettingsFragment implementation.
    @Override
    public void onCreatePreferences(@Nullable Bundle savedInstanceState, @Nullable String rootKey) {
        mPageTitle.set(getString(R.string.on_startup_title));
        SettingsUtils.addPreferencesFromResource(this, R.xml.on_startup_preferences);

        RadioButtonGroupOnStartupPreference radioGroupPref =
                findPreference(PREF_RADIO_BUTTON_GROUP);
        radioGroupPref.initialize(
                TabbedStartupCoordinator.getSessionStartupType(),
                startupType ->
                        TabbedStartupCoordinator.setSessionStartupType(getProfile(), startupType));
    }

    @Override
    public MonotonicObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }

    @Override
    public @SettingsFragment.AnimationType int getAnimationType() {
        return SettingsFragment.AnimationType.PROPERTY;
    }

    @Override
    public @Nullable String getMainMenuKey() {
        return "on_startup";
    }
}
