// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.appearance.settings;

import android.content.Context;
import android.content.SharedPreferences.OnSharedPreferenceChangeListener;
import android.os.Bundle;

import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.build.NullUtil;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.chrome.browser.settings.search.ChromeBaseSearchIndexProvider;
import org.chromium.chrome.browser.ui.vertical_tabs.VerticalTabUtils;
import org.chromium.chrome.browser.ui.vertical_tabs.VerticalTabUtils.ExpandOnHoverToggleEntryPoint;
import org.chromium.chrome.browser.ui.vertical_tabs.VerticalTabUtils.WindowWidthBoundary;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.CustomDividerFragment;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.settings.search.SettingsIndexData;

/** Fragment to manage tab position settings (Horizontal vs. Vertical). */
@NullMarked
public class TabPositionSettingsFragment extends ChromeBaseSettingsFragment
        implements CustomDividerFragment {
    public static final String PREF_TAB_POSITION_CARD_SELECTOR = "tab_position_card_selector";
    public static final String PREF_EXPAND_TABS_ON_HOVER_SWITCH = "expand_tabs_on_hover_switch";

    public static final ChromeBaseSearchIndexProvider SEARCH_INDEX_DATA_PROVIDER =
            new ChromeBaseSearchIndexProvider(
                    TabPositionSettingsFragment.class.getName(), R.xml.tab_position_preferences) {
                @Override
                public void updateDynamicPreferences(
                        Context context, SettingsIndexData indexData, Profile profile) {
                    String prefFragment = TabPositionSettingsFragment.class.getName();
                    // The card selector has no title, so there is nothing to search for.
                    indexData.removeEntryForKey(prefFragment, PREF_TAB_POSITION_CARD_SELECTOR);
                    if (!shouldShowExpandOnHoverSwitch(context)) {
                        indexData.removeEntryForKey(prefFragment, PREF_EXPAND_TABS_ON_HOVER_SWITCH);
                    }
                }
            };

    private final SettableMonotonicObservableSupplier<String> mPageTitle =
            ObservableSuppliers.createMonotonic();
    private @Nullable TabPositionCardPreference mCardPreference;
    private @Nullable ChromeSwitchPreference mExpandOnHoverSwitch;
    private @Nullable OnSharedPreferenceChangeListener mPrefsListener;

    @Override
    @SuppressWarnings("UseSharedPreferencesManagerFromChromeCheck")
    public void onCreatePreferences(@Nullable Bundle savedInstanceState, @Nullable String rootKey) {
        assert VerticalTabUtils.isVerticalTabsEligible(getContext())
                : "TabPositionSettingsFragment launched on ineligible device";

        mPageTitle.set(getString(R.string.tab_position_settings_title));
        SettingsUtils.addPreferencesFromResource(this, R.xml.tab_position_preferences);

        mCardPreference = NullUtil.assertNonNull(findPreference(PREF_TAB_POSITION_CARD_SELECTOR));
        mCardPreference.setOnPreferenceChangeListener(
                (preference, newValue) -> {
                    int widthDp = getResources().getConfiguration().screenWidthDp;
                    if (VerticalTabUtils.isTabLayoutSwitchingInProgress()
                            || VerticalTabUtils.getWindowWidthBoundary(widthDp)
                                    == WindowWidthBoundary.NOT_SHOWABLE) {
                        return false;
                    }
                    VerticalTabUtils.setVerticalTabsEnabled((boolean) newValue);
                    // TODO(crbug.com/559165430): Record layout toggle histogram when Settings
                    // entry point is added.
                    return true;
                });
        updateCardPreference();

        mExpandOnHoverSwitch =
                NullUtil.assertNonNull(findPreference(PREF_EXPAND_TABS_ON_HOVER_SWITCH));
        mExpandOnHoverSwitch.setOnPreferenceChangeListener(
                (preference, newValue) -> {
                    VerticalTabUtils.setExpandOnHoverEnabled(
                            (boolean) newValue, ExpandOnHoverToggleEntryPoint.SETTINGS);
                    return true;
                });
        updateExpandOnHoverSwitch();

        mPrefsListener =
                (sharedPreferences, key) -> {
                    if (ChromePreferenceKeys.VERTICAL_TABS_ENABLED.equals(key)) {
                        updateCardPreference();
                        updateExpandOnHoverSwitch();
                    } else if (ChromePreferenceKeys.VERTICAL_TABS_EXPAND_ON_HOVER.equals(key)) {
                        updateExpandOnHoverSwitch();
                    }
                };
        ContextUtils.getAppSharedPreferences()
                .registerOnSharedPreferenceChangeListener(mPrefsListener);
    }

    @Override
    @SuppressWarnings("UseSharedPreferencesManagerFromChromeCheck")
    public void onDestroy() {
        super.onDestroy();
        if (mPrefsListener != null) {
            ContextUtils.getAppSharedPreferences()
                    .unregisterOnSharedPreferenceChangeListener(mPrefsListener);
            mPrefsListener = null;
        }
    }

    @Override
    public void onStart() {
        super.onStart();
        updateCardPreference();
        updateExpandOnHoverSwitch();
    }

    @Override
    public MonotonicObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }

    @Override
    public boolean hasDivider() {
        return false;
    }

    @Override
    public @AnimationType int getAnimationType() {
        return AnimationType.PROPERTY;
    }

    private void updateCardPreference() {
        if (mCardPreference == null) {
            return;
        }
        boolean isVertical = VerticalTabUtils.isVerticalTabsEnabled(getContext());
        mCardPreference.setCheckedState(isVertical);
    }

    /**
     * Shows the expand-on-hover switch only while vertical tabs are selected and the feature is
     * available, and syncs its checked state with the user setting, which can also be changed from
     * the vertical tabs context menus.
     */
    private void updateExpandOnHoverSwitch() {
        if (mExpandOnHoverSwitch == null) {
            return;
        }
        mExpandOnHoverSwitch.setVisible(shouldShowExpandOnHoverSwitch(getContext()));
        mExpandOnHoverSwitch.setChecked(VerticalTabUtils.isExpandOnHoverEnabled());
    }

    /**
     * Returns whether the expand-on-hover switch should be shown. Used by both the UI and the
     * search index, so that the switch is only searchable while it is visible.
     */
    private static boolean shouldShowExpandOnHoverSwitch(@Nullable Context context) {
        return VerticalTabUtils.isExpandOnHoverFeatureEnabled()
                && VerticalTabUtils.isVerticalTabsEnabled(context);
    }

    @Nullable TabPositionCardPreference getCardPreferenceForTesting() {
        return mCardPreference;
    }

    @Nullable ChromeSwitchPreference getExpandOnHoverSwitchForTesting() {
        return mExpandOnHoverSwitch;
    }
}
