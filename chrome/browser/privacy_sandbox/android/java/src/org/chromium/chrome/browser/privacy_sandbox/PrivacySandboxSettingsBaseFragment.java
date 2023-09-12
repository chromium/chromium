// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.privacy_sandbox;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.provider.Browser;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;

import androidx.annotation.Nullable;
import androidx.browser.customtabs.CustomTabsIntent;
import androidx.fragment.app.Fragment;

import org.chromium.base.Callback;
import org.chromium.base.IntentUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.privacy_sandbox.v4.PrivacySandboxSettingsFragmentV4;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.settings.FragmentSettingsLauncher;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.browser_ui.util.TraceEventVectorDrawableCompat;

/**
 * Base class for PrivacySandboxSettings related Fragments. Initializes the options menu to
 * open a help page about the PrivacySandbox instead of the regular help center.
 *
 * Subclasses have to call super.onCreatePreferences(bundle, s) when overriding onCreatePreferences.
 */
public abstract class PrivacySandboxSettingsBaseFragment
        extends ChromeBaseSettingsFragment implements FragmentSettingsLauncher {
    // Key for the argument with which the PrivacySandbox fragment will be launched. The value for
    // this argument should be part of the PrivacySandboxReferrer enum, which contains all points of
    // entry to the Privacy Sandbox UI.
    public static final String PRIVACY_SANDBOX_REFERRER = "privacy-sandbox-referrer";

    private PrivacySandboxHelpers.CustomTabIntentHelper mCustomTabHelper;
    private SettingsLauncher mSettingsLauncher;
    private SnackbarManager mSnackbarManager;
    private Callback<Context> mCookieSettingsLauncher;

    /**
     * Launches the right version of PrivacySandboxSettings depending on feature flags.
     */
    public static void launchPrivacySandboxSettings(Context context,
            SettingsLauncher settingsLauncher, @PrivacySandboxReferrer int referrer) {
        Bundle fragmentArgs = new Bundle();
        fragmentArgs.putInt(PRIVACY_SANDBOX_REFERRER, referrer);
        var fragment = ChromeFeatureList.isEnabled(ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_4)
                ? PrivacySandboxSettingsFragmentV4.class
                : PrivacySandboxSettingsFragmentV3.class;
        settingsLauncher.launchSettingsActivity(context, fragment, fragmentArgs);
    }

    public static CharSequence getStatusString(Context context) {
        return context.getString(PrivacySandboxBridge.isPrivacySandboxEnabled()
                        ? R.string.privacy_sandbox_status_enabled
                        : R.string.privacy_sandbox_status_disabled);
    }

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
        help.setIcon(TraceEventVectorDrawableCompat.create(
                getResources(), R.drawable.ic_help_and_feedback, getActivity().getTheme()));
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        if (item.getItemId() == R.id.menu_id_targeted_help) {
            // Action for the question mark button.
            openUrlInCct(ChromeFeatureList.isEnabled(ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_4)
                            ? PrivacySandboxSettingsFragmentV4.HELP_CENTER_URL
                            : PrivacySandboxSettingsFragmentV3.PRIVACY_SANDBOX_URL);
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

    public void setSnackbarManager(SnackbarManager snackbarManager) {
        mSnackbarManager = snackbarManager;
    }

    protected void showSnackbar(int stringResId, SnackbarManager.SnackbarController controller,
            int type, int identifier) {
        mSnackbarManager.showSnackbar(
                Snackbar.make(getResources().getString(stringResId), controller, type, identifier));
    }

    protected void parseAndRecordReferrer() {
        Bundle extras = getArguments();
        assert (extras != null)
                && extras.containsKey(PRIVACY_SANDBOX_REFERRER)
            : "PrivacySandboxSettingsFragment must be launched with a privacy-sandbox-referrer "
                        + "fragment argument, but none was provided.";
        int referrer = extras.getInt(PRIVACY_SANDBOX_REFERRER);
        // Record all the referrer metrics.
        RecordHistogram.recordEnumeratedHistogram("Settings.PrivacySandbox.PrivacySandboxReferrer",
                referrer, PrivacySandboxReferrer.COUNT);
        if (referrer == PrivacySandboxReferrer.PRIVACY_SETTINGS) {
            RecordUserAction.record("Settings.PrivacySandbox.OpenedFromSettingsParent");
        } else if (referrer == PrivacySandboxReferrer.COOKIES_SNACKBAR) {
            RecordUserAction.record("Settings.PrivacySandbox.OpenedFromCookiesPageToast");
        } else if (referrer == PrivacySandboxReferrer.PAGE_INFO_AD_PRIVACY_SECTION) {
            RecordUserAction.record("PageInfo.AdPersonalization.ManageInterestClicked");
        }
    }

    protected void launchSettingsActivity(Class<? extends Fragment> fragment) {
        if (mSettingsLauncher != null) {
            mSettingsLauncher.launchSettingsActivity(getContext(), fragment);
        }
    }

    @Override
    public void setSettingsLauncher(SettingsLauncher settingsLauncher) {
        mSettingsLauncher = settingsLauncher;
    }

    protected void launchCookieSettings() {
        if (mCookieSettingsLauncher != null) {
            mCookieSettingsLauncher.onResult(getContext());
        }
    }

    public void setCookieSettingsIntentHelper(Callback<Context> cookieSettingsLauncher) {
        mCookieSettingsLauncher = cookieSettingsLauncher;
    }
}
