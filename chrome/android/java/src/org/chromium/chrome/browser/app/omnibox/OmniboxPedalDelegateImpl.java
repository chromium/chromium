// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.omnibox;

import android.app.Activity;

import androidx.annotation.DrawableRes;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.accessibility.settings.AccessibilitySettings;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.autofill.settings.AutofillPaymentMethodsFragment;
import org.chromium.chrome.browser.browsing_data.ClearBrowsingDataTabsFragment;
import org.chromium.chrome.browser.omnibox.action.OmniboxPedalType;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxPedalDelegate;
import org.chromium.chrome.browser.password_manager.ManagePasswordsReferrer;
import org.chromium.chrome.browser.password_manager.PasswordManagerLauncher;
import org.chromium.chrome.browser.safety_check.SafetyCheckSettingsFragment;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.browser_ui.site_settings.SiteSettings;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.PageTransition;

/**
 * Handle the clicks on the {@link OmniboxPedal}.
 */
public class OmniboxPedalDelegateImpl implements OmniboxPedalDelegate {
    private Activity mActivity;
    public OmniboxPedalDelegateImpl(Activity activity) {
        mActivity = activity;
    }

    @Override
    public void executeAction(@OmniboxPedalType int omniboxActionType) {
        SettingsLauncher settingsLauncher = new SettingsLauncherImpl();
        switch (omniboxActionType) {
            case OmniboxPedalType.CLEAR_BROWSING_DATA:
                settingsLauncher.launchSettingsActivity(
                        mActivity, ClearBrowsingDataTabsFragment.class);
                break;
            case OmniboxPedalType.MANAGE_PASSWORDS:
                PasswordManagerLauncher.showPasswordSettings(
                        mActivity, ManagePasswordsReferrer.CHROME_SETTINGS);
                break;
            case OmniboxPedalType.UPDATE_CREDIT_CARD:
                settingsLauncher.launchSettingsActivity(
                        mActivity, AutofillPaymentMethodsFragment.class);
                break;
            case OmniboxPedalType.LAUNCH_INCOGNITO:
                if (mActivity instanceof ChromeActivity) {
                    ((ChromeActivity) mActivity)
                            .onMenuOrKeyboardAction(
                                    R.id.new_incognito_tab_menu_id, /*fromMenu*/ false);
                }
                break;
            case OmniboxPedalType.RUN_CHROME_SAFETY_CHECK:
                settingsLauncher.launchSettingsActivity(
                        mActivity, SafetyCheckSettingsFragment.class);
                break;
            case OmniboxPedalType.MANAGE_SITE_SETTINGS:
                settingsLauncher.launchSettingsActivity(mActivity, SiteSettings.class);
                break;
            case OmniboxPedalType.MANAGE_CHROME_SETTINGS:
                settingsLauncher.launchSettingsActivity(mActivity);
                break;
            case OmniboxPedalType.VIEW_CHROME_HISTORY:
                if (mActivity instanceof ChromeActivity) {
                    ((ChromeActivity) mActivity)
                            .onMenuOrKeyboardAction(R.id.open_history_menu_id, /*fromMenu*/ false);
                }
                break;
            case OmniboxPedalType.MANAGE_CHROME_ACCESSIBILITY:
                settingsLauncher.launchSettingsActivity(mActivity, AccessibilitySettings.class);
                break;
            case OmniboxPedalType.PLAY_CHROME_DINO_GAME:
                if (mActivity instanceof ChromeActivity) {
                    ((ChromeActivity) mActivity)
                            .getActivityTab()
                            .loadUrl(new LoadUrlParams(
                                    UrlConstants.CHROME_DINO_URL, PageTransition.GENERATED));
                }
                break;
        }
        return;
    }

    @Override
    public @DrawableRes int getPedalIcon(@OmniboxPedalType int omniboxActionType) {
        switch (omniboxActionType) {
            case OmniboxPedalType.CLEAR_BROWSING_DATA:
            case OmniboxPedalType.MANAGE_PASSWORDS:
            case OmniboxPedalType.UPDATE_CREDIT_CARD:
            case OmniboxPedalType.LAUNCH_INCOGNITO:
            case OmniboxPedalType.RUN_CHROME_SAFETY_CHECK:
            case OmniboxPedalType.MANAGE_SITE_SETTINGS:
            case OmniboxPedalType.MANAGE_CHROME_SETTINGS:
            case OmniboxPedalType.VIEW_CHROME_HISTORY:
            case OmniboxPedalType.MANAGE_CHROME_ACCESSIBILITY:
                return R.drawable.fre_product_logo;
            case OmniboxPedalType.PLAY_CHROME_DINO_GAME:
                return R.drawable.ic_dino;
            default:
                // Please confirm the icon for the new pedals in
                // chrome/browser/ui/omnibox/omnibox_pedal_implementations.cc, if the new pedals use
                // a spicial icon.
                assert false : "New pedals need to confirm the icon and add the list above.";
                break;
        }
        return R.drawable.fre_product_logo;
    }
}
