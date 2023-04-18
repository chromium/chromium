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
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.browserservices.intents.WebappConstants;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.history_clusters.HistoryClustersCoordinator;
import org.chromium.chrome.browser.omnibox.suggestions.ActionChipsDelegate;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionsMetrics;
import org.chromium.chrome.browser.password_manager.ManagePasswordsReferrer;
import org.chromium.chrome.browser.password_manager.PasswordManagerLauncher;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.browser_ui.settings.SettingsLauncher.SettingsFragment;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.omnibox.action.HistoryClustersAction;
import org.chromium.components.omnibox.action.OmniboxAction;
import org.chromium.components.omnibox.action.OmniboxActionInSuggest;
import org.chromium.components.omnibox.action.OmniboxActionType;
import org.chromium.components.omnibox.action.OmniboxPedal;
import org.chromium.components.omnibox.action.OmniboxPedalType;
import org.chromium.content_public.browser.LoadUrlParams;
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
    private final @NonNull Supplier<Tab> mTabSupplier;

    public ActionChipsDelegateImpl(@NonNull Activity activity,
            @NonNull Supplier<HistoryClustersCoordinator> historyClustersCoordinatorSupplier,
            @NonNull ObservableSupplier<ModalDialogManager> modalDialogManagerSupplier,
            @NonNull Supplier<Tab> tabSupplier) {
        mActivity = activity;
        mSettingsLauncher = new SettingsLauncherImpl();
        mHistoryClustersCoordinatorSupplier = historyClustersCoordinatorSupplier;
        mModalDialogManagerSupplier = modalDialogManagerSupplier;
        mTabSupplier = tabSupplier;
    }

    private void executePedalAction(OmniboxPedal pedal) {
        @OmniboxPedalType
        int pedalId = pedal.pedalId;
        switch (pedalId) {
            case OmniboxPedalType.MANAGE_CHROME_SETTINGS:
                mSettingsLauncher.launchSettingsActivity(mActivity, SettingsFragment.MAIN);
                break;
            case OmniboxPedalType.CLEAR_BROWSING_DATA:
                mSettingsLauncher.launchSettingsActivity(
                        mActivity, SettingsFragment.CLEAR_BROWSING_DATA);
                break;
            case OmniboxPedalType.UPDATE_CREDIT_CARD:
                mSettingsLauncher.launchSettingsActivity(
                        mActivity, SettingsFragment.PAYMENT_METHODS);
                break;
            case OmniboxPedalType.RUN_CHROME_SAFETY_CHECK:
                mSettingsLauncher.launchSettingsActivity(mActivity, SettingsFragment.SAFETY_CHECK);
                break;
            case OmniboxPedalType.MANAGE_SITE_SETTINGS:
                mSettingsLauncher.launchSettingsActivity(mActivity, SettingsFragment.SITE);
                break;
            case OmniboxPedalType.MANAGE_CHROME_ACCESSIBILITY:
                mSettingsLauncher.launchSettingsActivity(mActivity, SettingsFragment.ACCESSIBILITY);
                break;
            case OmniboxPedalType.VIEW_CHROME_HISTORY:
                loadPageInCurrentTab(UrlConstants.HISTORY_URL);
                break;
            case OmniboxPedalType.PLAY_CHROME_DINO_GAME:
                loadPageInCurrentTab(UrlConstants.CHROME_DINO_URL);
                break;

            case OmniboxPedalType.MANAGE_PASSWORDS:
                PasswordManagerLauncher.showPasswordSettings(mActivity,
                        ManagePasswordsReferrer.CHROME_SETTINGS, mModalDialogManagerSupplier,
                        /*managePasskeys=*/false);
                break;
            case OmniboxPedalType.LAUNCH_INCOGNITO:
                startActivity(IntentHandler.createTrustedOpenNewTabIntent(
                        mActivity.getApplicationContext(), /*incognito=*/true));
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

    /**
     * Load the supplied URL in the current tab.
     * If not possible, open a new tab and load the url there. Try to re-use existing tabs where
     * possible.
     *
     * @param url the page URL to load
     */
    private void loadPageInCurrentTab(String url) {
        var tab = mTabSupplier.get();
        if (tab != null) {
            tab.loadUrl(new LoadUrlParams(url));
        } else {
            Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(url));
            intent.setComponent(new ComponentName(
                    mActivity.getApplicationContext(), ChromeLauncherActivity.class));
            intent.putExtra(WebappConstants.REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB, true);
            startActivity(intent);
        }
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

            switch (actionInSuggest.actionInfo.getActionType()) {
                case WEBSITE:
                    // Rather than invoking an intent that opens a new tab, load the page in the
                    // current tab.
                    loadPageInCurrentTab(intent.getDataString());
                    break;

                case CALL:
                    // Don't call directly. Use `DIAL` instead to let the user decide.
                    // Note also that ACTION_CALL requires a dedicated permission.
                    intent.setAction(Intent.ACTION_DIAL);
                    // fall through.
                case DIRECTIONS:
                default:
                    startActivity(intent);
                    break;
            }

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
}
