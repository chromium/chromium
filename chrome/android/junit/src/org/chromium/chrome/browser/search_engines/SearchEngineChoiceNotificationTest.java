// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.isNull;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.content.Intent;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.Spy;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.version_info.VersionInfo;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;

/** Unit tests for {@link SearchEngineChoiceNotification}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures({})
public final class SearchEngineChoiceNotificationTest {
    private static final String TEST_INITIAL_ENGINE = "google.com";
    private static final String TEST_ALTERNATIVE_ENGINE = "duckduckgo.com";

    @Spy private Context mContext = RuntimeEnvironment.application.getApplicationContext();
    @Mock private SnackbarManager mSnackbarManager;
    @Mock private TemplateUrlService mTemplateUrlService;
    @Mock private TemplateUrl mInitialSearchEngine;
    @Mock private TemplateUrl mAlternativeSearchEngine;
    @Mock private Profile mProfile;
    @Captor private ArgumentCaptor<Snackbar> mSnackbarArgument;
    @Mock private SettingsNavigation mSettingsNavigation;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        ContextUtils.initApplicationContextForTests(mContext);

        // Sets up appropriate responses from Template URL service.
        ProfileManager.setLastUsedProfileForTesting(mProfile);
        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);
        SettingsNavigationFactory.setInstanceForTesting(mSettingsNavigation);
        doReturn(TEST_ALTERNATIVE_ENGINE).when(mAlternativeSearchEngine).getKeyword();
        doReturn(SearchEngineType.SEARCH_ENGINE_DUCKDUCKGO)
                .when(mTemplateUrlService)
                .getSearchEngineTypeFromTemplateUrl(TEST_ALTERNATIVE_ENGINE);
        doReturn(TEST_INITIAL_ENGINE).when(mInitialSearchEngine).getKeyword();
        doReturn(SearchEngineType.SEARCH_ENGINE_GOOGLE)
                .when(mTemplateUrlService)
                .getSearchEngineTypeFromTemplateUrl(TEST_INITIAL_ENGINE);
        doReturn(mInitialSearchEngine)
                .when(mTemplateUrlService)
                .getDefaultSearchEngineTemplateUrl();
    }

    @Test
    @SmallTest
    public void receiveSearchEngineChoiceRequest() {
        SharedPreferencesManager prefs = ChromeSharedPreferences.getInstance();
        assertFalse(prefs.contains(ChromePreferenceKeys.SEARCH_ENGINE_CHOICE_REQUESTED_TIMESTAMP));
        SearchEngineChoiceNotification.receiveSearchEngineChoiceRequest();
        assertTrue(prefs.contains(ChromePreferenceKeys.SEARCH_ENGINE_CHOICE_REQUESTED_TIMESTAMP));

        long firstTimestamp =
                prefs.readLong(ChromePreferenceKeys.SEARCH_ENGINE_CHOICE_REQUESTED_TIMESTAMP);
        SearchEngineChoiceNotification.receiveSearchEngineChoiceRequest();
        long secondTimestamp =
                prefs.readLong(ChromePreferenceKeys.SEARCH_ENGINE_CHOICE_REQUESTED_TIMESTAMP);

        assertEquals(firstTimestamp, secondTimestamp);
    }

    @Test
    @SmallTest
    public void handleSearchEngineChoice_ignoredWhenNotRequested() {
        SharedPreferencesManager prefs = ChromeSharedPreferences.getInstance();
        assertFalse(prefs.contains(ChromePreferenceKeys.SEARCH_ENGINE_CHOICE_PRESENTED_VERSION));

        SearchEngineChoiceNotification.handleSearchEngineChoice(mContext, null);

        assertFalse(
                "When not requested, the call should have been ignored.",
                prefs.contains(ChromePreferenceKeys.SEARCH_ENGINE_CHOICE_PRESENTED_VERSION));

        assertEquals(
                0,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.SearchEngineChoice.Events",
                        SearchEngineChoiceMetrics.Events.SNACKBAR_SHOWN));
    }

    @Test
    @SmallTest
    public void handleSearchEngineChoice_ignoredWhenDefaultSearchManaged() {
        doReturn(true).when(mTemplateUrlService).isDefaultSearchManaged();
        SearchEngineChoiceNotification.receiveSearchEngineChoiceRequest();
        SharedPreferencesManager prefs = ChromeSharedPreferences.getInstance();
        assertFalse(prefs.contains(ChromePreferenceKeys.SEARCH_ENGINE_CHOICE_PRESENTED_VERSION));

        SearchEngineChoiceNotification.handleSearchEngineChoice(mContext, null);

        assertFalse(
                "When search engine settings are controlled by policy, the call should be ignored.",
                prefs.contains(ChromePreferenceKeys.SEARCH_ENGINE_CHOICE_PRESENTED_VERSION));

        assertEquals(
                0,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.SearchEngineChoice.Events",
                        SearchEngineChoiceMetrics.Events.SNACKBAR_SHOWN));
    }

    @Test
    @SmallTest
    public void handleSearchEngineChoice_performedFirstTime() {
        SearchEngineChoiceNotification.receiveSearchEngineChoiceRequest();
        SearchEngineChoiceNotification.handleSearchEngineChoice(mContext, mSnackbarManager);
        // TODO(fgorski): Snackbar content is scoped to its package, therefore cannot be verified
        // here at this time. See whether that can be fixed.
        verify(mSnackbarManager, times(1)).showSnackbar(any(Snackbar.class));

        assertEquals(
                "We are expecting exactly one snackbar shown event.",
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.SearchEngineChoice.Events",
                        SearchEngineChoiceMetrics.Events.SNACKBAR_SHOWN));

        SharedPreferencesManager prefs = ChromeSharedPreferences.getInstance();
        assertTrue(
                "Version of the app should be persisted upon prompting.",
                prefs.contains(ChromePreferenceKeys.SEARCH_ENGINE_CHOICE_PRESENTED_VERSION));

        assertEquals(
                "Presented version should be set to the current product version.",
                VersionInfo.getProductVersion(),
                prefs.readString(
                        ChromePreferenceKeys.SEARCH_ENGINE_CHOICE_PRESENTED_VERSION, null));
    }

    @Test
    @SmallTest
    public void handleSearchEngineChoice_ignoredOnSubsequentCalls() {
        SearchEngineChoiceNotification.receiveSearchEngineChoiceRequest();
        SearchEngineChoiceNotification.handleSearchEngineChoice(mContext, mSnackbarManager);
        verify(mSnackbarManager, times(1)).showSnackbar(any(Snackbar.class));

        SearchEngineChoiceNotification.handleSearchEngineChoice(mContext, mSnackbarManager);
        assertFalse(
                "Second call removes the preference for search engine choice before.",
                ChromeSharedPreferences.getInstance()
                        .contains(ChromePreferenceKeys.SEARCH_ENGINE_CHOICE_DEFAULT_TYPE_BEFORE));

        SearchEngineChoiceNotification.handleSearchEngineChoice(mContext, mSnackbarManager);

        // No increase in execution counter means it was not called again.
        verify(mSnackbarManager, times(1)).showSnackbar(any(Snackbar.class));
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.SearchEngineChoice.Events",
                        SearchEngineChoiceMetrics.Events.SNACKBAR_SHOWN));
    }

    @Test
    @SmallTest
    public void snackbarClicked() {
        SearchEngineChoiceNotification.receiveSearchEngineChoiceRequest();

        // We do not use a mock for SettingsNavigation here since the test needs to
        // verify that the launcher actually starts an activity.
        SettingsNavigationFactory.setInstanceForTesting(null);
        SearchEngineChoiceNotification.handleSearchEngineChoice(mContext, mSnackbarManager);
        verify(mSnackbarManager, times(1)).showSnackbar(mSnackbarArgument.capture());

        mSnackbarArgument.getValue().getController().onAction(null);
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.SearchEngineChoice.Events",
                        SearchEngineChoiceMetrics.Events.PROMPT_FOLLOWED));
        verify(mContext, times(1)).startActivity(any(Intent.class), isNull());
    }

    @Test
    @SmallTest
    public void reportSearchEngineChanged_whenNoChange() {
        SearchEngineChoiceNotification.receiveSearchEngineChoiceRequest();
        SearchEngineChoiceNotification.handleSearchEngineChoice(mContext, mSnackbarManager);
        verify(mSnackbarManager, times(1)).showSnackbar(mSnackbarArgument.capture());
        mSnackbarArgument.getValue().getController().onAction(null);

        // Simulates no change.
        SearchEngineChoiceNotification.handleSearchEngineChoice(mContext, mSnackbarManager);

        assertFalse(
                "First handleSearchEngineChoice call after prompt removes SE choice before pref.",
                ChromeSharedPreferences.getInstance()
                        .contains(ChromePreferenceKeys.SEARCH_ENGINE_CHOICE_DEFAULT_TYPE_BEFORE));

        assertEquals(
                0,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.SearchEngineChoice.Events",
                        SearchEngineChoiceMetrics.Events.SEARCH_ENGINE_CHANGED));
        assertEquals(
                0,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.SearchEngineChoice.ChosenSearchEngine",
                        SearchEngineType.SEARCH_ENGINE_DUCKDUCKGO));
    }

    @Test
    @SmallTest
    public void reportSearchEngineChanged_whenNoChangeOnFirstVisitToSettings() {
        SearchEngineChoiceNotification.receiveSearchEngineChoiceRequest();
        SearchEngineChoiceNotification.handleSearchEngineChoice(mContext, mSnackbarManager);
        verify(mSnackbarManager, times(1)).showSnackbar(mSnackbarArgument.capture());
        mSnackbarArgument.getValue().getController().onAction(null);

        // Simulates a change between the initialization, but reporting happens only the first time.
        SearchEngineChoiceNotification.handleSearchEngineChoice(mContext, mSnackbarManager);
        assertFalse(
                "First handleSearchEngineChoice call after prompt removes SE choice before pref.",
                ChromeSharedPreferences.getInstance()
                        .contains(ChromePreferenceKeys.SEARCH_ENGINE_CHOICE_DEFAULT_TYPE_BEFORE));

        doReturn(mAlternativeSearchEngine)
                .when(mTemplateUrlService)
                .getDefaultSearchEngineTemplateUrl();
        SearchEngineChoiceNotification.handleSearchEngineChoice(mContext, mSnackbarManager);

        assertEquals(
                0,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.SearchEngineChoice.Events",
                        SearchEngineChoiceMetrics.Events.SEARCH_ENGINE_CHANGED));
        assertEquals(
                0,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.SearchEngineChoice.ChosenSearchEngine",
                        SearchEngineType.SEARCH_ENGINE_DUCKDUCKGO));
    }

    @Test
    @SmallTest
    public void reportSearchEngineChanged_onlyFirstTime() {
        SearchEngineChoiceNotification.receiveSearchEngineChoiceRequest();
        SearchEngineChoiceNotification.handleSearchEngineChoice(mContext, mSnackbarManager);
        verify(mSnackbarManager, times(1)).showSnackbar(mSnackbarArgument.capture());
        mSnackbarArgument.getValue().getController().onAction(null);

        // Simulates a change of search engine on the first visit to settings.
        doReturn(mAlternativeSearchEngine)
                .when(mTemplateUrlService)
                .getDefaultSearchEngineTemplateUrl();
        SearchEngineChoiceNotification.handleSearchEngineChoice(mContext, mSnackbarManager);

        assertEquals(
                "Event is recorded when search engine was changed.",
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.SearchEngineChoice.Events",
                        SearchEngineChoiceMetrics.Events.SEARCH_ENGINE_CHANGED));
        assertEquals(
                "Newly chosen search engine type should be recoreded.",
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.SearchEngineChoice.ChosenSearchEngine",
                        SearchEngineType.SEARCH_ENGINE_DUCKDUCKGO));

        assertFalse(
                "First handleSearchEngineChoice call after prompt removes SE choice before pref.",
                ChromeSharedPreferences.getInstance()
                        .contains(ChromePreferenceKeys.SEARCH_ENGINE_CHOICE_DEFAULT_TYPE_BEFORE));

        SearchEngineChoiceNotification.handleSearchEngineChoice(mContext, mSnackbarManager);

        assertEquals(
                "Event should only be recorded once, therefore count should be still 1.",
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.SearchEngineChoice.Events",
                        SearchEngineChoiceMetrics.Events.SEARCH_ENGINE_CHANGED));
        assertEquals(
                "New Search Engine shoudl only be reported once, therefore count should be 1",
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.SearchEngineChoice.ChosenSearchEngine",
                        SearchEngineType.SEARCH_ENGINE_DUCKDUCKGO));
    }
}
