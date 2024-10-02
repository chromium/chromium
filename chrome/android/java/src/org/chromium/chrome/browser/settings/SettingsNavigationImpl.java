// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;

import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;

import org.chromium.base.IntentUtils;
import org.chromium.chrome.browser.accessibility.settings.AccessibilitySettings;
import org.chromium.chrome.browser.autofill.settings.AutofillPaymentMethodsFragment;
import org.chromium.chrome.browser.autofill.settings.FinancialAccountsManagementFragment;
import org.chromium.chrome.browser.browsing_data.ClearBrowsingDataFragment;
import org.chromium.chrome.browser.browsing_data.ClearBrowsingDataFragmentAdvanced;
import org.chromium.chrome.browser.browsing_data.ClearBrowsingDataTabsFragment;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.password_manager.settings.PasswordSettings;
import org.chromium.chrome.browser.safety_check.SafetyCheckSettingsFragment;
import org.chromium.chrome.browser.safety_hub.SafetyHubFragment;
import org.chromium.chrome.browser.sync.settings.GoogleServicesSettings;
import org.chromium.chrome.browser.sync.settings.ManageSyncSettings;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.components.browser_ui.site_settings.SiteSettings;

/** Implementation class for launching a {@link SettingsActivity}. */
public class SettingsNavigationImpl implements SettingsNavigation {

    /** Instantiated through SettingsNavigationFactory. */
    SettingsNavigationImpl() {}

    @Override
    public void startSettings(Context context) {
        startSettings(context, SettingsFragment.MAIN);
    }

    @Override
    public void startSettings(Context context, @SettingsFragment int settingsFragment) {
        Bundle fragmentArgs = null;
        switch (settingsFragment) {
            case SettingsFragment.CLEAR_BROWSING_DATA:
                fragmentArgs =
                        ClearBrowsingDataTabsFragment.createFragmentArgs(
                                context.getClass().getName());
                break;
            case SettingsFragment.CLEAR_BROWSING_DATA_ADVANCED_PAGE:
                fragmentArgs =
                        ClearBrowsingDataFragment.createFragmentArgs(
                                context.getClass().getName(),
                                /* isFetcherSuppliedFromOutside= */ false);
                break;
            case SettingsFragment.SAFETY_CHECK:
                if (!ChromeFeatureList.sSafetyHub.isEnabled()) {
                    fragmentArgs = SafetyCheckSettingsFragment.createBundle(true);
                }
                break;
            case SettingsFragment.MAIN:
            case SettingsFragment.PAYMENT_METHODS:
            case SettingsFragment.SITE:
            case SettingsFragment.ACCESSIBILITY:
            case SettingsFragment.PASSWORDS:
            case SettingsFragment.GOOGLE_SERVICES:
            case SettingsFragment.MANAGE_SYNC:
                break;
        }
        startSettings(context, getFragmentClassFromEnum(settingsFragment), fragmentArgs);
    }

    @Override
    public void startSettings(Context context, @Nullable Class<? extends Fragment> fragment) {
        startSettings(context, fragment, null);
    }

    @Override
    public void startSettings(
            Context context,
            @Nullable Class<? extends Fragment> fragment,
            @Nullable Bundle fragmentArgs) {
        Intent intent = createSettingsIntent(context, fragment, fragmentArgs);
        IntentUtils.safeStartActivity(context, intent);
    }

    @Override
    public Intent createSettingsIntent(
            Context context, @Nullable Class<? extends Fragment> fragment) {
        return createSettingsIntent(context, fragment, null);
    }

    @Override
    public Intent createSettingsIntent(
            Context context,
            @Nullable Class<? extends Fragment> fragment,
            @Nullable Bundle fragmentArgs) {
        String fragmentName = fragment == null ? null : fragment.getName();
        return SettingsIntentUtil.createIntent(context, fragmentName, fragmentArgs);
    }

    @Override
    public Intent createSettingsIntent(
            Context context, @SettingsFragment int fragment, @Nullable Bundle fragmentArgs) {
        return createSettingsIntent(context, getFragmentClassFromEnum(fragment), fragmentArgs);
    }

    private static @Nullable Class<? extends Fragment> getFragmentClassFromEnum(
            @SettingsFragment int fragment) {
        switch (fragment) {
            case SettingsFragment.MAIN:
                return null;
            case SettingsFragment.CLEAR_BROWSING_DATA:
                return ClearBrowsingDataTabsFragment.class;
            case SettingsFragment.CLEAR_BROWSING_DATA_ADVANCED_PAGE:
                return ClearBrowsingDataFragmentAdvanced.class;
            case SettingsFragment.PAYMENT_METHODS:
                return AutofillPaymentMethodsFragment.class;
            case SettingsFragment.SAFETY_CHECK:
                if (ChromeFeatureList.sSafetyHub.isEnabled()) {
                    return SafetyHubFragment.class;
                } else {
                    return SafetyCheckSettingsFragment.class;
                }
            case SettingsFragment.SITE:
                return SiteSettings.class;
            case SettingsFragment.ACCESSIBILITY:
                return AccessibilitySettings.class;
            case SettingsFragment.PASSWORDS:
                return PasswordSettings.class;
            case SettingsFragment.GOOGLE_SERVICES:
                return GoogleServicesSettings.class;
            case SettingsFragment.MANAGE_SYNC:
                return ManageSyncSettings.class;
            case SettingsFragment.FINANCIAL_ACCOUNTS:
                return FinancialAccountsManagementFragment.class;
        }
        assert false;
        return null;
    }

    @Override
    public void finishCurrentSettings(Fragment fragment) {
        Activity activity = fragment.getActivity();
        if (activity != null) {
            ((SettingsActivity) activity).finishCurrentSettings(fragment);
        }
    }
}
