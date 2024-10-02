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

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.browser.customtabs.CustomTabsIntent;
import androidx.fragment.app.Fragment;

import org.chromium.base.Callback;
import org.chromium.base.IntentUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.util.TraceEventVectorDrawableCompat;

/**
 * Base class for PrivacySandboxSettings related Fragments. Initializes the options menu to open a
 * help page about the PrivacySandbox instead of the regular help center.
 *
 * <p>Subclasses have to call super.onCreatePreferences(bundle, s) when overriding
 * onCreatePreferences.
 */
public abstract class PrivacySandboxSettingsBaseFragment extends ChromeBaseSettingsFragment {
    // Key for the argument with which the PrivacySandbox fragment will be launched. The value for
    // this argument should be part of the PrivacySandboxReferrer enum, which contains all points of
    // entry to the Privacy Sandbox UI.
    public static final String PRIVACY_SANDBOX_REFERRER = "privacy-sandbox-referrer";

    private PrivacySandboxBridge mPrivacySandboxBridge;
    private PrivacySandboxHelpers.CustomTabIntentHelper mCustomTabHelper;
    private OneshotSupplier<SnackbarManager> mSnackbarManagerSupplier;
    private Callback<Context> mCookieSettingsNavigation;

    /** Launches the right version of PrivacySandboxSettings depending on feature flags. */
    public static void launchPrivacySandboxSettings(
            Context context, @PrivacySandboxReferrer int referrer) {
        Bundle fragmentArgs = new Bundle();
        fragmentArgs.putInt(PRIVACY_SANDBOX_REFERRER, referrer);
        SettingsNavigationFactory.createSettingsNavigation()
                .startSettings(context, PrivacySandboxSettingsFragment.class, fragmentArgs);
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
        help.setIcon(
                TraceEventVectorDrawableCompat.create(
                        getResources(), R.drawable.ic_help_and_feedback, getActivity().getTheme()));
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        if (item.getItemId() == R.id.menu_id_targeted_help) {
            // Action for the question mark button.
            openUrlInCct(PrivacySandboxSettingsFragment.HELP_CENTER_URL);
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
        Intent intent =
                mCustomTabHelper.createCustomTabActivityIntent(
                        getContext(), customTabIntent.intent);
        intent.setPackage(getContext().getPackageName());
        intent.putExtra(Browser.EXTRA_APPLICATION_ID, getContext().getPackageName());
        IntentUtils.addTrustedIntentExtras(intent);
        IntentUtils.safeStartActivity(getContext(), intent);
    }

    public void setSnackbarManagerSupplier(
            OneshotSupplier<SnackbarManager> snackbarManagerSupplier) {
        mSnackbarManagerSupplier = snackbarManagerSupplier;
    }

    protected void showSnackbar(
            int stringResId,
            SnackbarManager.SnackbarController controller,
            int type,
            int identifier) {
        showSnackbar(stringResId, controller, type, identifier, 0, false);
    }

    protected void showSnackbar(
            int stringResId,
            SnackbarManager.SnackbarController controller,
            int type,
            int identifier,
            int actionStringResId,
            boolean multiLine) {
        var snackbar =
                Snackbar.make(getResources().getString(stringResId), controller, type, identifier);
        if (actionStringResId != 0) {
            snackbar.setAction(getResources().getString(actionStringResId), null);
        }
        if (multiLine) snackbar.setSingleLine(false);
        mSnackbarManagerSupplier.get().showSnackbar(snackbar);
    }

    protected void parseAndRecordReferrer() {
        Bundle extras = getArguments();
        assert (extras != null) && extras.containsKey(PRIVACY_SANDBOX_REFERRER)
                : "PrivacySandboxSettingsFragment must be launched with a privacy-sandbox-referrer "
                        + "fragment argument, but none was provided.";
        int referrer = extras.getInt(PRIVACY_SANDBOX_REFERRER);
        // Record all the referrer metrics.
        RecordHistogram.recordEnumeratedHistogram(
                "Settings.PrivacySandbox.PrivacySandboxReferrer",
                referrer,
                PrivacySandboxReferrer.COUNT);
        if (referrer == PrivacySandboxReferrer.PRIVACY_SETTINGS) {
            RecordUserAction.record("Settings.PrivacySandbox.OpenedFromSettingsParent");
        } else if (referrer == PrivacySandboxReferrer.COOKIES_SNACKBAR) {
            RecordUserAction.record("Settings.PrivacySandbox.OpenedFromCookiesPageToast");
        } else if (referrer == PrivacySandboxReferrer.PAGE_INFO_AD_PRIVACY_SECTION) {
            RecordUserAction.record("PageInfo.AdPersonalization.ManageInterestClicked");
        }
    }

    protected void startSettings(Class<? extends Fragment> fragment) {
        SettingsNavigationFactory.createSettingsNavigation().startSettings(getContext(), fragment);
    }

    @Override
    public void setProfile(@NonNull Profile profile) {
        super.setProfile(profile);
        mPrivacySandboxBridge = new PrivacySandboxBridge(profile);
    }

    /**
     * Return the {@link PrivacySandboxBridge} associated with the value set in {@link
     * #setProfile(Profile)}.
     */
    public PrivacySandboxBridge getPrivacySandboxBridge() {
        assert mPrivacySandboxBridge != null
                : "Attempting to use PrivacySandboxBridge prior to setProfile being called.";
        return mPrivacySandboxBridge;
    }

    protected void launchCookieSettings() {
        if (mCookieSettingsNavigation != null) {
            mCookieSettingsNavigation.onResult(getContext());
        }
    }

    public void setCookieSettingsIntentHelper(Callback<Context> cookieSettingsNavigation) {
        mCookieSettingsNavigation = cookieSettingsNavigation;
    }
}
