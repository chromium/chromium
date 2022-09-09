// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.omnibox;

import android.app.Activity;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.text.TextUtils;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.IntentUtils;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.autofill.settings.AutofillPaymentMethodsFragment;
import org.chromium.chrome.browser.browserservices.intents.WebappConstants;
import org.chromium.chrome.browser.browsing_data.ClearBrowsingDataTabsFragment;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.history.HistoryActivity;
import org.chromium.chrome.browser.history_clusters.HistoryClustersCoordinator;
import org.chromium.chrome.browser.omnibox.action.OmniboxActionType;
import org.chromium.chrome.browser.omnibox.action.OmniboxPedalType;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxPedalDelegate;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionsMetrics;
import org.chromium.chrome.browser.omnibox.suggestions.pedal.PedalSuggestionViewProperties.PedalIcon;
import org.chromium.chrome.browser.password_manager.ManagePasswordsReferrer;
import org.chromium.chrome.browser.password_manager.PasswordManagerLauncher;
import org.chromium.chrome.browser.safety_check.SafetyCheckSettingsFragment;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.components.browser_ui.accessibility.AccessibilitySettings;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.browser_ui.site_settings.SiteSettings;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.omnibox.action.HistoryClustersAction;
import org.chromium.components.omnibox.action.OmniboxPedal;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.modaldialog.ModalDialogManager;

/**
 * Handle the clicks on the {@link OmniboxPedal}.
 */
public class OmniboxPedalDelegateImpl implements OmniboxPedalDelegate {
    private final @NonNull Activity mActivity;
    private @Nullable HistoryClustersCoordinator mHistoryClustersCoordinator;
    private final ObservableSupplier<ModalDialogManager> mModalDialogManagerSupplier;

    public OmniboxPedalDelegateImpl(@NonNull Activity activity,
            OneshotSupplier<HistoryClustersCoordinator> historyClustersCoordinatorSupplier,
            ObservableSupplier<ModalDialogManager> modalDialogManagerSupplier) {
        mActivity = activity;
        historyClustersCoordinatorSupplier.onAvailable(
                coordinator -> mHistoryClustersCoordinator = coordinator);
        mModalDialogManagerSupplier = modalDialogManagerSupplier;
    }

    @Override
    public void execute(OmniboxPedal omniboxPedal) {
        if (omniboxPedal.hasActionId()) {
            executeNonPedalAction(omniboxPedal);
            return;
        }

        @OmniboxPedalType
        int omniboxPedalType = omniboxPedal.getPedalID();
        SettingsLauncher settingsLauncher = new SettingsLauncherImpl();
        switch (omniboxPedalType) {
            case OmniboxPedalType.CLEAR_BROWSING_DATA:
                settingsLauncher.launchSettingsActivity(
                        mActivity, ClearBrowsingDataTabsFragment.class);
                break;
            case OmniboxPedalType.MANAGE_PASSWORDS:
                PasswordManagerLauncher.showPasswordSettings(mActivity,
                        ManagePasswordsReferrer.CHROME_SETTINGS, mModalDialogManagerSupplier);
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
        SuggestionsMetrics.recordPedalUsed(omniboxPedalType);
        return;
    }

    private void executeNonPedalAction(OmniboxPedal omniboxPedal) {
        switch (omniboxPedal.getActionID()) {
            case OmniboxActionType.HISTORY_CLUSTERS:
                if (mHistoryClustersCoordinator != null) {
                    assert omniboxPedal instanceof HistoryClustersAction;
                    String query = ((HistoryClustersAction) omniboxPedal).getQuery();
                    assert !TextUtils.isEmpty(query);
                    mHistoryClustersCoordinator.openHistoryClustersUi(query);
                }
                break;
        }
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
     * Start the activity via mActivity.
     *
     * @param intent The intent to launch the activity.
     */
    private void startActivity(@NonNull Intent intent) {
        IntentUtils.addTrustedIntentExtras(intent);
        mActivity.startActivity(intent);
    }

    @Override
    public @NonNull PedalIcon getIcon(OmniboxPedal omniboxPedal) {
        if (!omniboxPedal.hasPedalId()) {
            return getActionIcon(omniboxPedal);
        }

        @OmniboxPedalType
        int omniboxPedalType = omniboxPedal.getPedalID();

        switch (omniboxPedalType) {
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
                // chrome/browser/ui/omnibox/omnibox_pedal_implementations.cc, if the new pedal uses
                // a special icon.
                assert false : "New pedals need to confirm the icon and add the list above.";
                break;
        }
        return new PedalIcon(R.drawable.fre_product_logo, /*tintWithTextColor=*/false);
    }

    /** Returns the icon for an action that's not a pedal. */
    private PedalIcon getActionIcon(OmniboxPedal omniboxPedal) {
        int omniboxActionType = omniboxPedal.getActionID();

        switch (omniboxActionType) {
            case OmniboxActionType.HISTORY_CLUSTERS:
                return new PedalIcon(R.drawable.ic_journeys, /*tintWithTextColor=*/true);
            default:
                return new PedalIcon(R.drawable.fre_product_logo, /*tintWithTextColor=*/false);
        }
    }

    /**
     * Returns true, if the current activity type is regular Chrome activity.
     * Other activity types (SearchActivity etc) return false.
     */
    private boolean isChromeActivity() {
        return mActivity instanceof ChromeActivity;
    }
}
