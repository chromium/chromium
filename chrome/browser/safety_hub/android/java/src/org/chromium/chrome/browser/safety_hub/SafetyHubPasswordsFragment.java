// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import android.os.Bundle;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.util.TraceEventVectorDrawableCompat;

/** Safety Hub subpage that displays account and local password check results. */
@NullMarked
public class SafetyHubPasswordsFragment extends SafetyHubBaseFragment {
    @VisibleForTesting static final String PREF_HEADER = "header";
    @VisibleForTesting static final String PREF_PASSWORDS_SECTION = "passwords_section";

    private final SettableMonotonicObservableSupplier<String> mPageTitle =
            ObservableSuppliers.createMonotonic();

    @Override
    public void onCreatePreferences(@Nullable Bundle bundle, @Nullable String s) {
        SettingsUtils.addPreferencesFromResource(this, R.xml.safety_hub_passwords_preferences);
        mPageTitle.set(getString(R.string.password_list_title));
        setHasOptionsMenu(true);

        // TODO(crbug.com/407931779): Populate preference list with account and local password
        // modules.
    }

    @Override
    public MonotonicObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }

    @Override
    public void onCreateOptionsMenu(Menu menu, MenuInflater inflater) {
        menu.clear();
        MenuItem help =
                menu.add(Menu.NONE, R.id.menu_id_targeted_help, Menu.NONE, getHelpMenuStringRes());
        help.setIcon(
                TraceEventVectorDrawableCompat.create(
                        getResources(), R.drawable.ic_help_24dp, getActivity().getTheme()));
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        if (item.getItemId() == R.id.menu_id_targeted_help) {
            getCustomTabLauncher().openUrlInCct(getContext(), HELP_CENTER_URL);
            return true;
        }
        return super.onOptionsItemSelected(item);
    }

    @Override
    public @AnimationType int getAnimationType() {
        return AnimationType.PROPERTY;
    }
}
