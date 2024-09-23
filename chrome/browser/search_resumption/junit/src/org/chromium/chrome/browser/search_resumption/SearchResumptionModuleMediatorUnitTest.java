// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_resumption;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
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
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.FeatureList;
import org.chromium.base.UserDataHost;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteController;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteController.OnSuggestionsReceivedListener;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteControllerJni;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.search_resumption.SearchResumptionModuleUtils.ModuleNotShownReason;
import org.chromium.chrome.browser.search_resumption.SearchResumptionModuleUtils.ModuleShowStatus;
import org.chromium.chrome.browser.search_resumption.SearchResumptionUserData.SuggestionResult;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.AutocompleteResult;
import org.chromium.components.omnibox.OmniboxSuggestionType;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.sync.SyncService;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.Arrays;
import java.util.List;

/** Unit tests for {@link SearchResumptionModuleMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@SuppressWarnings("DoNotMock") // Mocking GURL
public class SearchResumptionModuleMediatorUnitTest {
    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock private Tab mTabToTrack;
    @Mock private Tab mTab;
    @Mock private ViewStub mParent;
    @Mock private SearchResumptionModuleView mModuleLayoutView;
    @Mock private SearchResumptionTileContainerView mSuggestionTilesContainerView;
    @Mock private AutocompleteController mAutocompleteController;
    @Mock private AutocompleteController.Natives mControllerJniMock;
    @Mock SearchResumptionTileBuilder mTileBuilder;
    @Mock private Profile mProfile;
    @Mock private TemplateUrlService mTemplateUrlService;
    @Captor private ArgumentCaptor<OnSuggestionsReceivedListener> mListener;

    @Mock private AutocompleteMatch mSearchSuggest1;
    @Mock private AutocompleteMatch mSearchSuggest2;
    @Mock private AutocompleteMatch mNonSearchSuggest1;
    @Mock private AutocompleteResult mAutocompleteResult;
    @Mock private IdentityServicesProvider mIdentityServicesProvider;
    @Mock private SigninManager mSignInManager;
    @Mock private SyncService mSyncServiceMock;

    private GURL mUrlToTrack;
    private String[] mSuggestionTexts;
    private GURL[] mSuggestionUrls;
    private UserActionTester mActionTester;
    private UserDataHost mUserDataHost;
    private SearchResumptionModuleMediator mMediator;
    private FeatureList.TestValues mFeatureListValues;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mFeatureListValues = new FeatureList.TestValues();
        FeatureList.setTestValues(mFeatureListValues);
        mFeatureListValues.addFeatureFlagOverride(
                ChromeFeatureList.SEARCH_RESUMPTION_MODULE_ANDROID, false);

        mUserDataHost = new UserDataHost();
        mJniMocker.mock(AutocompleteControllerJni.TEST_HOOKS, mControllerJniMock);
        doReturn(mAutocompleteController).when(mControllerJniMock).getForProfile(any());
        mUrlToTrack = JUnitTestGURLs.EXAMPLE_URL;
        doReturn(mUrlToTrack).when(mTabToTrack).getUrl();
        GURL currentUrl = JUnitTestGURLs.NTP_URL;
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

        SyncServiceFactory.setInstanceForTesting(mSyncServiceMock);

        mActionTester = new UserActionTester();
    }

    @After
    public void tearDown() {
        mActionTester.tearDown();
    }

    @Test
    @MediumTest
    public void testDoNotBuildModuleWithoutEnoughSuggestions() {
        createMediator(null, /* useNewServiceEnabled= */ false);
        List<AutocompleteMatch> list = Arrays.asList(mNonSearchSuggest1, mNonSearchSuggest1);
        doReturn(list).when(mAutocompleteResult).getSuggestionsList();

        mMediator.onSuggestionsReceived(mAutocompleteResult, true);
        verify(mParent, times(0)).inflate();
        Assert.assertEquals(
                0,
                RecordHistogram.getHistogramValueCountForTesting(
                        SearchResumptionModuleUtils.UMA_MODULE_SHOW, ModuleShowStatus.EXPANDED));
        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        SearchResumptionModuleUtils.UMA_MODULE_NOT_SHOW,
                        ModuleNotShownReason.NOT_ENOUGH_RESULT));
    }

    @Test
    @MediumTest
    public void testShowModuleWithEnoughResults() {
        createMediator(null, /* useNewServiceEnabled= */ false);
        List<AutocompleteMatch> list =
                Arrays.asList(mNonSearchSuggest1, mSearchSuggest1, mSearchSuggest2);
        doReturn(list).when(mAutocompleteResult).getSuggestionsList();

        mMediator.onSuggestionsReceived(mAutocompleteResult, true);
        verify(mParent, times(1)).inflate();
        Assert.assertEquals(View.VISIBLE, mSuggestionTilesContainerView.getVisibility());
        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        SearchResumptionModuleUtils.UMA_MODULE_SHOW, ModuleShowStatus.EXPANDED));
    }

    @Test
    @SmallTest
    public void testShowModuleWithCachedResults() {
        List<AutocompleteMatch> list =
                Arrays.asList(mNonSearchSuggest1, mSearchSuggest1, mSearchSuggest2);
        SuggestionResult suggestionResult = new SuggestionResult(mUrlToTrack, list);

        createMediator(suggestionResult, /* useNewServiceEnabled= */ false);
        verify(mParent, times(1)).inflate();
        Assert.assertEquals(View.VISIBLE, mSuggestionTilesContainerView.getVisibility());
        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        SearchResumptionModuleUtils.UMA_MODULE_SHOW_CACHED,
                        ModuleShowStatus.EXPANDED));
    }

    @Test
    @MediumTest
    public void testDoNotBuildModuleWithoutEnoughSuggestions_newServiceAPI() {
        String[] texts = {"suggestion 1"};
        GURL[] gUrls = {JUnitTestGURLs.URL_1};

        createMediator(null, /* useNewServiceEnabled= */ true);
        mMediator.onSuggestionsAvailable(texts, gUrls);
        verify(mParent, times(0)).inflate();
        Assert.assertEquals(
                0,
                RecordHistogram.getHistogramValueCountForTesting(
                        SearchResumptionModuleUtils.UMA_MODULE_SHOW, ModuleShowStatus.EXPANDED));
        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        SearchResumptionModuleUtils.UMA_MODULE_NOT_SHOW,
                        ModuleNotShownReason.NOT_ENOUGH_RESULT));
    }

    @Test
    @MediumTest
    public void testShowModuleWithEnoughResults_newServiceAPI() {
        initSuggestions();

        createMediator(null, /* useNewServiceEnabled= */ true);
        mMediator.onSuggestionsAvailable(mSuggestionTexts, mSuggestionUrls);
        verify(mParent, times(1)).inflate();
        Assert.assertEquals(View.VISIBLE, mSuggestionTilesContainerView.getVisibility());
        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        SearchResumptionModuleUtils.UMA_MODULE_SHOW, ModuleShowStatus.EXPANDED));

        mMediator.onExpandedOrCollapsed(/* expand= */ true);
        Assert.assertTrue(
                mActionTester.getActions().contains(SearchResumptionModuleUtils.ACTION_EXPAND));

        mMediator.onExpandedOrCollapsed(/* expand= */ false);
        Assert.assertTrue(
                mActionTester.getActions().contains(SearchResumptionModuleUtils.ACTION_COLLAPSE));
    }

    @Test
    @SmallTest
    public void testShowModuleWithCachedResults_newServiceAPI() {
        initSuggestions();
        SuggestionResult suggestionResult = createCachedSuggestions();

        createMediator(suggestionResult, /* useNewServiceEnabled= */ true);
        verify(mParent, times(1)).inflate();
        Assert.assertEquals(View.VISIBLE, mSuggestionTilesContainerView.getVisibility());
        Assert.assertEquals(
                1,
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
        mMediator =
                new SearchResumptionModuleMediator(
                        mParent,
                        mTabToTrack,
                        mTab,
                        mProfile,
                        mTileBuilder,
                        cachedSuggestions);
        if (!useNewServiceEnabled && cachedSuggestions == null) {
            verify(mAutocompleteController).addOnSuggestionsReceivedListener(mListener.capture());
            verify(mAutocompleteController, times(1))
                    .startZeroSuggest(
                            any(),
                            eq(mUrlToTrack),
                            anyInt(),
                            any(),
                            /* isOnFocusContext= */ eq(false));
        }

        mFeatureListValues.addFeatureFlagOverride(
                ChromeFeatureList.SEARCH_RESUMPTION_MODULE_ANDROID, true);
        mFeatureListValues.addFieldTrialParamOverride(
                ChromeFeatureList.SEARCH_RESUMPTION_MODULE_ANDROID,
                SearchResumptionModuleUtils.USE_NEW_SERVICE_PARAM,
                String.valueOf(useNewServiceEnabled));
    }

    private void initSuggestions() {
        mSuggestionTexts = new String[] {"suggestion 1", "suggestion2"};
        mSuggestionUrls = new GURL[] {JUnitTestGURLs.URL_1, JUnitTestGURLs.URL_2};
    }

    private SuggestionResult createCachedSuggestions() {
        initSuggestions();
        return new SuggestionResult(mUrlToTrack, mSuggestionTexts, mSuggestionUrls);
    }
}
