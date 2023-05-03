// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.omnibox;

import android.content.ActivityNotFoundException;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;

import androidx.annotation.NonNull;

import org.chromium.base.IntentUtils;
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

import java.net.URISyntaxException;

/**
 * Handle the events related to {@link OmniboxAction}.
 */
public class ActionChipsDelegateImpl implements ActionChipsDelegate {
    private final @NonNull Context mContext;
    private final @NonNull SettingsLauncher mSettingsLauncher;
    private final @NonNull Supplier<HistoryClustersCoordinator> mHistoryClustersCoordinatorSupplier;
    private final @NonNull Supplier<Tab> mTabSupplier;

    public ActionChipsDelegateImpl(@NonNull Context context,
            @NonNull Supplier<HistoryClustersCoordinator> historyClustersCoordinatorSupplier,
            @NonNull Supplier<Tab> tabSupplier) {
        mContext = context;
        mSettingsLauncher = new SettingsLauncherImpl();
        mHistoryClustersCoordinatorSupplier = historyClustersCoordinatorSupplier;
        mTabSupplier = tabSupplier;
    }

    private void executePedalAction(OmniboxPedal pedal) {
        @OmniboxPedalType
        int pedalId = pedal.pedalId;
        switch (pedalId) {
            case OmniboxPedalType.MANAGE_CHROME_SETTINGS:
                mSettingsLauncher.launchSettingsActivity(mContext, SettingsFragment.MAIN);
                break;
            case OmniboxPedalType.CLEAR_BROWSING_DATA:
                mSettingsLauncher.launchSettingsActivity(
                        mContext, SettingsFragment.CLEAR_BROWSING_DATA);
                break;
            case OmniboxPedalType.UPDATE_CREDIT_CARD:
                mSettingsLauncher.launchSettingsActivity(
                        mContext, SettingsFragment.PAYMENT_METHODS);
                break;
            case OmniboxPedalType.RUN_CHROME_SAFETY_CHECK:
                mSettingsLauncher.launchSettingsActivity(mContext, SettingsFragment.SAFETY_CHECK);
                break;
            case OmniboxPedalType.MANAGE_SITE_SETTINGS:
                mSettingsLauncher.launchSettingsActivity(mContext, SettingsFragment.SITE);
                break;
            case OmniboxPedalType.MANAGE_CHROME_ACCESSIBILITY:
                mSettingsLauncher.launchSettingsActivity(mContext, SettingsFragment.ACCESSIBILITY);
                break;
            case OmniboxPedalType.VIEW_CHROME_HISTORY:
                loadPageInCurrentTab(UrlConstants.HISTORY_URL);
                break;
            case OmniboxPedalType.PLAY_CHROME_DINO_GAME:
                loadPageInCurrentTab(UrlConstants.CHROME_DINO_URL);
                break;

            case OmniboxPedalType.MANAGE_PASSWORDS:
                PasswordManagerLauncher.showPasswordSettings(mContext,
                        ManagePasswordsReferrer.CHROME_SETTINGS,
                        // clang-format off: trying to be clever even with curly braces.
                        () -> mTabSupplier.get().getWindowAndroid().getModalDialogManager(),
                        // clang-format on
                        /*managePasskeys=*/false);
                break;
            case OmniboxPedalType.LAUNCH_INCOGNITO:
                startActivity(IntentHandler.createTrustedOpenNewTabIntent(
                        mContext.getApplicationContext(), /*incognito=*/true));
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
        if (tab.isUserInteractable()) {
            tab.loadUrl(new LoadUrlParams(url));
        } else {
            Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(url));
            intent.setComponent(new ComponentName(
                    mContext.getApplicationContext(), ChromeLauncherActivity.class));
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
        var actionType = actionInSuggest.actionInfo.getActionType();
        boolean actionStarted = false;
        Intent intent = null;

        try {
            intent = Intent.parseUri(
                    actionInSuggest.actionInfo.getActionUri(), Intent.URI_INTENT_SCHEME);

            switch (actionType) {
                case WEBSITE:
                    // Rather than invoking an intent that opens a new tab, load the page in the
                    // current tab.
                    loadPageInCurrentTab(intent.getDataString());
                    actionStarted = true;
                    break;

                case REVIEWS:
                    assert false : "Pending implementation";
                    break;

                case CALL:
                    // Don't call directly. Use `DIAL` instead to let the user decide.
                    // Note also that ACTION_CALL requires a dedicated permission.
                    intent.setAction(Intent.ACTION_DIAL);
                    // Start dialer even if the user is in incognito mode. The intent only pre-dials
                    // the phone number without ever making the call. This gives the user the chance
                    // to abandon before making a call.
                    startActivity(intent);
                    actionStarted = true;
                    break;

                case DIRECTIONS:
                    // Open directions in maps only if maps are installed and the incognito mode is
                    // not engaged. In all other cases, redirect the action to Browser.
                    Tab currentTab = mTabSupplier.get();
                    if (currentTab == null || !currentTab.isIncognito()) {
                        startActivity(intent);
                        actionStarted = true;
                    }
                    break;

                    // No `default` to capture new variants.
            }

            // Record intent started only if it was sent.
            if (actionStarted) {
                SuggestionsMetrics.recordActionInSuggestIntentResult(
                        SuggestionsMetrics.ActionInSuggestIntentResult.SUCCESS);
            }
        } catch (URISyntaxException e) {
            // Never happens. http://b/279756377.
        } catch (ActivityNotFoundException e) {
            SuggestionsMetrics.recordActionInSuggestIntentResult(
                    SuggestionsMetrics.ActionInSuggestIntentResult.ACTIVITY_NOT_FOUND);
        } finally {
            // At this point we know that we were unable to launch the target activity.
            // We may still be able to handle the corresponding action inside the browser.
            if (!actionStarted) {
                switch (actionType) {
                    case DIRECTIONS:
                        loadPageInCurrentTab(intent.getDataString());
                        break;

                    case CALL:
                    case REVIEWS:
                    case WEBSITE:
                        // Give up. Don't add the `default` clause though, capture missed variants.
                        break;
                }
            }
        }
    }
    /**
     * Start the activity referenced by the supplied {@link android.content.Intent}.
     * Decorates the intent with trusted intent extras when the intent references the browser.
     */
    private void startActivity(@NonNull Intent intent) {
        if (IntentUtils.intentTargetsSelf(mContext, intent)) {
            IntentUtils.addTrustedIntentExtras(intent);
        }
        mContext.startActivity(intent);
    }
}
