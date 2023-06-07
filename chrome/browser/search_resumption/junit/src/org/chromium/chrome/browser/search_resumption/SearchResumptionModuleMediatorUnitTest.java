// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_resumption;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.endsWith;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.view.View;
import android.view.ViewStub;

import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.UserDataHost;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteController;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteController.OnSuggestionsReceivedListener;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteControllerProvider;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.search_resumption.SearchResumptionModuleUtils.ModuleNotShownReason;
import org.chromium.chrome.browser.search_resumption.SearchResumptionModuleUtils.ModuleShowStatus;
import org.chromium.chrome.browser.search_resumption.SearchResumptionUserData.SuggestionResult;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.AutocompleteResult;
import org.chromium.components.omnibox.OmniboxSuggestionType;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.sync.SyncService;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Unit tests for {@link SearchResumptionModuleMediator}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE,
        shadows = {SearchResumptionModuleMediatorUnitTest.ShadowChromeFeatureList.class})
@SuppressWarnings("DoNotMock") // Mocking GURL
public class SearchResumptionModuleMediatorUnitTest {
    @Implements(ChromeFeatureList.class)
    static class ShadowChromeFeatureList {
        static final Map<String, Boolean> sParamValues = new HashMap<>();
        static boolean sEnableSearchResumptionModule;

        @Implementation
        public static boolean isEnabled(String featureName) {
            return featureName.equals(ChromeFeatureList.SEARCH_RESUMPTION_MODULE_ANDROID)
                    && sEnableSearchResumptionModule;
        }

        @Implementation
        public static boolean getFieldTrialParamByFeatureAsBoolean(
                String featureName, String paramName, boolean defaultValue) {
            return sParamValues.containsKey(paramName) ? sParamValues.get(paramName) : defaultValue;
        }
    }

    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    @Mock
    private Tab mTabToTrack;
    @Mock
    private Tab mTab;
    @Mock
    private ViewStub mParent;
    @Mock
    private SearchResumptionModuleView mModuleLayoutView;
    @Mock
    private SearchResumptionTileContainerView mSuggestionTilesContainerView;
    @Mock
    private AutocompleteControllerProvider mAutocompleteProvider;
    @Mock
    private AutocompleteController mAutocompleteController;
    @Mock
    SearchResumptionTileBuilder mTileBuilder;
    @Mock
    private Profile mProfile;
    @Mock
    private TemplateUrlService mTemplateUrlService;
    @Captor
    private ArgumentCaptor<OnSuggestionsReceivedListener> mListener;

    @Mock
    private AutocompleteMatch mSearchSuggest1;
    @Mock
    private AutocompleteMatch mSearchSuggest2;
    @Mock
    private AutocompleteMatch mNonSearchSuggest1;
    @Mock
    private AutocompleteResult mAutocompleteResult;
    @Mock
    private IdentityServicesProvider mIdentityServicesProvider;
    @Mock
    private SigninManager mSignInManager;
    @Mock
    private SyncService mSyncServiceMock;

