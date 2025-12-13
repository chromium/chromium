// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;

import androidx.fragment.app.Fragment;

import org.chromium.base.IntentUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.autofill.settings.AutofillPaymentMethodsFragment;
import org.chromium.chrome.browser.autofill.settings.FinancialAccountsManagementFragment;
import org.chromium.chrome.browser.autofill.settings.NonCardPaymentMethodsManagementFragment;
import org.chromium.chrome.browser.browsing_data.ClearBrowsingDataFragment;
import org.chromium.chrome.browser.safety_hub.SafetyHubFragment;
import org.chromium.chrome.browser.sync.settings.GoogleServicesSettings;
import org.chromium.chrome.browser.sync.settings.ManageSyncSettings;
import org.chromium.components.browser_ui.accessibility.AccessibilitySettings;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.components.browser_ui.site_settings.SiteSettings;

/** Implementation class for launching a {@link SettingsActivity}. */
@NullMarked
public class SettingsNavigationImpl implements SettingsNavigation {

    /** Instantiated through SettingsNavigationFactory. */
    SettingsNavigationImpl() {}

    @Override
    public void startSettings(Context context) {
        startSettings(context, SettingsFragment.MAIN);
    }

    @Override
    public void startSettings(Context context, @SettingsFragment int settingsFragment) {
        startSettings(context, settingsFragment, /* addToBackStack= */ false);
    }

    @Override
    public void startSettings(
            Context context, @SettingsFragment int settingsFragment, boolean addToBackStack) {
        Bundle fragmentArgs = null;
        switch (settingsFragment) {
            case SettingsFragment.CLEAR_BROWSING_DATA:
                fragmentArgs =
                        ClearBrowsingDataFragment.createFragmentArgs(context.getClass().getName());
                break;
            case SettingsFragment.MAIN:
            case SettingsFragment.PAYMENT_METHODS:
            case SettingsFragment.SAFETY_CHECK:
            case SettingsFragment.SITE:
            case SettingsFragment.ACCESSIBILITY:
            case SettingsFragment.GOOGLE_SERVICES:
            case SettingsFragment.MANAGE_SYNC:
            case SettingsFragment.FINANCIAL_ACCOUNTS:
            case SettingsFragment.NON_CARD_PAYMENT_METHODS:
                break;
        }
        startSettings(
                context, getFragmentClassFromEnum(settingsFragment), fragmentArgs, addToBackStack);
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
        startSettings(context, fragment, fragmentArgs, /* addToBackStack= */ false);
    }

    @Override
    public void startSettings(
            Context context,
            @Nullable Class<? extends Fragment> fragment,
            @Nullable Bundle fragmentArgs,
            boolean addToBackStack) {
        Intent intent = createSettingsIntent(context, fragment, fragmentArgs, addToBackStack);
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
        return createSettingsIntent(context, fragment, fragmentArgs, /* addToBackStack= */ false);
    }

    @Override
    public Intent createSettingsIntent(
            Context context,
            @Nullable Class<? extends Fragment> fragment,
            @Nullable Bundle fragmentArgs,
            boolean addToBackStack) {
        String fragmentName = fragment == null ? null : fragment.getName();
        return SettingsIntentUtil.createIntent(context, fragmentName, fragmentArgs, addToBackStack);
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
                return ClearBrowsingDataFragment.class;
            case SettingsFragment.PAYMENT_METHODS:
                return AutofillPaymentMethodsFragment.class;
            case SettingsFragment.SAFETY_CHECK:
                return SafetyHubFragment.class;
            case SettingsFragment.SITE:
                return SiteSettings.class;
            case SettingsFragment.ACCESSIBILITY:
                return AccessibilitySettings.class;
            case SettingsFragment.GOOGLE_SERVICES:
                return GoogleServicesSettings.class;
            case SettingsFragment.MANAGE_SYNC:
                return ManageSyncSettings.class;
            case SettingsFragment.FINANCIAL_ACCOUNTS:
                return FinancialAccountsManagementFragment.class;
            case SettingsFragment.NON_CARD_PAYMENT_METHODS:
                return NonCardPaymentMethodsManagementFragment.class;
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

    @Override
    public void executePendingNavigations(Activity activity) {
        ((SettingsActivity) activity).executePendingNavigations();
    }
}
