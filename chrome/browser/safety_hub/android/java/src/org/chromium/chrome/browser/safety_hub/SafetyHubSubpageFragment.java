// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.preference.PreferenceCategory;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.settings.TextMessagePreference;
import org.chromium.ui.widget.ButtonCompat;

/* Common ancestor for Safety Hub subpage fragments. */
public abstract class SafetyHubSubpageFragment extends SafetyHubBaseFragment {
    private static final String PREF_HEADER = "header";
    private static final String PREF_LIST = "preference_list";

    protected PreferenceCategory mPreferenceList;
    protected ButtonCompat mBottomButton;
    protected boolean mBulkActionConfirmed;
    private final ObservableSupplierImpl<String> mPageTitle = new ObservableSupplierImpl<>();

    @Override
    public void onCreatePreferences(@Nullable Bundle bundle, @Nullable String s) {
        SettingsUtils.addPreferencesFromResource(this, R.xml.safety_hub_subpage_preferences);
        mPageTitle.set(getString(getTitleId()));

        TextMessagePreference headPreference = (TextMessagePreference) findPreference(PREF_HEADER);
        headPreference.setSummary(getHeaderId());

        mPreferenceList = findPreference(PREF_LIST);
        mPreferenceList.setTitle(getPermissionsListTextId());
        setHasOptionsMenu(true);
    }

    @Override
    public ObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }

    @NonNull
    @Override
    public View onCreateView(
            @NonNull LayoutInflater inflater,
            @Nullable ViewGroup container,
            @Nullable Bundle savedInstanceState) {
        LinearLayout view =
                (LinearLayout) super.onCreateView(inflater, container, savedInstanceState);
        LinearLayout bottomView =
                (LinearLayout) inflater.inflate(R.layout.safety_hub_bottom_elements, view, false);
        mBottomButton = bottomView.findViewById(R.id.safety_hub_permissions_button);
        mBottomButton.setText(getButtonTextId());
        mBottomButton.setOnClickListener(
                new View.OnClickListener() {
                    @Override
                    public void onClick(View view) {
                        mBulkActionConfirmed = true;
                        SettingsNavigationFactory.createSettingsNavigation()
                                .finishCurrentSettings(SafetyHubSubpageFragment.this);
                    }
                });
        view.addView(bottomView);
        return view;
    }

    @Override
    public void onResume() {
        super.onResume();
        updatePreferenceList();
    }

    @Override
    public void onCreateOptionsMenu(Menu menu, MenuInflater inflater) {
        menu.clear();
        MenuItem item =
                menu.add(
                        Menu.NONE,
                        R.id.safety_hub_subpage_menu_item,
                        Menu.NONE,
                        getMenuItemTextId());
        item.setShowAsAction(MenuItem.SHOW_AS_ACTION_NEVER);
    }

    protected abstract @StringRes int getTitleId();

    protected abstract @StringRes int getHeaderId();

    protected abstract @StringRes int getButtonTextId();

    protected abstract @StringRes int getMenuItemTextId();

    protected abstract @StringRes int getPermissionsListTextId();

    protected abstract void updatePreferenceList();
}
