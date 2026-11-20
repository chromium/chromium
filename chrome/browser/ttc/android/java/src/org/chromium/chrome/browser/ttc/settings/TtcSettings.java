// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ttc.settings;

import static org.chromium.build.NullUtil.assertNonNull;

import android.os.Bundle;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.chrome.browser.settings.search.ChromeBaseSearchIndexProvider;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarPrefs;
import org.chromium.chrome.browser.ttc.R;
import org.chromium.components.browser_ui.settings.ChromeBasePreference;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.SettingsFragment;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.prefs.PrefChangeRegistrar;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;

/**
 * The TTC settings page, chrome://settings/ai/ttc. Every setting is offered on every form factor; a
 * setting whose UI isn't available on a form factor yet is saved for when it is.
 */
@NullMarked
public class TtcSettings extends ChromeBaseSettingsFragment {
    @VisibleForTesting static final String PREF_MICROPHONE_CHIME = "ttc_microphone_chime";
    @VisibleForTesting static final String PREF_DESCRIPTION = "ttc_description";
    @VisibleForTesting static final String PREF_PIN_TO_TOOLBAR = "ttc_pin_to_toolbar";
    @VisibleForTesting static final String PREF_CAPTIONS = "ttc_captions";

    /** The key of the main settings row that opens this page. */
    private static final String MAIN_MENU_KEY = "ttc";

    private final SettableMonotonicObservableSupplier<String> mPageTitle =
            ObservableSuppliers.createMonotonic();
    private @Nullable PrefChangeRegistrar mPrefChangeRegistrar;

    @Override
    public void onCreatePreferences(@Nullable Bundle savedInstanceState, @Nullable String rootKey) {
        SettingsUtils.addPreferencesFromResource(this, R.xml.ttc_settings);
        mPageTitle.set(getString(R.string.ttc_settings_title));

        // Android has a single regular profile, and TTC settings aren't specific to Incognito.
        PrefService prefs = UserPrefs.get(getProfile().getOriginalProfile());
        PrefChangeRegistrar registrar = new PrefChangeRegistrar(prefs);
        mPrefChangeRegistrar = registrar;

        // TODO(b/580057376): Nothing plays the chime yet, pending a sound from UX.
        bindSwitchToProfilePref(
                PREF_MICROPHONE_CHIME, Pref.TTC_MICROPHONE_CHIME_ENABLED, prefs, registrar);
        bindSwitchToProfilePref(PREF_CAPTIONS, Pref.TTC_CAPTIONS_ENABLED, prefs, registrar);
        updatePinToToolbarSummary();
    }

    @Override
    public void onResume() {
        super.onResume();
        // TTC is pinned on the toolbar shortcut settings page, which the pin row opens.
        updatePinToToolbarSummary();
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
    public @Nullable String getMainMenuKey() {
        return MAIN_MENU_KEY;
    }

    @Override
    public @SettingsFragment.AnimationType int getAnimationType() {
        return SettingsFragment.AnimationType.PROPERTY;
    }

    /**
     * Binds the switch {@code key} to the boolean profile pref {@code pref}. The switch follows the
     * pref when it's changed elsewhere while this page shows, e.g. from the session UI's menu.
     */
    private void bindSwitchToProfilePref(
            String key, String pref, PrefService prefs, PrefChangeRegistrar registrar) {
        ChromeSwitchPreference switchPreference = assertNonNull(findPreference(key));
        switchPreference.setChecked(prefs.getBoolean(pref));
        switchPreference.setOnPreferenceChangeListener(
                (preference, newValue) -> {
                    prefs.setBoolean(pref, (boolean) newValue);
                    return true;
                });
        registrar.addObserver(pref, () -> switchPreference.setChecked(prefs.getBoolean(pref)));
    }

    /**
     * Shows whether TTC is pinned to the toolbar, i.e. chosen as the toolbar shortcut, on the row
     * that opens the toolbar shortcut settings.
     */
    private void updatePinToToolbarSummary() {
        ChromeBasePreference pinToToolbar = assertNonNull(findPreference(PREF_PIN_TO_TOOLBAR));
        boolean isPinned =
                AdaptiveToolbarPrefs.isCustomizationPreferenceEnabled()
                        && AdaptiveToolbarPrefs.getCustomizationSetting()
                                == AdaptiveToolbarButtonVariant.TTC;
        pinToToolbar.setSummary(
                isPinned
                        ? R.string.ttc_settings_pin_to_toolbar_pinned_summary
                        : R.string.ttc_settings_pin_to_toolbar_summary);
    }

    public static final ChromeBaseSearchIndexProvider SEARCH_INDEX_DATA_PROVIDER =
            new ChromeBaseSearchIndexProvider(TtcSettings.class.getName(), R.xml.ttc_settings);
}
