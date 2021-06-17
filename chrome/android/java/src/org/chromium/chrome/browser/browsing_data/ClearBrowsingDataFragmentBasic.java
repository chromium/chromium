// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browsing_data;

import android.os.Bundle;
import android.text.SpannableString;

import androidx.preference.Preference;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.sync.SyncService;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.document.TabDelegate;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.sync.ModelType;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;

import java.util.Arrays;
import java.util.List;

/**
 * A simpler version of {@link ClearBrowsingDataFragment} with fewer dialog options and more
 * explanatory text.
 */
public class ClearBrowsingDataFragmentBasic extends ClearBrowsingDataFragment {
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        ClearBrowsingDataCheckBoxPreference historyCheckbox =
                (ClearBrowsingDataCheckBoxPreference) findPreference(
                        getPreferenceKey(DialogOption.CLEAR_HISTORY));
        ClearBrowsingDataCheckBoxPreference cookiesCheckbox =
                (ClearBrowsingDataCheckBoxPreference) findPreference(
                        getPreferenceKey(DialogOption.CLEAR_COOKIES_AND_SITE_DATA));

        historyCheckbox.setLinkClickDelegate(() -> {
            new TabDelegate(false /* incognito */)
                    .launchUrl(UrlConstants.MY_ACTIVITY_URL_IN_CBD, TabLaunchType.FROM_CHROME_UI);
        });

