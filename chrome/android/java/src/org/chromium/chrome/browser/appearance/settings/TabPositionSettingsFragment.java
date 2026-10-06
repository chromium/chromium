// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.appearance.settings;

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
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.chrome.browser.settings.search.ChromeBaseSearchIndexProvider;
import org.chromium.chrome.browser.ui.vertical_tabs.VerticalTabUtils;
import org.chromium.chrome.browser.ui.vertical_tabs.VerticalTabUtils.ExpandOnHoverToggleEntryPoint;
import org.chromium.chrome.browser.ui.vertical_tabs.VerticalTabUtils.WindowWidthBoundary;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.CustomDividerFragment;
import org.chromium.components.browser_ui.settings.SettingsUtils;

/** Fragment to manage tab position settings (Horizontal vs. Vertical). */
@NullMarked
public class TabPositionSettingsFragment extends ChromeBaseSettingsFragment
        implements CustomDividerFragment {
    public static final String PREF_TAB_POSITION_CARD_SELECTOR = "tab_position_card_selector";
    public static final String PREF_EXPAND_TABS_ON_HOVER_SWITCH = "expand_tabs_on_hover_switch";

    // TODO(crbug.com/542280452): Make the expand-on-hover switch searchable: index
    // R.xml.tab_position_preferences and, in updateDynamicPreferences(), remove the entries for
    // the description, the card selector, and the switch while it is hidden (i.e. vertical tabs
    // are not selected or the "expand_on_hover" feature param is off).
    public static final ChromeBaseSearchIndexProvider SEARCH_INDEX_DATA_PROVIDER =
            new ChromeBaseSearchIndexProvider(
                    TabPositionSettingsFragment.class.getName(),
                    ChromeBaseSearchIndexProvider.INDEX_OPT_OUT);

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
        mExpandOnHoverSwitch.setVisible(
                VerticalTabUtils.isExpandOnHoverFeatureEnabled()
                        && VerticalTabUtils.isVerticalTabsEnabled(getContext()));
        mExpandOnHoverSwitch.setChecked(VerticalTabUtils.isExpandOnHoverEnabled());
    }

    @Nullable TabPositionCardPreference getCardPreferenceForTesting() {
        return mCardPreference;
    }

    @Nullable ChromeSwitchPreference getExpandOnHoverSwitchForTesting() {
        return mExpandOnHoverSwitch;
    }
}
