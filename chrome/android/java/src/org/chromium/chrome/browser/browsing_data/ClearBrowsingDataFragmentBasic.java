// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browsing_data;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.provider.Browser;
import android.text.SpannableString;
import android.view.View;

import androidx.annotation.IntDef;
import androidx.browser.customtabs.CustomTabsIntent;
import androidx.preference.Preference;

import org.chromium.base.Callback;
import org.chromium.base.IntentUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.document.ChromeAsyncTabLauncher;
import org.chromium.components.browser_ui.settings.ClickableSpansTextMessagePreference;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.sync.DataType;
import org.chromium.components.sync.SyncService;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Arrays;
import java.util.List;

/**
 * A simpler version of {@link ClearBrowsingDataFragment} with fewer dialog options and more
 * explanatory text.
 */
public class ClearBrowsingDataFragmentBasic extends ClearBrowsingDataFragment {
    /**
     * Functional interface to start a Chrome Custom Tab for the given intent, e.g. by using {@link
     * org.chromium.chrome.browser.LaunchIntentDispatcher#createCustomTabActivityIntent}.
     * TODO(crbug.com/40773797): Update when LaunchIntentDispatcher is (partially-)modularized.
     */
    public interface CustomTabIntentHelper {
        /**
         * @see org.chromium.chrome.browser.LaunchIntentDispatcher#createCustomTabActivityIntent
         */
        Intent createCustomTabActivityIntent(Context context, Intent intent);
    }

    /**
     * UMA histogram values for MyActivity navigations.
     * Note: this should stay in sync with ClearBrowsingDataMyActivityNavigation in enums.xml.
     */
    @IntDef({MyActivityNavigation.TOP_LEVEL, MyActivityNavigation.SEARCH_HISTORY})
    @Retention(RetentionPolicy.SOURCE)
    private @interface MyActivityNavigation {
        int TOP_LEVEL = 0;
        int SEARCH_HISTORY = 1;
        int NUM_ENTRIES = 2;
    }

    private CustomTabIntentHelper mCustomTabHelper;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        ClearBrowsingDataCheckBoxPreference historyCheckbox =
                (ClearBrowsingDataCheckBoxPreference)
                        findPreference(getPreferenceKey(DialogOption.CLEAR_HISTORY));
        ClearBrowsingDataCheckBoxPreference cookiesCheckbox =
                (ClearBrowsingDataCheckBoxPreference)
                        findPreference(getPreferenceKey(DialogOption.CLEAR_COOKIES_AND_SITE_DATA));

        historyCheckbox.setLinkClickDelegate(
                () -> {
                    new ChromeAsyncTabLauncher(/* incognito= */ false)
                            .launchUrl(
                                    UrlConstants.MY_ACTIVITY_URL_IN_CBD,
                                    TabLaunchType.FROM_CHROME_UI);
                });

