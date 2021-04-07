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
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.sync.ProfileSyncService;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.document.TabDelegate;
import org.chromium.components.embedder_support.util.UrlConstants;
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
        Preference searchHistoryTextPref =
                findPreference(ClearBrowsingDataFragment.PREF_SEARCH_HISTORY_TEXT);
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.SEARCH_HISTORY_LINK)
                && identityManager.hasPrimaryAccount() && searchHistoryTextPref != null) {
            searchHistoryTextPref.setSummary(buildSearchHistoryText());
        } else if (searchHistoryTextPref != null) {
            // Remove the search history text when the flag is disabled or the user is signed out.
            getPreferenceScreen().removePreference(searchHistoryTextPref);
        }
    }

    private SpannableString buildSearchHistoryText() {
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

    private boolean isHistorySyncEnabled() {
        ProfileSyncService syncService = ProfileSyncService.get();
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
