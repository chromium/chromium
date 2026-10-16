// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings.startup;

import static org.chromium.build.NullUtil.assertNonNull;

import android.os.Bundle;
import android.text.TextUtils;

import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.multiwindow.TabbedStartupCoordinator;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.chrome.browser.settings.search.ChromeBaseSearchIndexProvider;
import org.chromium.components.browser_ui.settings.SettingsFragment;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.prefs.PrefChangeRegistrar;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;

import java.util.ArrayList;
import java.util.List;

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
    private @Nullable PrefChangeRegistrar mPrefChangeRegistrar;
    private @Nullable RadioButtonGroupOnStartupPreference mRadioGroupPref;

    // ChromeBaseSettingsFragment implementation.
    @Override
    public void onCreatePreferences(@Nullable Bundle savedInstanceState, @Nullable String rootKey) {
        mPageTitle.set(getString(R.string.on_startup_title));
        SettingsUtils.addPreferencesFromResource(this, R.xml.on_startup_preferences);

        PrefService prefService = UserPrefs.get(getProfile());
        mRadioGroupPref = findPreference(PREF_RADIO_BUTTON_GROUP);
        assertNonNull(mRadioGroupPref)
                .initialize(
                        TabbedStartupCoordinator.getSessionStartupType(),
                        TabbedStartupCoordinator.getSessionStartupUrls(prefService),
                        startupType ->
                                TabbedStartupCoordinator.setSessionStartupType(
                                        getProfile(), startupType),
                        new RadioButtonGroupOnStartupPreference.StartupPageItemActionListener() {
                            @Override
                            public void onEditStartupPage(int index, String url) {
                                // TODO(crbug.com/571155672): Launch edit startup page dialog.
                            }

                            @Override
                            public void onRemoveStartupPage(int index, String url) {
                                List<String> urls =
                                        new ArrayList<>(
                                                TabbedStartupCoordinator.getSessionStartupUrls(
                                                        prefService));
                                if (index >= 0
                                        && index < urls.size()
                                        && TextUtils.equals(urls.get(index), url)) {
                                    urls.remove(index);
                                } else {
                                    int matchedIndex = urls.indexOf(url);
                                    if (matchedIndex < 0) {
                                        return;
                                    }
                                    urls.remove(matchedIndex);
                                }
                                TabbedStartupCoordinator.setSessionStartupUrls(prefService, urls);
                            }
                        });

        mPrefChangeRegistrar = new PrefChangeRegistrar(prefService);
        mPrefChangeRegistrar.addObserver(
                Pref.RESTORE_ON_STARTUP,
                () -> {
                    if (mRadioGroupPref != null) {
                        mRadioGroupPref.setStartupType(
                                TabbedStartupCoordinator.getSessionStartupType());
                    }
                });
        mPrefChangeRegistrar.addObserver(
                Pref.URLS_TO_RESTORE_ON_STARTUP,
                () -> {
                    if (mRadioGroupPref != null) {
                        mRadioGroupPref.setStartupUrls(
                                TabbedStartupCoordinator.getSessionStartupUrls(prefService));
                    }
                });
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        if (mPrefChangeRegistrar != null) {
            mPrefChangeRegistrar.destroy();
            mPrefChangeRegistrar = null;
        }
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