    private GURL mUrlToTrack;
    private String[] mSuggestionTexts;
    private GURL[] mSuggestionUrls;
    private UserActionTester mActionTester;
    private UserDataHost mUserDataHost;
    private SearchResumptionModuleMediator mMediator;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mUserDataHost = new UserDataHost();
        doReturn(mAutocompleteController).when(mAutocompleteProvider).get(any());
        mUrlToTrack = JUnitTestGURLs.getGURL(JUnitTestGURLs.EXAMPLE_URL);
        doReturn(mUrlToTrack).when(mTabToTrack).getUrl();
        GURL currentUrl = JUnitTestGURLs.getGURL(JUnitTestGURLs.NTP_URL);
        doReturn(currentUrl).when(mTab).getUrl();
        doReturn(false).when(mTab).canGoForward();
        doReturn(mUserDataHost).when(mTab).getUserDataHost();
        doReturn(mModuleLayoutView).when(mParent).inflate();
        doReturn(mSuggestionTilesContainerView)
                .when(mModuleLayoutView)
                .findViewById(R.id.search_resumption_module_tiles_container);
        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);
        when(mTemplateUrlService.isSearchResultsPageFromDefaultSearchProvider(any()))
                .thenReturn(false);

        doReturn(OmniboxSuggestionType.SEARCH_SUGGEST).when(mSearchSuggest1).getType();
        doReturn("suggestion 1").when(mSearchSuggest1).getDisplayText();
        doReturn(OmniboxSuggestionType.SEARCH_SUGGEST).when(mSearchSuggest2).getType();
        doReturn("suggestion 2").when(mSearchSuggest2).getDisplayText();
        doReturn(OmniboxSuggestionType.TILE_NAVSUGGEST).when(mNonSearchSuggest1).getType();
        doReturn("non search suggestion 1").when(mNonSearchSuggest1).getDisplayText();

        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);
        doReturn(mSignInManager).when(mIdentityServicesProvider).getSigninManager(any());

        SyncServiceFactory.overrideForTests(mSyncServiceMock);

        mActionTester = new UserActionTester();
    }

    @After
    public void tearDown() {
        IdentityServicesProvider.setInstanceForTests(null);
        ShadowChromeFeatureList.sEnableSearchResumptionModule = false;
        ShadowChromeFeatureList.sParamValues.clear();
        mActionTester.tearDown();
        mSuggestionTexts = null;
        mSuggestionUrls = null;
        SharedPreferencesManager.getInstance().removeKey(
                ChromePreferenceKeys.SEARCH_RESUMPTION_MODULE_COLLAPSE_ON_NTP);
    }

    @Test
    @MediumTest
    public void testDoNotBuildModuleWithoutEnoughSuggestions() {
        createMediator(null, false /* useNewServiceEnabled */);
        List<AutocompleteMatch> list = Arrays.asList(mNonSearchSuggest1, mNonSearchSuggest1);
        doReturn(list).when(mAutocompleteResult).getSuggestionsList();

        mMediator.onSuggestionsReceived(mAutocompleteResult, "", true);
        verify(mParent, times(0)).inflate();
        Assert.assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(
                        SearchResumptionModuleUtils.UMA_MODULE_SHOW, ModuleShowStatus.EXPANDED));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        SearchResumptionModuleUtils.UMA_MODULE_NOT_SHOW,
                        ModuleNotShownReason.NOT_ENOUGH_RESULT));
    }

    @Test
    @MediumTest
    public void testShowModuleWithEnoughResults() {
        createMediator(null, false /* useNewServiceEnabled */);
        List<AutocompleteMatch> list =
                Arrays.asList(mNonSearchSuggest1, mSearchSuggest1, mSearchSuggest2);
        doReturn(list).when(mAutocompleteResult).getSuggestionsList();

        mMediator.onSuggestionsReceived(mAutocompleteResult, "", true);
        verify(mParent, times(1)).inflate();
        Assert.assertEquals(View.VISIBLE, mSuggestionTilesContainerView.getVisibility());
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        SearchResumptionModuleUtils.UMA_MODULE_SHOW, ModuleShowStatus.EXPANDED));
    }

    @Test
    @SmallTest
    public void testShowModuleWithCachedResults() {
        List<AutocompleteMatch> list =
                Arrays.asList(mNonSearchSuggest1, mSearchSuggest1, mSearchSuggest2);
        SuggestionResult suggestionResult = new SuggestionResult(mUrlToTrack, list);

        createMediator(suggestionResult, false /* useNewServiceEnabled */);
        verify(mParent, times(1)).inflate();
        Assert.assertEquals(View.VISIBLE, mSuggestionTilesContainerView.getVisibility());
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        SearchResumptionModuleUtils.UMA_MODULE_SHOW_CACHED,
                        ModuleShowStatus.EXPANDED));
    }

    @Test
    @MediumTest
    public void testDoNotBuildModuleWithoutEnoughSuggestions_newServiceAPI() {
        String[] texts = {"suggestion 1"};
        GURL[] gUrls = {JUnitTestGURLs.getGURL(JUnitTestGURLs.URL_1)};

        createMediator(null, true /* useNewServiceEnabled */);
        mMediator.onSuggestionsAvailable(texts, gUrls);
        verify(mParent, times(0)).inflate();
        Assert.assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(
                        SearchResumptionModuleUtils.UMA_MODULE_SHOW, ModuleShowStatus.EXPANDED));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        SearchResumptionModuleUtils.UMA_MODULE_NOT_SHOW,
                        ModuleNotShownReason.NOT_ENOUGH_RESULT));
    }

    @Test
    @MediumTest
    public void testShowModuleWithEnoughResults_newServiceAPI() {
        initSuggestions();

        createMediator(null, true /* useNewServiceEnabled */);
        mMediator.onSuggestionsAvailable(mSuggestionTexts, mSuggestionUrls);
        verify(mParent, times(1)).inflate();
        Assert.assertEquals(View.VISIBLE, mSuggestionTilesContainerView.getVisibility());
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        SearchResumptionModuleUtils.UMA_MODULE_SHOW, ModuleShowStatus.EXPANDED));

        mMediator.onExpandedOrCollapsed(true /* expand */);
        Assert.assertTrue(
                mActionTester.getActions().contains(SearchResumptionModuleUtils.ACTION_EXPAND));

        mMediator.onExpandedOrCollapsed(false /* expand */);
        Assert.assertTrue(
                mActionTester.getActions().contains(SearchResumptionModuleUtils.ACTION_COLLAPSE));
    }

    @Test
    @SmallTest
    public void testShowModuleWithCachedResults_newServiceAPI() {
        initSuggestions();
        SuggestionResult suggestionResult = createCachedSuggestions();

        createMediator(suggestionResult, true /* useNewServiceEnabled */);
        verify(mParent, times(1)).inflate();
        Assert.assertEquals(View.VISIBLE, mSuggestionTilesContainerView.getVisibility());
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        SearchResumptionModuleUtils.UMA_MODULE_SHOW_CACHED,
                        ModuleShowStatus.EXPANDED));
    }

    @Test
    @MediumTest
    public void testModuleVisibility() {
        testShowModuleWithEnoughResults();
        mMediator.onSignedOut();
        verify(mModuleLayoutView, times(1)).setVisibility(View.GONE);

        mMediator.onSignedIn();
        verify(mModuleLayoutView, times(1)).setVisibility(View.VISIBLE);

        doReturn(false).when(mTemplateUrlService).isDefaultSearchEngineGoogle();
        mMediator.onTemplateURLServiceChanged();
        verify(mModuleLayoutView, times(2)).setVisibility(View.GONE);

        doReturn(true).when(mTemplateUrlService).isDefaultSearchEngineGoogle();
        mMediator.onTemplateURLServiceChanged();
        verify(mModuleLayoutView, times(2)).setVisibility(View.VISIBLE);

        when(mSyncServiceMock.hasKeepEverythingSynced()).thenReturn(false);
        mMediator.syncStateChanged();
        verify(mModuleLayoutView, times(3)).setVisibility(View.GONE);

        when(mSyncServiceMock.hasKeepEverythingSynced()).thenReturn(true);
        mMediator.syncStateChanged();
        verify(mModuleLayoutView, times(3)).setVisibility(View.VISIBLE);
    }

    @Test
    @SmallTest
    public void testDestroy() {
        testShowModuleWithEnoughResults();
        mMediator.destroy();

        verify(mModuleLayoutView, times(1)).destroy();
    }

    private void createMediator(SuggestionResult cachedSuggestions, boolean useNewServiceEnabled) {
        mMediator = new SearchResumptionModuleMediator(mParent, mAutocompleteProvider, mTabToTrack,
                mTab, mProfile, mTileBuilder, cachedSuggestions);
        if (!useNewServiceEnabled && cachedSuggestions == null) {
            verify(mAutocompleteController).addOnSuggestionsReceivedListener(mListener.capture());
            verify(mAutocompleteController, times(1))
                    .startZeroSuggest(any(), endsWith(mUrlToTrack.getSpec()), anyInt(), any());
        }

        ShadowChromeFeatureList.sEnableSearchResumptionModule = true;
        ShadowChromeFeatureList.sParamValues.put(
                SearchResumptionModuleUtils.USE_NEW_SERVICE_PARAM, useNewServiceEnabled);
        Assert.assertEquals(useNewServiceEnabled,
                ShadowChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                        ChromeFeatureList.SEARCH_RESUMPTION_MODULE_ANDROID,
                        SearchResumptionModuleUtils.USE_NEW_SERVICE_PARAM, false));
    }

    private void initSuggestions() {
        mSuggestionTexts = new String[] {"suggestion 1", "suggestion2"};
        mSuggestionUrls = new GURL[] {JUnitTestGURLs.getGURL(JUnitTestGURLs.URL_1),
                JUnitTestGURLs.getGURL(JUnitTestGURLs.URL_2)};
    }

    private SuggestionResult createCachedSuggestions() {
        initSuggestions();
        return new SuggestionResult(mUrlToTrack, mSuggestionTexts, mSuggestionUrls);
    }
}