        IdentityManager identityManager =
                IdentityServicesProvider.get().getIdentityManager(getProfile());
        if (identityManager.hasPrimaryAccount(ConsentLevel.SIGNIN)) {
            // Update the Clear Browsing History text based on the sign-in/sync state and whether
            // the link to MyActivity is displayed inline or at the bottom of the page.
            // Note: when  sync is disabled, the default string is used.
            if (isHistorySyncEnabled()) {
                // The text is different only for users with history sync.
                historyCheckbox.setSummary(R.string.clear_browsing_history_summary_synced_no_link);
            }
            cookiesCheckbox.setSummary(
                    R.string.clear_cookies_and_site_data_summary_basic_signed_in);
        }
    }

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        super.onCreatePreferences(savedInstanceState, rootKey);
        Profile profile = getProfile();
        IdentityManager identityManager =
                IdentityServicesProvider.get().getIdentityManager(profile);
        ClickableSpansTextMessagePreference googleDataTextPref =
                (ClickableSpansTextMessagePreference)
                        findPreference(ClearBrowsingDataFragment.PREF_GOOGLE_DATA_TEXT);
        Preference nonGoogleSearchHistoryTextPref =
                findPreference(ClearBrowsingDataFragment.PREF_SEARCH_HISTORY_NON_GOOGLE_TEXT);
        TemplateUrlService templateUrlService = TemplateUrlServiceFactory.getForProfile(profile);
        TemplateUrl defaultSearchEngine = templateUrlService.getDefaultSearchEngineTemplateUrl();
        boolean isDefaultSearchEngineGoogle = templateUrlService.isDefaultSearchEngineGoogle();

        // Google-related links to delete search history and other browsing activity.
        if (defaultSearchEngine == null
                || !identityManager.hasPrimaryAccount(ConsentLevel.SIGNIN)) {
            // One of two cases:
            // 1. The default search engine is disabled.
            // 2. The user is not signed into Chrome.
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
        if (defaultSearchEngine == null || isDefaultSearchEngineGoogle) {
            // One of two cases:
            // 1. The default search engine is disabled.
            // 2. The default search engine is Google.
            // In all those cases, delete the link to clear non-Google search history.
            deleteNonGoogleSearchHistoryTextIfExists();
        } else if (defaultSearchEngine.getIsPrepopulated()) {
            // Prepopulated non-Google DSE. Use its name in the text.
            nonGoogleSearchHistoryTextPref.setSummary(
                    getContext()
                            .getString(
                                    R.string.clear_search_history_non_google_dse,
                                    defaultSearchEngine.getShortName()));
        } else {
            // Unknown non-Google DSE. Use generic text.
            nonGoogleSearchHistoryTextPref.setSummary(
                    R.string.clear_search_history_non_google_dse_unknown);
        }
    }

    public void setCustomTabIntentHelper(CustomTabIntentHelper tabHelper) {
        mCustomTabHelper = tabHelper;
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
        return SpanApplier.applySpans(
                getContext().getString(R.string.clear_search_history_link),
                new SpanInfo(
                        "<link1>",
                        "</link1>",
                        new NoUnderlineClickableSpan(
                                getContext(),
                                createOpenMyActivityCallback(/* openSearchHistory= */ true))),
                new SpanInfo(
                        "<link2>",
                        "</link2>",
                        new NoUnderlineClickableSpan(
                                getContext(),
                                createOpenMyActivityCallback(/* openSearchHistory= */ false))));
    }

    private SpannableString buildGoogleMyActivityText() {
        return SpanApplier.applySpans(
                getContext().getString(R.string.clear_search_history_link_other_forms),
                new SpanInfo(
                        "<link1>",
                        "</link1>",
                        new NoUnderlineClickableSpan(
                                getContext(),
                                createOpenMyActivityCallback(/* openSearchHistory= */ false))));
    }

    /** If openSearchHistory is true, opens the search history page; otherwise: top level. */
    private Callback<View> createOpenMyActivityCallback(boolean openSearchHistory) {
        return (widget) -> {
            assert mCustomTabHelper != null
                    : "CCT helper must be set on ClearBrowsingFragmentBasic before opening a link.";
            CustomTabsIntent customTabIntent =
                    new CustomTabsIntent.Builder().setShowTitle(true).build();

            String url;
            if (openSearchHistory) {
                url = UrlConstants.GOOGLE_SEARCH_HISTORY_URL_IN_CBD;
                RecordHistogram.recordEnumeratedHistogram(
                        "Settings.ClearBrowsingData.OpenMyActivity",
                        MyActivityNavigation.SEARCH_HISTORY,
                        MyActivityNavigation.NUM_ENTRIES);
            } else {
                url = UrlConstants.MY_ACTIVITY_URL_IN_CBD;
                RecordHistogram.recordEnumeratedHistogram(
                        "Settings.ClearBrowsingData.OpenMyActivity",
                        MyActivityNavigation.TOP_LEVEL,
                        MyActivityNavigation.NUM_ENTRIES);
            }
            customTabIntent.intent.setData(Uri.parse(url));
            Intent intent =
                    mCustomTabHelper.createCustomTabActivityIntent(
                            getContext(), customTabIntent.intent);
            intent.setPackage(getContext().getPackageName());
            intent.putExtra(Browser.EXTRA_APPLICATION_ID, getContext().getPackageName());
            IntentUtils.addTrustedIntentExtras(intent);
            IntentUtils.safeStartActivity(getContext(), intent);
        };
    }

    private boolean isHistorySyncEnabled() {
        SyncService syncService = SyncServiceFactory.getForProfile(getProfile());
        return syncService != null
                && syncService.getActiveDataTypes().contains(DataType.HISTORY_DELETE_DIRECTIVES);
    }

    @Override
    protected int getClearBrowsingDataTabType() {
        return ClearBrowsingDataTab.BASIC;
    }

    @Override
    protected List<Integer> getDialogOptions(Bundle fragmentArgs) {
        return Arrays.asList(
                DialogOption.CLEAR_HISTORY,
                DialogOption.CLEAR_COOKIES_AND_SITE_DATA,
                DialogOption.CLEAR_CACHE);
    }

    @Override
    protected void onClearBrowsingData() {
        super.onClearBrowsingData();
        RecordHistogram.recordEnumeratedHistogram(
                "History.ClearBrowsingData.UserDeletedFromTab",
                ClearBrowsingDataTab.BASIC,
                ClearBrowsingDataTab.MAX_VALUE + 1);
        RecordUserAction.record("ClearBrowsingData_BasicTab");
    }
}
