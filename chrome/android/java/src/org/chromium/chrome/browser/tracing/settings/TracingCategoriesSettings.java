// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tracing.settings;

import android.content.Context;
import android.os.Bundle;
import android.text.TextUtils;

import androidx.preference.CheckBoxPreference;
import androidx.preference.Preference;
import androidx.preference.PreferenceFragmentCompat;
import androidx.preference.PreferenceScreen;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.browser.tracing.TracingController;
import org.chromium.components.browser_ui.settings.ChromeBaseCheckBoxPreference;
import org.chromium.components.browser_ui.settings.EmbeddableSettingsPage;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * Settings fragment that configures chrome tracing categories of a specific type. The type is
 * passed to the fragment via an extra (EXTRA_CATEGORY_TYPE).
 */
public class TracingCategoriesSettings extends PreferenceFragmentCompat
        implements EmbeddableSettingsPage, Preference.OnPreferenceChangeListener {
    public static final String EXTRA_CATEGORY_TYPE = "type";

    // Non-translated strings:
    private static final String MSG_CATEGORY_SELECTION_TITLE = "Select categories";
    private static final ObservableSupplier<String> sPageTitle =
            new ObservableSupplierImpl<>(MSG_CATEGORY_SELECTION_TITLE);

    private static final String SELECT_ALL_KEY = "select-all";
    private static final String SELECT_ALL_TITLE = "Select all";

    private @TracingSettings.CategoryType int mType;
    private Set<String> mEnabledCategories;
    private List<CheckBoxPreference> mAllPreferences;
    private CheckBoxPreference mSelectAllPreference;

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        PreferenceScreen preferenceScreen =
                getPreferenceManager().createPreferenceScreen(getStyledContext());
        preferenceScreen.setOrderingAsAdded(true);

        mType = getArguments().getInt(EXTRA_CATEGORY_TYPE);
        mEnabledCategories = new HashSet<>(TracingSettings.getEnabledCategories(mType));
        mAllPreferences = new ArrayList<>();

        List<String> sortedCategories =
                new ArrayList<>(TracingController.getInstance().getKnownCategories());
        Collections.sort(sortedCategories);

        // Special preference to select all or deselect the entire list.
        mSelectAllPreference = new ChromeBaseCheckBoxPreference(getStyledContext(), null);
        mSelectAllPreference.setKey(SELECT_ALL_KEY);
        mSelectAllPreference.setTitle(SELECT_ALL_TITLE);
        mSelectAllPreference.setPersistent(false);
        mSelectAllPreference.setOnPreferenceChangeListener(this);
        preferenceScreen.addPreference(mSelectAllPreference);

        for (String category : sortedCategories) {
            if (TracingSettings.getCategoryType(category) == mType) {
                CheckBoxPreference pref = createPreference(category);
                mAllPreferences.add(pref);
                preferenceScreen.addPreference(pref);
            }
        }
        mSelectAllPreference.setChecked(mEnabledCategories.size() == mAllPreferences.size());
        setPreferenceScreen(preferenceScreen);
    }

    @Override
    public ObservableSupplier<String> getPageTitle() {
        return sPageTitle;
    }

    private CheckBoxPreference createPreference(String category) {
        CheckBoxPreference preference = new ChromeBaseCheckBoxPreference(getStyledContext(), null);
        preference.setKey(category);
        preference.setTitle(
                category.startsWith(TracingSettings.NON_DEFAULT_CATEGORY_PREFIX)
                        ? category.substring(TracingSettings.NON_DEFAULT_CATEGORY_PREFIX.length())
                        : category);
        preference.setChecked(mEnabledCategories.contains(category));
        preference.setPersistent(false); // We persist the preference value ourselves.
        preference.setOnPreferenceChangeListener(this);
        return preference;
    }

    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        boolean value = (boolean) newValue;
        if (TextUtils.equals(SELECT_ALL_KEY, preference.getKey())) {
            setStateForAllPreferences(value);
            return true;
        }

        if (value) {
            mEnabledCategories.add(preference.getKey());
        } else {
            mEnabledCategories.remove(preference.getKey());
        }
        mSelectAllPreference.setChecked(mEnabledCategories.size() == mAllPreferences.size());
        TracingSettings.setEnabledCategories(mType, mEnabledCategories);
        return true;
    }

    private Context getStyledContext() {
        return getPreferenceManager().getContext();
    }

    private void setStateForAllPreferences(boolean enabled) {
        for (CheckBoxPreference pref : mAllPreferences) {
            pref.setChecked(enabled);
            pref.callChangeListener(pref.isChecked());
        }
    }
}
