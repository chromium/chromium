// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.omnibox;

import android.app.Activity;
import android.content.ActivityNotFoundException;
import android.content.ComponentName;
import android.content.Intent;
import android.net.Uri;

import androidx.annotation.NonNull;

import org.chromium.base.IntentUtils;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.autofill.settings.AutofillPaymentMethodsFragment;
import org.chromium.chrome.browser.browserservices.intents.WebappConstants;
import org.chromium.chrome.browser.browsing_data.ClearBrowsingDataTabsFragment;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.history.HistoryActivity;
import org.chromium.chrome.browser.history_clusters.HistoryClustersCoordinator;
import org.chromium.chrome.browser.omnibox.suggestions.ActionChipsDelegate;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionsMetrics;
import org.chromium.chrome.browser.password_manager.ManagePasswordsReferrer;
import org.chromium.chrome.browser.password_manager.PasswordManagerLauncher;
import org.chromium.chrome.browser.safety_check.SafetyCheckSettingsFragment;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.components.browser_ui.accessibility.AccessibilitySettings;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.browser_ui.site_settings.SiteSettings;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.omnibox.action.HistoryClustersAction;
import org.chromium.components.omnibox.action.OmniboxAction;
import org.chromium.components.omnibox.action.OmniboxActionInSuggest;
import org.chromium.components.omnibox.action.OmniboxActionType;
import org.chromium.components.omnibox.action.OmniboxPedal;
import org.chromium.components.omnibox.action.OmniboxPedalType;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.net.URISyntaxException;

/**
 * Handle the events related to {@link OmniboxAction}.
 */
public class ActionChipsDelegateImpl implements ActionChipsDelegate {
    private final @NonNull Activity mActivity;
    private final @NonNull SettingsLauncher mSettingsLauncher;
    private final @NonNull Supplier<HistoryClustersCoordinator> mHistoryClustersCoordinatorSupplier;
    private final @NonNull ObservableSupplier<ModalDialogManager> mModalDialogManagerSupplier;

    public ActionChipsDelegateImpl(@NonNull Activity activity,
            @NonNull Supplier<HistoryClustersCoordinator> historyClustersCoordinatorSupplier,
            @NonNull ObservableSupplier<ModalDialogManager> modalDialogManagerSupplier) {
        mActivity = activity;
        mSettingsLauncher = new SettingsLauncherImpl();
        mHistoryClustersCoordinatorSupplier = historyClustersCoordinatorSupplier;
        mModalDialogManagerSupplier = modalDialogManagerSupplier;
    }

    private void executePedalAction(OmniboxPedal pedal) {
        @OmniboxPedalType
        int pedalId = pedal.pedalId;
        switch (pedalId) {
            case OmniboxPedalType.CLEAR_BROWSING_DATA:
                mSettingsLauncher.launchSettingsActivity(
                        mActivity, ClearBrowsingDataTabsFragment.class);
                break;
            case OmniboxPedalType.MANAGE_PASSWORDS:
                PasswordManagerLauncher.showPasswordSettings(mActivity,
                        ManagePasswordsReferrer.CHROME_SETTINGS, mModalDialogManagerSupplier,
                        /*managePasskeys=*/false);
                break;
            case OmniboxPedalType.UPDATE_CREDIT_CARD:
                mSettingsLauncher.launchSettingsActivity(
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
                mSettingsLauncher.launchSettingsActivity(mActivity,
                        SafetyCheckSettingsFragment.class,
                        SafetyCheckSettingsFragment.createBundle(
                                /*runSafetyCheckImmediately=*/true));
                break;
            case OmniboxPedalType.MANAGE_SITE_SETTINGS:
                mSettingsLauncher.launchSettingsActivity(mActivity, SiteSettings.class);
                break;
            case OmniboxPedalType.MANAGE_CHROME_SETTINGS:
                mSettingsLauncher.launchSettingsActivity(mActivity);
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
                mSettingsLauncher.launchSettingsActivity(mActivity, AccessibilitySettings.class);
                break;
            case OmniboxPedalType.PLAY_CHROME_DINO_GAME:
                if (isChromeActivity()) {
                    ((ChromeActivity) mActivity)
                            .getActivityTab()
                            .loadUrl(new LoadUrlParams(
                                    UrlConstants.CHROME_DINO_URL, PageTransition.GENERATED));
                } else {
                    startActivity(createDinoIntent());
                }
                break;
        }
        SuggestionsMetrics.recordPedalUsed(pedalId);
    }

    @Override
    public void execute(OmniboxAction action) {
        switch (action.actionId) {
            case OmniboxActionType.PEDAL:
                executePedalAction(OmniboxPedal.from(action));
                break;

            case OmniboxActionType.HISTORY_CLUSTERS:
                var historyClustersCoordinator = mHistoryClustersCoordinatorSupplier.get();
                if (historyClustersCoordinator != null) {
                    historyClustersCoordinator.openHistoryClustersUi(
                            HistoryClustersAction.from(action).query);
                }
                break;

            case OmniboxActionType.ACTION_IN_SUGGEST:
                startActionInSuggestIntent(OmniboxActionInSuggest.from(action));
                break;
        }
    }

    /** Returns an intent to launch a new tab with chrome://dino/ URL. */
    private @NonNull Intent createDinoIntent() {
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(UrlConstants.CHROME_DINO_URL));
        intent.setComponent(
                new ComponentName(mActivity.getApplicationContext(), ChromeLauncherActivity.class));
        intent.putExtra(WebappConstants.REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB, true);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_NEW_DOCUMENT);

        return intent;
    }

    /**
     * Execute an Intent associated with OmniboxActionInSuggest.
     *
     * @param actionInSuggest the action to execute the intent for
     */
    private void startActionInSuggestIntent(OmniboxActionInSuggest actionInSuggest) {
        try {
            var intent = Intent.parseUri(
                    actionInSuggest.actionInfo.getActionUri(), Intent.URI_INTENT_SCHEME);

            // Don't call directly. Use `DIAL` instead to let the user decide.
            // Note also that ACTION_CALL requires a dedicated permission.
            if (intent.getAction().equals(Intent.ACTION_CALL)) {
                intent.setAction(Intent.ACTION_DIAL);
            }

            intent.putExtra(WebappConstants.REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB, true);
            startActivity(intent);

            SuggestionsMetrics.recordActionInSuggestIntentResult(
                    SuggestionsMetrics.ActionInSuggestIntentResult.SUCCESS);
        } catch (URISyntaxException e) {
            SuggestionsMetrics.recordActionInSuggestIntentResult(
                    SuggestionsMetrics.ActionInSuggestIntentResult.BAD_URI_SYNTAX);
        } catch (ActivityNotFoundException e) {
            SuggestionsMetrics.recordActionInSuggestIntentResult(
                    SuggestionsMetrics.ActionInSuggestIntentResult.ACTIVITY_NOT_FOUND);
        }
    }
    /**
     * Start the activity referenced by the supplied {@link android.content.Intent}.
     * Decorates the intent with trusted intent extras when the intent references the browser.
     */
    private void startActivity(@NonNull Intent intent) {
        if (IntentUtils.intentTargetsSelf(mActivity, intent)) {
            IntentUtils.addTrustedIntentExtras(intent);
        }
        mActivity.startActivity(intent);
    }

    /**
     * Returns true, if the current activity type is regular Chrome activity.
     * Other activity types (SearchActivity etc) return false.
     */
    private boolean isChromeActivity() {
        return mActivity instanceof ChromeActivity;
    }
}