        IdentityManager identityManager = IdentityServicesProvider.get().getIdentityManager(
                Profile.getLastUsedRegularProfile());
        if (identityManager.hasPrimaryAccount()) {
            // Update the Clear Browsing History text based on the sign-in/sync state and whether
            // the link to MyActivity is displayed inline or at the bottom of the page.
            // Note: when the flag is enabled but sync is disabled, the default string is used, so
            // there is no need to change it.
            if (ChromeFeatureList.isEnabled(ChromeFeatureList.SEARCH_HISTORY_LINK)
                    && isHistorySyncEnabled()) {
                // The text is different only for users with history sync.
                historyCheckbox.setSummary(R.string.clear_browsing_history_summary_synced_no_link);
            } else if (!ChromeFeatureList.isEnabled(ChromeFeatureList.SEARCH_HISTORY_LINK)) {
                historyCheckbox.setSummary(isHistorySyncEnabled()
                                ? R.string.clear_browsing_history_summary_synced
                                : R.string.clear_browsing_history_summary_signed_in);
            }
            cookiesCheckbox.setSummary(
                    R.string.clear_cookies_and_site_data_summary_basic_signed_in);
        }
    }

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        super.onCreatePreferences(savedInstanceState, rootKey);
        IdentityManager identityManager = IdentityServicesProvider.get().getIdentityManager(
                Profile.getLastUsedRegularProfile());
        Preference googleDataTextPref =
                findPreference(ClearBrowsingDataFragment.PREF_GOOGLE_DATA_TEXT);
        Preference nonGoogleSearchHistoryTextPref =
                findPreference(ClearBrowsingDataFragment.PREF_SEARCH_HISTORY_NON_GOOGLE_TEXT);
        TemplateUrlService templateUrlService = TemplateUrlServiceFactory.get();
        TemplateUrl defaultSearchEngine = templateUrlService.getDefaultSearchEngineTemplateUrl();
        boolean isDefaultSearchEngineGoogle = templateUrlService.isDefaultSearchEngineGoogle();

        // Google-related links to delete search history and other browsing activity.
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.SEARCH_HISTORY_LINK)
                || defaultSearchEngine == null || !identityManager.hasPrimaryAccount()) {
            // One of three cases:
            // 1. The feature is disabled.
            // 2. The default search engine is disabled.
            // 3. The user is not signed into Chrome.
            // In all those cases, delete the link to clear Google data using MyActivity.
            deleteGoogleDataTextIfExists();
        } else if (isDefaultSearchEngineGoogle) {
            // Signed-in and the DSE is Google. Build the text with two links.
            googleDataTextPref.setSummary(buildGoogleSearchHistoryText());
        } else {
            // Signed-in and non-Google DSE. Build the text with the MyActivity link only.
            googleDataTextPref.setSummary(buildGoogleMyActivityText());
        }

        // Text for search history if DSE is not Google.
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.SEARCH_HISTORY_LINK)
                || defaultSearchEngine == null || isDefaultSearchEngineGoogle) {
            // One of three cases:
            // 1. The feature is disabled.
            // 2. The default search engine is disabled.
            // 3. The default search engine is Google.
            // In all those cases, delete the link to clear non-Google search history.
            deleteNonGoogleSearchHistoryTextIfExists();
        } else if (defaultSearchEngine.getIsPrepopulated()) {
            // Prepopulated non-Google DSE. Use its name in the text.
            nonGoogleSearchHistoryTextPref.setSummary(
                    getContext().getString(R.string.clear_search_history_non_google_dse,
                            defaultSearchEngine.getShortName()));
        } else {
            // Unknown non-Google DSE. Use generic text.
            nonGoogleSearchHistoryTextPref.setSummary(
                    R.string.clear_search_history_non_google_dse_unknown);
        }
    }

    private void deleteGoogleDataTextIfExists() {
        Preference googleDataTextPref =
                findPreference(ClearBrowsingDataFragment.PREF_GOOGLE_DATA_TEXT);
        if (googleDataTextPref != null) {
            getPreferenceScreen().removePreference(googleDataTextPref);
        }
    }

    private void deleteNonGoogleSearchHistoryTextIfExists() {
        Preference searchHistoryTextPref =
                findPreference(ClearBrowsingDataFragment.PREF_SEARCH_HISTORY_NON_GOOGLE_TEXT);
        if (searchHistoryTextPref != null) {
            getPreferenceScreen().removePreference(searchHistoryTextPref);
        }
    }

    private SpannableString buildGoogleSearchHistoryText() {
        return SpanApplier.applySpans(getContext().getString(R.string.clear_search_history_link),
                new SpanInfo("<link1>", "</link1>",
                        new NoUnderlineClickableSpan(getContext().getResources(),
                                (widget) -> {
                                    new TabDelegate(false /* incognito */)
                                            .launchUrl(
                                                    UrlConstants.GOOGLE_SEARCH_HISTORY_URL_IN_CBD,
                                                    TabLaunchType.FROM_CHROME_UI);
                                })),
                new SpanInfo("<link2>", "</link2>",
                        new NoUnderlineClickableSpan(getContext().getResources(), (widget) -> {
                            new TabDelegate(false /* incognito */)
                                    .launchUrl(UrlConstants.MY_ACTIVITY_URL_IN_CBD,
                                            TabLaunchType.FROM_CHROME_UI);
                        })));
    }

    private SpannableString buildGoogleMyActivityText() {
        return SpanApplier.applySpans(
                getContext().getString(R.string.clear_search_history_link_other_forms),
                new SpanInfo("<link1>", "</link1>",
                        new NoUnderlineClickableSpan(getContext().getResources(), (widget) -> {
                            new TabDelegate(false /* incognito */)
                                    .launchUrl(UrlConstants.MY_ACTIVITY_URL_IN_CBD,
                                            TabLaunchType.FROM_CHROME_UI);
                        })));
    }

    private boolean isHistorySyncEnabled() {
        SyncService syncService = SyncService.get();
        return syncService != null && syncService.isSyncRequested()
                && syncService.getActiveDataTypes().contains(ModelType.HISTORY_DELETE_DIRECTIVES);
    }

    @Override
    protected int getClearBrowsingDataTabType() {
        return ClearBrowsingDataTab.BASIC;
    }

    @Override
    protected List<Integer> getDialogOptions() {
        return Arrays.asList(DialogOption.CLEAR_HISTORY, DialogOption.CLEAR_COOKIES_AND_SITE_DATA,
                DialogOption.CLEAR_CACHE);
    }

    @Override
    protected void onClearBrowsingData() {
        super.onClearBrowsingData();
        RecordHistogram.recordEnumeratedHistogram("History.ClearBrowsingData.UserDeletedFromTab",
                ClearBrowsingDataTab.BASIC, ClearBrowsingDataTab.NUM_TYPES);
        RecordUserAction.record("ClearBrowsingData_BasicTab");
    }
}
