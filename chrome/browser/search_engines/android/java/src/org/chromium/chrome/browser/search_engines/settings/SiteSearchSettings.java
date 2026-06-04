// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.settings;

import android.content.Context;
import android.os.Bundle;

import org.chromium.base.ServiceLoaderUtil;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.R;
import org.chromium.chrome.browser.search_engines.settings.common.SearchEngineListPreference;
import org.chromium.chrome.browser.search_engines.settings.custom_search_engine.CustomSearchEngineCoordinator;
import org.chromium.chrome.browser.search_engines.settings.custom_site_search.CustomSiteSearchCoordinator;
import org.chromium.chrome.browser.search_engines.settings.extensions.ExtensionSearchEngineCoordinator;
import org.chromium.chrome.browser.search_engines.settings.inactive_shortcut.InactiveShortcutCoordinator;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.chrome.browser.settings.search.ChromeBaseSearchIndexProvider;
import org.chromium.chrome.browser.ui.extensions.ExtensionUi;
import org.chromium.components.browser_ui.settings.SettingsFragment;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.settings.search.PreferenceParser;
import org.chromium.components.browser_ui.settings.search.SettingsIndexData;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManagerHolder;

/**
 * Fragment for the "Manage search engines and site search" settings page. Displays lists of
 * standard search engines and custom site search shortcuts.
 */
@NullMarked
public class SiteSearchSettings extends ChromeBaseSettingsFragment {
    // TODO(crbug.com/478726836): See if this needs to be added to the search index
    private static final String KEYBOARD_SHORTCUT_RADIO_GROUP_PREF =
            "keyboard_shortcut_radio_group";
    private static final String CUSTOM_SEARCH_ENGINE_LIST_PREF = "custom_search_engine_item_list";
    private static final String CUSTOM_SITE_SEARCH_LIST_PREF = "custom_site_search_item_list";
    private static final String INACTIVE_SHORTCUT_LIST_PREF = "inactive_shortcut_list";
    private static final String EXTENSIONS_PREF_KEY = "extension_item_list";

    private static final String SEARCH_ENGINES_SECTION_PREF_KEY = "custom_search_engine_section";
    private static final String CUSTOM_SITE_SEARCH_SECTION_PREF_KEY = "custom_site_search_section";
    private static final String INACTIVE_SHORTCUTS_SECTION_PREF_KEY = "inactive_shortcut_section";
    private static final String EXTENSIONS_SECTION_PREF_KEY = "extension_section";

    private final SettableMonotonicObservableSupplier<String> mPageTitle =
            ObservableSuppliers.createMonotonic();

    private @Nullable CustomSearchEngineCoordinator mSearchEngineCoordinator;
    private @Nullable CustomSiteSearchCoordinator mSiteSearchCoordinator;
    private @Nullable InactiveShortcutCoordinator mInactiveShortcutCoordinator;
    private @Nullable ExtensionSearchEngineCoordinator mExtensionSearchEngineCoordinator;

