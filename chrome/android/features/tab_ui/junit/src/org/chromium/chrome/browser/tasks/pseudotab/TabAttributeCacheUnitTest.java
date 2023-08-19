// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.pseudotab;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

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

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileJni;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.tab.TabImpl;
import org.chromium.chrome.browser.tab.state.CriticalPersistedTabData;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorImpl;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabObserver;
import org.chromium.chrome.browser.tasks.pseudotab.TabAttributeCache.LastSearchTermProvider;
import org.chromium.chrome.browser.tasks.tab_management.TabUiUnitTestUtils;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.NavigationEntry;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.NavigationHistory;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;
import java.util.List;

/**
 * Unit tests for {@link TabAttributeCache}.
 */
@SuppressWarnings("ResultOfMethodCallIgnored")
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabAttributeCacheUnitTest {
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();
    @Rule
    public JniMocker jniMocker = new JniMocker();

    private static final int TAB1_ID = 456;
    private static final int TAB2_ID = 789;
    private static final int POSITION1 = 0;
    private static final int POSITION2 = 1;

    @Mock
    TabModelSelectorImpl mTabModelSelector;
    @Mock
    TabModelFilterProvider mTabModelFilterProvider;
    @Mock
    TabModelFilter mTabModelFilter;
    @Mock
    TabModel mTabModel;
    @Mock
    CriticalPersistedTabData mCriticalPersistedTabData1;
    @Mock
    CriticalPersistedTabData mCriticalPersistedTabData2;
    @Captor
    ArgumentCaptor<TabModelObserver> mTabModelObserverCaptor;
    @Captor
    ArgumentCaptor<TabModelSelectorObserver> mTabModelSelectorObserverCaptor;
    @Captor
    ArgumentCaptor<TabModelSelectorTabObserver> mTabObserverCaptor;
    @Mock
    Profile.Natives mProfileJniMock;

    private TabImpl mTab1;
    private TabImpl mTab2;
    private TabAttributeCache mCache;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        jniMocker.mock(ProfileJni.TEST_HOOKS, mProfileJniMock);

        mTab1 = TabUiUnitTestUtils.prepareTab(TAB1_ID, mCriticalPersistedTabData1);
        mTab2 = TabUiUnitTestUtils.prepareTab(TAB2_ID, mCriticalPersistedTabData2);

        List<TabModel> tabModelList = new ArrayList<>();
        tabModelList.add(mTabModel);

        doReturn(tabModelList).when(mTabModelSelector).getModels();
        doReturn(mTabModelFilterProvider).when(mTabModelSelector).getTabModelFilterProvider();
        doReturn(mTabModelFilter).when(mTabModelFilterProvider).getTabModelFilter(eq(false));

        doNothing().when(mTabModelFilter).addObserver(mTabModelObserverCaptor.capture());

        doNothing().when(mTabModelSelector).addObserver(mTabModelSelectorObserverCaptor.capture());

        doReturn(mTab1).when(mTabModel).getTabAt(POSITION1);
        doReturn(mTab2).when(mTabModel).getTabAt(POSITION2);
        doReturn(POSITION1).when(mTabModel).indexOf(mTab1);
        doReturn(POSITION2).when(mTabModel).indexOf(mTab2);
        doNothing().when(mTab1).addObserver(mTabObserverCaptor.capture());
        doNothing().when(mTab2).addObserver(mTabObserverCaptor.capture());
        doReturn(0).when(mTabModel).index();
        doReturn(2).when(mTabModel).getCount();
        doReturn(mTabModel).when(mTabModel).getComprehensiveModel();

        mCache = new TabAttributeCache(mTabModelSelector);
    }

    @After
    public void tearDown() {
        mCache.destroy();
    }

    @Test
    public void updateUrl() {
        GURL url = JUnitTestGURLs.getGURL(JUnitTestGURLs.EXAMPLE_URL);
        doReturn(url).when(mTab1).getUrl();

        Assert.assertNotEquals(url, TabAttributeCache.getUrl(TAB1_ID));

        mTabObserverCaptor.getValue().onUrlUpdated(mTab1);
        Assert.assertEquals(url, TabAttributeCache.getUrl(TAB1_ID));

        mTabModelSelectorObserverCaptor.getValue().onTabStateInitialized();
        mTabModelObserverCaptor.getValue().tabClosureCommitted(mTab1);
        Assert.assertNotEquals(url, TabAttributeCache.getUrl(TAB1_ID));
    }

    @Test
    public void updateUrl_incognito() {
        String url = JUnitTestGURLs.EXAMPLE_URL;
        doReturn(JUnitTestGURLs.getGURL(url)).when(mTab1).getUrl();
        doReturn(true).when(mTab1).isIncognito();

        mTabObserverCaptor.getValue().onUrlUpdated(mTab1);
        Assert.assertNotEquals(url, TabAttributeCache.getUrl(TAB1_ID));
    }

    @Test
    public void updateTitle() {
        String title = "title 1";
        doReturn(title).when(mTab1).getTitle();

        Assert.assertNotEquals(title, TabAttributeCache.getTitle(TAB1_ID));

        mTabObserverCaptor.getValue().onTitleUpdated(mTab1);
        Assert.assertEquals(title, TabAttributeCache.getTitle(TAB1_ID));

        mTabModelSelectorObserverCaptor.getValue().onTabStateInitialized();
        mTabModelObserverCaptor.getValue().tabClosureCommitted(mTab1);
        Assert.assertNotEquals(title, TabAttributeCache.getTitle(TAB1_ID));
    }

    @Test
    public void updateTitle_incognito() {
        String title = "title 1";
        doReturn(title).when(mTab1).getTitle();
        doReturn(true).when(mTab1).isIncognito();

        mTabObserverCaptor.getValue().onTitleUpdated(mTab1);
        Assert.assertNotEquals(title, TabAttributeCache.getTitle(TAB1_ID));
    }

    @Test
    public void updateRootId() {
        int rootId = 1337;
        doReturn(rootId).when(mCriticalPersistedTabData1).getRootId();

        Assert.assertNotEquals(rootId, TabAttributeCache.getRootId(TAB1_ID));

        mTabObserverCaptor.getValue().onRootIdChanged(mTab1, rootId);
        Assert.assertEquals(rootId, TabAttributeCache.getRootId(TAB1_ID));

        mTabModelSelectorObserverCaptor.getValue().onTabStateInitialized();
        mTabModelObserverCaptor.getValue().tabClosureCommitted(mTab1);
        Assert.assertNotEquals(rootId, TabAttributeCache.getRootId(TAB1_ID));
    }

    @Test
    public void updateRootId_incognito() {
        int rootId = 1337;
        doReturn(rootId).when(mCriticalPersistedTabData1).getRootId();
        doReturn(true).when(mTab1).isIncognito();

        mTabObserverCaptor.getValue().onRootIdChanged(mTab1, rootId);
        Assert.assertNotEquals(rootId, TabAttributeCache.getRootId(TAB1_ID));
    }

    @Test
    public void updateTimestamp() {
        long timestamp = 1337;
        doReturn(timestamp).when(mCriticalPersistedTabData1).getTimestampMillis();

        Assert.assertNotEquals(timestamp, TabAttributeCache.getTimestampMillis(TAB1_ID));

        mTabObserverCaptor.getValue().onTimestampChanged(mTab1, timestamp);
        Assert.assertEquals(timestamp, TabAttributeCache.getTimestampMillis(TAB1_ID));

        mTabModelSelectorObserverCaptor.getValue().onTabStateInitialized();
        mTabModelObserverCaptor.getValue().tabClosureCommitted(mTab1);
        Assert.assertNotEquals(timestamp, TabAttributeCache.getTimestampMillis(TAB1_ID));
    }

    @Test
    public void updateTimestamp_incognito() {
        long timestamp = 1337;
        doReturn(timestamp).when(mCriticalPersistedTabData1).getTimestampMillis();
        doReturn(true).when(mTab1).isIncognito();

        mTabObserverCaptor.getValue().onTimestampChanged(mTab1, timestamp);
        Assert.assertNotEquals(timestamp, TabAttributeCache.getTimestampMillis(TAB1_ID));
    }

    @Test
    public void updateLastSearchTerm() {
        String searchTerm = "chromium";

        LastSearchTermProvider lastSearchTermProvider = mock(LastSearchTermProvider.class);
        TabAttributeCache.setLastSearchTermMockForTesting(lastSearchTermProvider);
        NavigationHandle navigationHandle = mock(NavigationHandle.class);

        Assert.assertNull(TabAttributeCache.getLastSearchTerm(TAB1_ID));

        doReturn(searchTerm).when(lastSearchTermProvider).getLastSearchTerm(mTab1);
        WebContents webContents = mock(WebContents.class);
        doReturn(webContents).when(mTab1).getWebContents();

        mTabObserverCaptor.getValue().onDidFinishNavigationInPrimaryMainFrame(
                mTab1, navigationHandle);
        Assert.assertEquals(searchTerm, TabAttributeCache.getLastSearchTerm(TAB1_ID));

        // Empty strings should propagate.
        doReturn("").when(lastSearchTermProvider).getLastSearchTerm(mTab1);
        mTabObserverCaptor.getValue().onDidFinishNavigationInPrimaryMainFrame(
                mTab1, navigationHandle);
        Assert.assertEquals("", TabAttributeCache.getLastSearchTerm(TAB1_ID));

        // Null should also propagate.
        doReturn(null).when(lastSearchTermProvider).getLastSearchTerm(mTab1);
        mTabObserverCaptor.getValue().onDidFinishNavigationInPrimaryMainFrame(
                mTab1, navigationHandle);
        Assert.assertNull(TabAttributeCache.getLastSearchTerm(TAB1_ID));

        mTabModelSelectorObserverCaptor.getValue().onTabStateInitialized();
        mTabModelObserverCaptor.getValue().tabClosureCommitted(mTab1);
        Assert.assertNull(TabAttributeCache.getLastSearchTerm(TAB1_ID));
    }

    @Test
    public void updateLastSearchTerm_incognito() {
        String searchTerm = "chromium";
        doReturn(true).when(mTab1).isIncognito();

        LastSearchTermProvider lastSearchTermProvider = mock(LastSearchTermProvider.class);
        TabAttributeCache.setLastSearchTermMockForTesting(lastSearchTermProvider);
        doReturn(searchTerm).when(lastSearchTermProvider).getLastSearchTerm(mTab1);

        mTabObserverCaptor.getValue().onDidFinishNavigationInPrimaryMainFrame(mTab1, null);
        Assert.assertNull(TabAttributeCache.getLastSearchTerm(TAB1_ID));
    }

    @Test
    public void findLastSearchTerm() {
        GURL otherUrl = JUnitTestGURLs.getGURL(JUnitTestGURLs.EXAMPLE_URL);
        GURL searchUrl = JUnitTestGURLs.getGURL(JUnitTestGURLs.SEARCH_URL);
        String searchTerm = "test";
        GURL searchUrl2 = JUnitTestGURLs.getGURL(JUnitTestGURLs.SEARCH_2_URL);
        String searchTerm2 = "query";

        TemplateUrlService service = Mockito.mock(TemplateUrlService.class);
        doReturn(null).when(service).getSearchQueryForUrl(otherUrl);
        doReturn(searchTerm).when(service).getSearchQueryForUrl(searchUrl);
        doReturn(searchTerm2).when(service).getSearchQueryForUrl(searchUrl2);
        TemplateUrlServiceFactory.setInstanceForTesting(service);

        WebContents webContents = mock(WebContents.class);
        doReturn(webContents).when(mTab1).getWebContents();
        when(mProfileJniMock.fromWebContents(eq(webContents))).thenReturn(mock(Profile.class));
        NavigationController navigationController = mock(NavigationController.class);
        doReturn(navigationController).when(webContents).getNavigationController();
        NavigationHistory navigationHistory = mock(NavigationHistory.class);
        doReturn(navigationHistory).when(navigationController).getNavigationHistory();
        doReturn(2).when(navigationHistory).getCurrentEntryIndex();
        NavigationEntry navigationEntry1 = mock(NavigationEntry.class);
        NavigationEntry navigationEntry0 = mock(NavigationEntry.class);
        doReturn(navigationEntry1).when(navigationHistory).getEntryAtIndex(1);
        doReturn(navigationEntry0).when(navigationHistory).getEntryAtIndex(0);
        doReturn(otherUrl).when(mTab1).getUrl();

        // No searches.
        doReturn(otherUrl).when(navigationEntry1).getOriginalUrl();
        doReturn(otherUrl).when(navigationEntry0).getOriginalUrl();
        Assert.assertNull(TabAttributeCache.findLastSearchTerm(mTab1));

        // Has SRP.
        doReturn(searchUrl).when(navigationEntry1).getOriginalUrl();
        doReturn(otherUrl).when(navigationEntry0).getOriginalUrl();
        Assert.assertEquals(searchTerm, TabAttributeCache.findLastSearchTerm(mTab1));

        // Has earlier SRP.
        doReturn(otherUrl).when(navigationEntry1).getOriginalUrl();
        doReturn(searchUrl2).when(navigationEntry0).getOriginalUrl();
        Assert.assertEquals(searchTerm2, TabAttributeCache.findLastSearchTerm(mTab1));

        // Latest one wins.
        doReturn(searchUrl).when(navigationEntry1).getOriginalUrl();
        doReturn(searchUrl2).when(navigationEntry0).getOriginalUrl();
        Assert.assertEquals(searchTerm, TabAttributeCache.findLastSearchTerm(mTab1));

        // Only care about previous ones.
        doReturn(1).when(navigationHistory).getCurrentEntryIndex();
        Assert.assertEquals(searchTerm2, TabAttributeCache.findLastSearchTerm(mTab1));

        // Skip if the SRP is showing.
        doReturn(2).when(navigationHistory).getCurrentEntryIndex();
        doReturn(searchUrl).when(mTab1).getUrl();
        Assert.assertNull(TabAttributeCache.findLastSearchTerm(mTab1));

        // Reset current SRP.
        doReturn(otherUrl).when(mTab1).getUrl();
        Assert.assertEquals(searchTerm, TabAttributeCache.findLastSearchTerm(mTab1));

        verify(navigationHistory, never()).getEntryAtIndex(eq(2));
    }

    @Test
    public void removeEscapedCodePoints() {
        Assert.assertEquals("", TabAttributeCache.removeEscapedCodePoints(""));
        Assert.assertEquals("", TabAttributeCache.removeEscapedCodePoints("%0a"));
        Assert.assertEquals("", TabAttributeCache.removeEscapedCodePoints("%0A"));
        Assert.assertEquals("AB", TabAttributeCache.removeEscapedCodePoints("A%FE%FFB"));
        Assert.assertEquals("a%0", TabAttributeCache.removeEscapedCodePoints("a%0"));
        Assert.assertEquals("%", TabAttributeCache.removeEscapedCodePoints("%%00"));
        Assert.assertEquals("%0G", TabAttributeCache.removeEscapedCodePoints("%0G"));
        Assert.assertEquals("abcc", TabAttributeCache.removeEscapedCodePoints("abc%abc"));
        Assert.assertEquals("a%a", TabAttributeCache.removeEscapedCodePoints("a%bc%a%bc"));
    }

    @Test
    public void onTabStateInitialized() {
        GURL url1 = JUnitTestGURLs.getGURL(JUnitTestGURLs.EXAMPLE_URL);
        doReturn(url1).when(mTab1).getUrl();
        String title1 = "title 1";
        doReturn(title1).when(mTab1).getTitle();
        int rootId1 = 1337;
        doReturn(rootId1).when(mCriticalPersistedTabData1).getRootId();
        long timestamp1 = 123456;
        doReturn(timestamp1).when(mCriticalPersistedTabData1).getTimestampMillis();

        GURL url2 = JUnitTestGURLs.getGURL(JUnitTestGURLs.URL_2);
        doReturn(url2).when(mTab2).getUrl();
        String title2 = "title 2";
        doReturn(title2).when(mTab2).getTitle();
        int rootId2 = 42;
        doReturn(rootId2).when(mCriticalPersistedTabData2).getRootId();

        String searchTerm = "chromium";
        LastSearchTermProvider lastSearchTermProvider = mock(LastSearchTermProvider.class);
        TabAttributeCache.setLastSearchTermMockForTesting(lastSearchTermProvider);
        doReturn(searchTerm).when(lastSearchTermProvider).getLastSearchTerm(mTab1);
        WebContents webContents = mock(WebContents.class);
        doReturn(webContents).when(mTab1).getWebContents();

        doReturn(mTab1).when(mTabModelFilter).getTabAt(0);
        doReturn(mTab2).when(mTabModelFilter).getTabAt(1);
        doReturn(2).when(mTabModelFilter).getCount();
        doReturn(mTab1).when(mTabModelSelector).getCurrentTab();

        Assert.assertNotEquals(url1, TabAttributeCache.getUrl(TAB1_ID));
        Assert.assertNotEquals(title1, TabAttributeCache.getTitle(TAB1_ID));
        Assert.assertNotEquals(rootId1, TabAttributeCache.getRootId(TAB1_ID));
        Assert.assertNotEquals(timestamp1, TabAttributeCache.getTimestampMillis(TAB1_ID));
        Assert.assertNotEquals(searchTerm, TabAttributeCache.getLastSearchTerm(TAB1_ID));

        Assert.assertNotEquals(url2, TabAttributeCache.getUrl(TAB2_ID));
        Assert.assertNotEquals(title2, TabAttributeCache.getTitle(TAB2_ID));
        Assert.assertNotEquals(rootId2, TabAttributeCache.getRootId(TAB2_ID));

        mTabModelSelectorObserverCaptor.getValue().onTabStateInitialized();

        Assert.assertEquals(url1, TabAttributeCache.getUrl(TAB1_ID));
        Assert.assertEquals(title1, TabAttributeCache.getTitle(TAB1_ID));
        Assert.assertEquals(rootId1, TabAttributeCache.getRootId(TAB1_ID));
        Assert.assertEquals(timestamp1, TabAttributeCache.getTimestampMillis(TAB1_ID));
        Assert.assertEquals(searchTerm, TabAttributeCache.getLastSearchTerm(TAB1_ID));

        Assert.assertEquals(url2, TabAttributeCache.getUrl(TAB2_ID));
        Assert.assertEquals(title2, TabAttributeCache.getTitle(TAB2_ID));
        Assert.assertEquals(rootId2, TabAttributeCache.getRootId(TAB2_ID));
    }

}
