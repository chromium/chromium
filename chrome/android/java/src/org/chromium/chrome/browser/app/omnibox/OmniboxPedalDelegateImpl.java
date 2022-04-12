// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.omnibox;

import android.app.Activity;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;

import androidx.annotation.NonNull;

import org.chromium.base.IntentUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.autofill.settings.AutofillPaymentMethodsFragment;
import org.chromium.chrome.browser.browserservices.intents.WebappConstants;
import org.chromium.chrome.browser.browsing_data.ClearBrowsingDataTabsFragment;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.history.HistoryActivity;
import org.chromium.chrome.browser.omnibox.action.OmniboxActionType;
import org.chromium.chrome.browser.omnibox.action.OmniboxPedalType;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxPedalDelegate;
import org.chromium.chrome.browser.omnibox.suggestions.pedal.PedalSuggestionViewProperties.PedalIcon;
import org.chromium.chrome.browser.password_manager.ManagePasswordsReferrer;
import org.chromium.chrome.browser.password_manager.PasswordManagerLauncher;
import org.chromium.chrome.browser.safety_check.SafetyCheckSettingsFragment;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.components.browser_ui.accessibility.AccessibilitySettings;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.browser_ui.site_settings.SiteSettings;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.PageTransition;

/**
 * Handle the clicks on the {@link OmniboxPedal}.
 */
public class OmniboxPedalDelegateImpl implements OmniboxPedalDelegate {
    private final @NonNull Activity mActivity;

    public OmniboxPedalDelegateImpl(@NonNull Activity activity) {
        mActivity = activity;
    }

    @Override
    public void executeAction(int omniboxActionType) {
        assert (omniboxActionType >= OmniboxPedalType.NONE
                && omniboxActionType < OmniboxPedalType.TOTAL_COUNT)
                || (omniboxActionType >= OmniboxActionType.FIRST
                        && omniboxActionType < OmniboxActionType.LAST);
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
                if (isChromeActivity()) {
                    ((ChromeActivity) mActivity)
                            .onMenuOrKeyboardAction(
                                    R.id.new_incognito_tab_menu_id, /*fromMenu*/ false);
                } else {
                    Intent intent = IntentHandler.createTrustedOpenNewTabIntent(
                            mActivity.getApplicationContext(), /*incognito=*/true);
                    startActivity(intent);
                }
                break;
            case OmniboxPedalType.RUN_CHROME_SAFETY_CHECK:
                settingsLauncher.launchSettingsActivity(mActivity,
                        SafetyCheckSettingsFragment.class,
                        SafetyCheckSettingsFragment.createBundle(
                                /*runSafetyCheckImmediately=*/true));
                break;
            case OmniboxPedalType.MANAGE_SITE_SETTINGS:
                settingsLauncher.launchSettingsActivity(mActivity, SiteSettings.class);
                break;
            case OmniboxPedalType.MANAGE_CHROME_SETTINGS:
                settingsLauncher.launchSettingsActivity(mActivity);
                break;
            case OmniboxPedalType.VIEW_CHROME_HISTORY:
                if (isChromeActivity()) {
                    ((ChromeActivity) mActivity)
                            .onMenuOrKeyboardAction(R.id.open_history_menu_id, /*fromMenu*/ false);
                } else {
                    Intent intent = new Intent();
                    intent.setClass(mActivity.getApplicationContext(), HistoryActivity.class);
                    intent.putExtra(IntentHandler.EXTRA_INCOGNITO_MODE, false);
                    startActivity(intent);
                }
                break;
            case OmniboxPedalType.MANAGE_CHROME_ACCESSIBILITY:
                settingsLauncher.launchSettingsActivity(mActivity, AccessibilitySettings.class);
                break;
            case OmniboxPedalType.PLAY_CHROME_DINO_GAME:
                if (isChromeActivity()) {
                    ((ChromeActivity) mActivity)
                            .getActivityTab()
                            .loadUrl(new LoadUrlParams(
                                    UrlConstants.CHROME_DINO_URL, PageTransition.GENERATED));
                } else {
                    Context context = mActivity.getApplicationContext();
                    Intent dinoIntent = createDinoIntent(context);
                    startActivity(dinoIntent);
                }
                break;
        }
        return;
    }

    /**
     * Creates an intent to launch a new tab with chrome://dino/ URL.
     *
     * @param context The context from which the intent is being created.
     * @return An intent to launch a tab with a new tab with chrome://dino/ URL.
     */
    private @NonNull Intent createDinoIntent(final @NonNull Context context) {
        // We concatenate the forward slash to the URL since if a Dino tab already exists, we would
        // like to reuse it. In order to determine if there is an existing Dino tab,
        // ChromeTabbedActivity will check by comparing URLs of existing tabs to the URL of our
        // intent. If there is an existing Dino tab, it would have a forward slash appended to the
        // end of its URL, so our URL must have a forward slash to match.
        String chromeDinoUrl = UrlConstants.CHROME_DINO_URL + "/";

        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(chromeDinoUrl));
        intent.setComponent(new ComponentName(context, ChromeLauncherActivity.class));
        intent.putExtra(WebappConstants.REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB, true);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_NEW_DOCUMENT);

        return intent;
    }

    /**
     * Start the actrivty by mActivity.
     *
     * @param intent The intent to launch the activity.
     */
    private void startActivity(@NonNull Intent intent) {
        IntentUtils.addTrustedIntentExtras(intent);
        mActivity.startActivity(intent);
    }

    @Override
    public @NonNull PedalIcon getPedalIcon(int omniboxActionType) {
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
                return new PedalIcon(R.drawable.fre_product_logo, /*tintWithTextColor=*/false);
            case OmniboxPedalType.PLAY_CHROME_DINO_GAME:
                return new PedalIcon(R.drawable.ic_dino, /*tintWithTextColor=*/true);
            default:
                // Please confirm the icon for the new pedals in
                // chrome/browser/ui/omnibox/omnibox_pedal_implementations.cc, if the new pedals use
                // a spicial icon.
                assert false : "New pedals need to confirm the icon and add the list above.";
                break;
        }
        return new PedalIcon(R.drawable.fre_product_logo, /*tintWithTextColor=*/false);
    }

    /**
     * Returns true, if the current activity type is regular Chrome activity.
     * Other activity types (SearchActivity etc) return false.
     */
    private boolean isChromeActivity() {
        return mActivity instanceof ChromeActivity;
    }
}
