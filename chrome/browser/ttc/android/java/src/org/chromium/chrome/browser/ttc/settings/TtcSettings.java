// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ttc.settings;

import static org.chromium.build.NullUtil.assertNonNull;

import android.content.Context;
import android.content.res.Configuration;
import android.os.Bundle;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.ui.KeyboardUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.chrome.browser.settings.search.ChromeBaseSearchIndexProvider;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarPrefs;
import org.chromium.chrome.browser.ttc.R;
import org.chromium.components.browser_ui.settings.ChromeBasePreference;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.SettingsFragment;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.settings.search.SettingsIndexData;
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
    @VisibleForTesting static final String PREF_KEYBOARD_SHORTCUT = "ttc_keyboard_shortcut";

    @VisibleForTesting
    static final String PREF_HOLD_TO_SPEAK_SHORTCUT = "ttc_hold_to_speak_shortcut";

    /** The key of the main settings row that opens this page. */
    private static final String MAIN_MENU_KEY = "ttc";

    private final SettableMonotonicObservableSupplier<String> mPageTitle =
            ObservableSuppliers.createMonotonic();
    private @Nullable PrefChangeRegistrar mPrefChangeRegistrar;
    private @Nullable TtcShortcutRecorderDialog mShortcutRecorderDialog;

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

        bindShortcutPreference(
                PREF_KEYBOARD_SHORTCUT,
                Pref.TTC_KEYBOARD_SHORTCUT,
                Pref.TTC_HOLD_TO_SPEAK_SHORTCUT,
                prefs,
                registrar);
        bindShortcutPreference(
                PREF_HOLD_TO_SPEAK_SHORTCUT,
                Pref.TTC_HOLD_TO_SPEAK_SHORTCUT,
                Pref.TTC_KEYBOARD_SHORTCUT,
                prefs,
                registrar);
        updateShortcutVisibility();
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        // The activities that show settings handle keyboard configuration changes themselves, so
        // connecting or disconnecting a physical keyboard doesn't recreate this page.
        if (updateShortcutVisibility()) notifyPreferencesUpdated();
    }

    @Override
    public void onResume() {
        super.onResume();
        // TTC is pinned on the toolbar shortcut settings page, which the pin row opens.
        updatePinToToolbarSummary();
    }

    @Override
    public void onDestroyView() {
        super.onDestroyView();
        dismissShortcutRecorderDialog();
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
     * Binds the shortcut row {@code key} to the string profile pref {@code pref}. A recorded
     * shortcut can't be the one {@code otherPref} holds, so that TTC's shortcuts stay distinct.
     */
    private void bindShortcutPreference(
            String key,
            String pref,
            String otherPref,
            PrefService prefs,
            PrefChangeRegistrar registrar) {
        TtcShortcutPreference shortcutPreference = assertNonNull(findPreference(key));
        // A saved value that isn't a valid shortcut, e.g. one that Chrome has started to use
        // since, shows as not set.
        Runnable update =
                () -> {
                    String shortcut = prefs.getString(pref);
                    boolean isValid =
                            TtcShortcutUtils.getStatus(shortcut) == TtcShortcutStatus.VALID;
                    shortcutPreference.setShortcut(isValid ? shortcut : "");
                };
        update.run();
        shortcutPreference.setOnEditListener(
                () ->
                        showShortcutRecorderDialog(
                                shortcutPreference,
                                /* unavailableShortcut= */ prefs.getString(otherPref),
                                shortcut -> {
                                    prefs.setString(pref, shortcut);
                                    update.run();
                                }));
        shortcutPreference.setOnClearListener(
                () -> {
                    prefs.setString(pref, "");
                    update.run();
                });
        registrar.addObserver(pref, update::run);
    }

    /**
     * Shows the dialog that records the shortcut of {@code shortcutPreference}, unless one already
     * shows, e.g. because the row was clicked twice.
     */
    private void showShortcutRecorderDialog(
            TtcShortcutPreference shortcutPreference,
            String unavailableShortcut,
            Callback<String> onSave) {
        if (mShortcutRecorderDialog != null) return;
        TtcShortcutRecorderDialog dialog =
                new TtcShortcutRecorderDialog(
                        requireContext(),
                        assertNonNull(shortcutPreference.getTitle()),
                        unavailableShortcut,
                        onSave,
                        /* onDismiss= */ () -> mShortcutRecorderDialog = null);
        mShortcutRecorderDialog = dialog;
        dialog.show();
    }

    private void dismissShortcutRecorderDialog() {
        if (mShortcutRecorderDialog != null) mShortcutRecorderDialog.dismiss();
    }

    @Nullable TtcShortcutRecorderDialog getShortcutRecorderDialogForTesting() {
        return mShortcutRecorderDialog;
    }

    /**
     * Shows the shortcut rows only while a physical keyboard is connected. Returns whether that
     * changed whether they show.
     */
    private boolean updateShortcutVisibility() {
        boolean hasKeyboard = KeyboardUtils.isHardKeyboardConnected(requireContext());
        boolean changed = false;
        for (String key : new String[] {PREF_KEYBOARD_SHORTCUT, PREF_HOLD_TO_SPEAK_SHORTCUT}) {
            TtcShortcutPreference shortcutPreference = assertNonNull(findPreference(key));
            if (shortcutPreference.isVisible() == hasKeyboard) continue;
            shortcutPreference.setVisible(hasKeyboard);
            changed = true;
        }
        // Recording a shortcut needs a keyboard.
        if (!hasKeyboard) dismissShortcutRecorderDialog();
        return changed;
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
            new ChromeBaseSearchIndexProvider(TtcSettings.class.getName(), R.xml.ttc_settings) {
                @Override
                public void updateDynamicPreferences(
                        Context context, SettingsIndexData indexData, Profile profile) {
                    // The shortcuts only show while a physical keyboard is connected.
                    if (!KeyboardUtils.isHardKeyboardConnected(context)) {
                        String fragment = TtcSettings.class.getName();
                        indexData.removeEntryForKey(fragment, PREF_KEYBOARD_SHORTCUT);
                        indexData.removeEntryForKey(fragment, PREF_HOLD_TO_SPEAK_SHORTCUT);
                    }
                }
            };
}