    @Override
    public void onCreatePreferences(@Nullable Bundle savedInstanceState, @Nullable String rootKey) {
        mPageTitle.set(getString(R.string.manage_search_engines_and_site_search));

        Context context = getContext();
        Profile profile = getProfile();
        ModalDialogManager modalDialogManager =
                ((ModalDialogManagerHolder) getActivity()).getModalDialogManager();

        // Keyboard Shortcut
        SettingsUtils.addPreferencesFromResource(this, R.xml.keyboard_shortcut_preferences);
        KeyboardShortcutRadioButtonGroupPreference keyboardShortcutPref =
                findPreference(KEYBOARD_SHORTCUT_RADIO_GROUP_PREF);
        keyboardShortcutPref.setProfile(profile);

        // Search Engines
        SettingsUtils.addPreferencesFromResource(this, R.xml.custom_search_engine_preferences);
        SearchEngineListPreference customSearchEnginePref =
                findPreference(CUSTOM_SEARCH_ENGINE_LIST_PREF);
        mSearchEngineCoordinator =
                new CustomSearchEngineCoordinator(
                        context, profile, customSearchEnginePref, modalDialogManager);

        // Site Search
        SettingsUtils.addPreferencesFromResource(this, R.xml.custom_site_search_preferences);
        SearchEngineListPreference customSiteSearchPref =
                findPreference(CUSTOM_SITE_SEARCH_LIST_PREF);
        mSiteSearchCoordinator =
                new CustomSiteSearchCoordinator(
                        context, profile, customSiteSearchPref, modalDialogManager);

        // Inactive Shortcuts
        SettingsUtils.addPreferencesFromResource(this, R.xml.inactive_shortcut_preferences);
        SearchEngineListPreference inactiveShortcutPref =
                findPreference(INACTIVE_SHORTCUT_LIST_PREF);
        mInactiveShortcutCoordinator =
                new InactiveShortcutCoordinator(
                        context, profile, inactiveShortcutPref, modalDialogManager);

        // Extensions
        if (ExtensionUi.isEnabled(profile)) {
            mExtensionSearchEngineCoordinator =
                    ServiceLoaderUtil.maybeCreate(ExtensionSearchEngineCoordinator.class);
            if (mExtensionSearchEngineCoordinator != null) {
                SettingsUtils.addPreferencesFromResource(this, R.xml.extensions_preferences);
                SearchEngineListPreference extensionsPref = findPreference(EXTENSIONS_PREF_KEY);
                mExtensionSearchEngineCoordinator.initialize(
                        context, profile, extensionsPref, getCustomTabLauncher());
            }
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
    public void onDestroy() {
        if (mSearchEngineCoordinator != null) {
            mSearchEngineCoordinator.destroy();
            mSearchEngineCoordinator = null;
        }
        if (mSiteSearchCoordinator != null) {
            mSiteSearchCoordinator.destroy();
            mSiteSearchCoordinator = null;
        }
        if (mInactiveShortcutCoordinator != null) {
            mInactiveShortcutCoordinator.destroy();
            mInactiveShortcutCoordinator = null;
        }
        if (mExtensionSearchEngineCoordinator != null) {
            mExtensionSearchEngineCoordinator.destroy();
            mExtensionSearchEngineCoordinator = null;
        }
        super.onDestroy();
    }

    public static final ChromeBaseSearchIndexProvider SEARCH_INDEX_DATA_PROVIDER =
            new ChromeBaseSearchIndexProvider(
                    SiteSearchSettings.class.getName(), 0, /* isSearchable= */ false) {
                @Override
                public void updateDynamicPreferences(
                        Context context, SettingsIndexData indexData, Profile profile) {
                    if (OmniboxFeatures.sOmniboxSiteSearch.isEnabled()) {
                        addNonSearchableIndex(
                                context,
                                indexData,
                                SEARCH_ENGINES_SECTION_PREF_KEY,
                                R.string.site_search_search_engines_header,
                                R.string.site_search_search_engines_description);
                        addNonSearchableIndex(
                                context,
                                indexData,
                                CUSTOM_SITE_SEARCH_SECTION_PREF_KEY,
                                R.string.site_search_site_search_header,
                                R.string.site_search_site_search_description);
                        addNonSearchableIndex(
                                context,
                                indexData,
                                INACTIVE_SHORTCUTS_SECTION_PREF_KEY,
                                R.string.site_search_inactive_shortcuts_header,
                                0);
                        if (ExtensionUi.isEnabled(profile)) {
                            addNonSearchableIndex(
                                    context,
                                    indexData,
                                    EXTENSIONS_SECTION_PREF_KEY,
                                    R.string.site_search_extensions_header,
                                    R.string.site_search_extensions_description);
                        }
                    }
                }

                private static void addNonSearchableIndex(
                        Context context,
                        SettingsIndexData indexData,
                        String key,
                        int titleResId,
                        int summaryResId) {
                    String fragment = SiteSearchSettings.class.getName();
                    String id = PreferenceParser.createUniqueId(fragment, key);
                    SettingsIndexData.Entry.Builder builder =
                            new SettingsIndexData.Entry.Builder(
                                    /* id= */ id,
                                    /* key= */ key,
                                    /* title= */ context.getString(titleResId),
                                    /* parentFragment= */ fragment);
                    builder.setIsSearchable(false);
                    if (summaryResId != 0) {
                        builder.setSummary(context.getString(summaryResId));
                    }
                    indexData.addEntry(id, builder.build());
                }
            };
}
