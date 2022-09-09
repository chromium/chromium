// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.privacy_sandbox;

import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.provider.Browser;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;

import androidx.annotation.Nullable;
import androidx.browser.customtabs.CustomTabsIntent;
import androidx.preference.PreferenceFragmentCompat;
import androidx.vectordrawable.graphics.drawable.VectorDrawableCompat;

import org.chromium.base.IntentUtils;

/**
 * Base class for PrivacySandboxSettings related Fragments. Initializes the options menu to
 * open a help page about the PrivacySandbox instead of the regular help center.
 *
 * Subclasses have to call super.onCreatePreferences(bundle, s) when overriding onCreatePreferences.
 */
public abstract class PrivacySandboxSettingsBaseFragment extends PreferenceFragmentCompat {
    private PrivacySandboxHelpers.CustomTabIntentHelper mCustomTabHelper;

    @Override
    public void onCreatePreferences(@Nullable Bundle bundle, @Nullable String s) {
        // Enable the options menu to be able to use a custom question mark button.
        setHasOptionsMenu(true);
    }

    @Override
    public void onCreateOptionsMenu(Menu menu, MenuInflater inflater) {
        // Add the custom question mark button.
        menu.clear();
        MenuItem help =
                menu.add(Menu.NONE, R.id.menu_id_targeted_help, Menu.NONE, R.string.menu_help);
        help.setIcon(VectorDrawableCompat.create(
                getResources(), R.drawable.ic_help_and_feedback, getActivity().getTheme()));
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        if (item.getItemId() == R.id.menu_id_targeted_help) {
            // Action for the question mark button.
            openUrlInCct(PrivacySandboxSettingsFragment.PRIVACY_SANDBOX_URL);
            return true;
        }
        return false;
    }

    /**
     * Set the necessary CCT helpers to be able to natively open links. This is needed because the
     * helpers are not modularized.
     */
    public void setCustomTabIntentHelper(PrivacySandboxHelpers.CustomTabIntentHelper tabHelper) {
        mCustomTabHelper = tabHelper;
    }

    protected void openUrlInCct(String url) {
        assert (mCustomTabHelper != null)
            : "CCT helpers must be set on PrivacySandboxSettingsFragment before opening a "
              + "link.";
        CustomTabsIntent customTabIntent =
                new CustomTabsIntent.Builder().setShowTitle(true).build();
        customTabIntent.intent.setData(Uri.parse(url));
        Intent intent = mCustomTabHelper.createCustomTabActivityIntent(
                getContext(), customTabIntent.intent);
        intent.setPackage(getContext().getPackageName());
        intent.putExtra(Browser.EXTRA_APPLICATION_ID, getContext().getPackageName());
        IntentUtils.addTrustedIntentExtras(intent);
        IntentUtils.safeStartActivity(getContext(), intent);
    }
}
