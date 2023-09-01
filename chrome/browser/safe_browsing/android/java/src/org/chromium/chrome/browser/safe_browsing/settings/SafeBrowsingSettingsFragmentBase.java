// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safe_browsing.settings;

import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.provider.Browser;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;

import androidx.browser.customtabs.CustomTabsIntent;
import androidx.preference.PreferenceFragmentCompat;

import org.chromium.base.IntentUtils;
import org.chromium.chrome.browser.feedback.FragmentHelpAndFeedbackLauncher;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncher;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.util.TraceEventVectorDrawableCompat;

/**
 * The base fragment class for Safe Browsing settings fragments.
 */
public abstract class SafeBrowsingSettingsFragmentBase
        extends PreferenceFragmentCompat implements FragmentHelpAndFeedbackLauncher {
    private HelpAndFeedbackLauncher mHelpAndFeedbackLauncher;
    private SafeBrowsingSettingsFragmentHelper.CustomTabIntentHelper mCustomTabHelper;

    @Override
    public void onCreatePreferences(Bundle bundle, String s) {
        SettingsUtils.addPreferencesFromResource(this, getPreferenceResource());
        getActivity().setTitle(R.string.prefs_section_safe_browsing_title);

        onCreatePreferencesInternal(bundle, s);

        setHasOptionsMenu(true);
    }

    @Override
    public void setHelpAndFeedbackLauncher(HelpAndFeedbackLauncher helpAndFeedbackLauncher) {
        mHelpAndFeedbackLauncher = helpAndFeedbackLauncher;
    }

    /**
     * Set the necessary CCT helpers to be able to natively open links. This is needed because the
     * helpers are not modularized.
     */
    public void setCustomTabIntentHelper(
            SafeBrowsingSettingsFragmentHelper.CustomTabIntentHelper tabHelper) {
        mCustomTabHelper = tabHelper;
    }

    protected void openUrlInCct(String url) {
        assert (mCustomTabHelper != null)
            : "CCT helpers must be set on SafeBrowsingSettingsFragmentBase before opening a "
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

    @Override
    public void onCreateOptionsMenu(Menu menu, MenuInflater inflater) {
        menu.clear();
        MenuItem help =
                menu.add(Menu.NONE, R.id.menu_id_targeted_help, Menu.NONE, R.string.menu_help);
        help.setIcon(TraceEventVectorDrawableCompat.create(
                getResources(), R.drawable.ic_help_and_feedback, getActivity().getTheme()));
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        if (item.getItemId() != R.id.menu_id_targeted_help) {
            return false;
        }
        mHelpAndFeedbackLauncher.show(
                getActivity(), getString(R.string.help_context_safe_browsing), null);
        return true;
    }

    /**
     * Called within {@link SafeBrowsingSettingsFragmentBase#onCreatePreferences(Bundle, String)}.
     * If the child class needs to handle specific logic during preference creation, it can override
     * this method.
     */
    protected void onCreatePreferencesInternal(Bundle bundle, String s) {}

    /**
     * @return The resource id of the preference.
     */
    protected abstract int getPreferenceResource();
}
