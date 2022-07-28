// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_resumption;

import static org.mockito.ArgumentMatchers.endsWith;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyInt;
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
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.OmniboxSuggestionType;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteController;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteController.OnSuggestionsReceivedListener;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteControllerJni;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.search_resumption.SearchResumptionModuleUtils.ModuleNotShownReason;
import org.chromium.chrome.browser.search_resumption.SearchResumptionModuleUtils.ModuleShowStatus;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.sync.SyncService;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.AutocompleteResult;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.url.GURL;

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

    // The search suggestions are meant to be shown on any website.
    private static final String URL_TO_TRACK = "/foo.com";

    @Rule
    public JniMocker mJniMocker = new JniMocker();
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    @Mock
    private Tab mTabToTrack;
    @Mock
    private ViewStub mParent;
    @Mock
    private SearchResumptionModuleView mModuleLayoutView;
    @Mock
    private SearchResumptionTileContainerView mSuggestionTilesContainerView;
    @Mock
    private AutocompleteController mAutocompleteController;
    @Mock
    private AutocompleteController.Natives mControllerJniMock;
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

    private UserActionTester mActionTester;
    private SearchResumptionModuleMediator mMediator;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(AutocompleteControllerJni.TEST_HOOKS, mControllerJniMock);
        doReturn(mAutocompleteController).when(mControllerJniMock).getForProfile(any());

        GURL url = createMockGurl(URL_TO_TRACK);
        doReturn(url).when(mTabToTrack).getUrl();
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

        SyncService.overrideForTests(mSyncServiceMock);

        mActionTester = new UserActionTester();

        mMediator =
                new SearchResumptionModuleMediator(mParent, mTabToTrack, mProfile, mTileBuilder);
        verify(mAutocompleteController).addOnSuggestionsReceivedListener(mListener.capture());
        verify(mAutocompleteController, times(1))
                .startZeroSuggest(any(), endsWith(URL_TO_TRACK), anyInt(), any());

        ShadowChromeFeatureList.sEnableSearchResumptionModule = true;
        ShadowChromeFeatureList.sParamValues.put(
                SearchResumptionModuleUtils.USE_NEW_SERVICE_PARAM, false);
        Assert.assertFalse(ShadowChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.SEARCH_RESUMPTION_MODULE_ANDROID,
                SearchResumptionModuleUtils.USE_NEW_SERVICE_PARAM, false));
    }

    @After
    public void tearDown() {
        IdentityServicesProvider.setInstanceForTests(null);
        ShadowChromeFeatureList.sEnableSearchResumptionModule = false;
        ShadowChromeFeatureList.sParamValues.clear();
        mActionTester.tearDown();
    }

    @Test
    @MediumTest
    public void testDoNotBuildModuleWithoutEnoughSuggestions() {
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
    @MediumTest
    public void testDoNotBuildModuleWithoutEnoughSuggestions_newServiceAPI() {
        String[] texts = {"suggestion 1"};
        GURL[] gUrls = {createMockGurl("foo.com")};

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
        String[] texts = {"suggestion 1", "suggestion2"};
        GURL[] gUrls = {createMockGurl("foo.com"), createMockGurl("bar.com")};

        mMediator.onSuggestionsAvailable(texts, gUrls);
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

    private static GURL createMockGurl(String url) {
        GURL gurl = Mockito.mock(GURL.class);
        when(gurl.getSpec()).thenReturn(url);
        return gurl;
    }
}
