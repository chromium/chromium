// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.settings.inactive_shortcut;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.AimEligibilityServiceFactory;
import org.chromium.chrome.browser.search_engines.AimEligibilityServiceFactoryJni;
import org.chromium.chrome.browser.search_engines.R;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.search_engines.settings.common.SiteSearchProperties;
import org.chromium.components.favicon.LargeIconBridgeJni;
import org.chromium.components.omnibox.OmniboxFeatureList;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.search_engines.StarterPackId;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlCategory;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.ui.listmenu.BasicListMenu;
import org.chromium.ui.listmenu.ListMenuDelegate;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.ModelListAdapter;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link InactiveShortcutMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures(OmniboxFeatureList.STARTER_PACK_EXPANSION)
public class InactiveShortcutMediatorUnitTest {
    @Rule public MockitoRule mockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private TemplateUrlService mTemplateUrlService;
    @Mock private LargeIconBridgeJni mLargeIconBridgeJni;
    @Mock private Callback<TemplateUrl> mOnRemoveSearchEngine;
    @Mock private AimEligibilityServiceFactory.Natives mAimEligibilityNativesMock;
    @Mock private UserPrefs.Natives mUserPrefsJniMock;
    @Mock private PrefService mPrefServiceMock;

    private Context mContext;
    private InactiveShortcutMediator mMediator;
    private ModelList mModelList;

    @Before
    public void setUp() {
        mContext = RuntimeEnvironment.application;
        UserPrefsJni.setInstanceForTesting(mUserPrefsJniMock);
        doReturn(mPrefServiceMock).when(mUserPrefsJniMock).get(any(Profile.class));
        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);
        LargeIconBridgeJni.setInstanceForTesting(mLargeIconBridgeJni);
        AimEligibilityServiceFactoryJni.setInstanceForTesting(mAimEligibilityNativesMock);

        mModelList = new ModelList();

        doAnswer(
                        invocation -> {
                            Runnable runnable = invocation.getArgument(0);
                            runnable.run();
                            return null;
                        })
                .when(mTemplateUrlService)
                .runWhenLoaded(any());
    }

    private TemplateUrl createMockTemplateUrl(String keyword, String shortName) {
        TemplateUrl templateUrl = org.mockito.Mockito.mock(TemplateUrl.class);
        when(templateUrl.getKeyword()).thenReturn(keyword);
        when(templateUrl.getShortName()).thenReturn(shortName);
        when(templateUrl.getFaviconURL()).thenReturn(new GURL("http://example.com/favicon.ico"));
        return templateUrl;
    }

    private void setUpTemplateUrlService(int searchEngineCount) {
        List<TemplateUrl> urls = new ArrayList<>();
        for (int i = 0; i < searchEngineCount; i++) {
            urls.add(createMockTemplateUrl("keyword" + i, "test site " + i));
        }
        when(mTemplateUrlService.getTemplateUrlsByCategory(
                        TemplateUrlCategory.INACTIVE_SITE_SEARCH))
                .thenReturn(urls);
    }

    private void initMediator() {
        mMediator =
                new InactiveShortcutMediator(mContext, mModelList, mProfile, mOnRemoveSearchEngine);
    }

    @Test
    public void testSiteSearchList_underMaxRows() {
        setUpTemplateUrlService(/* searchEngineCount= */ 3);
        initMediator();

        verifyCollapsedModelListView(/* searchEngineCount= */ 3);
    }

    @Test
    public void testSiteSearchList_exactMaxRows() {
        setUpTemplateUrlService(/* searchEngineCount= */ 5);
        initMediator();

        verifyCollapsedModelListView(/* searchEngineCount= */ 5);
    }

    @Test
    public void testSiteSearchList_overMaxRows() {
        setUpTemplateUrlService(/* searchEngineCount= */ 7);
        initMediator();

        verifyCollapsedModelListView(/* searchEngineCount= */ 7);

        // Click "More" to expand
        PropertyModel moreButtonModel = mModelList.get(5).model;
        assertFalse(moreButtonModel.get(SiteSearchProperties.IS_EXPANDED));
        moreButtonModel.get(SiteSearchProperties.ON_CLICK).onClick(null);

        assertTrue(moreButtonModel.get(SiteSearchProperties.IS_EXPANDED));
        // 5 site search items + 1 more button + 2 expanded site search items
        assertEquals(8, mModelList.size());

        for (int i = 6; i < 8; i++) {
            ListItem item = mModelList.get(i);
            assertEquals(SiteSearchProperties.ViewType.SEARCH_ENGINE, item.type);
            assertEquals("test site " + (i - 1), item.model.get(SiteSearchProperties.SITE_NAME));
            assertEquals("keyword" + (i - 1), item.model.get(SiteSearchProperties.SITE_SHORTCUT));
        }

        // Click "More" to collapse
        moreButtonModel.get(SiteSearchProperties.ON_CLICK).onClick(null);

        assertFalse(moreButtonModel.get(SiteSearchProperties.IS_EXPANDED));
        verifyCollapsedModelListView(/* searchEngineCount= */ 7);
    }

    @Test
    public void testSiteSearchList_templateUrlServiceChanged() {
        setUpTemplateUrlService(7);
        initMediator();

        verifyCollapsedModelListView(/* searchEngineCount= */ 7);

        // Expand the list
        PropertyModel moreButtonModel = mModelList.get(5).model;
        moreButtonModel.get(SiteSearchProperties.ON_CLICK).onClick(null);
        assertTrue(mMediator.isExpandedForTesting());

        mMediator.onTemplateURLServiceChanged();

        assertTrue(mMediator.isExpandedForTesting());
        assertEquals(8, mModelList.size());
        for (int i = 6; i < 8; i++) {
            ListItem item = mModelList.get(i);
            assertEquals(SiteSearchProperties.ViewType.SEARCH_ENGINE, item.type);
            assertEquals("test site " + (i - 1), item.model.get(SiteSearchProperties.SITE_NAME));
            assertEquals("keyword" + (i - 1), item.model.get(SiteSearchProperties.SITE_SHORTCUT));
        }
    }

    @Test
    public void testMenuDelegate_NormalUrl() {
        TemplateUrl templateUrl = createMockTemplateUrl("keyword", "shortName");
        when(mTemplateUrlService.getTemplateUrlsByCategory(
                        TemplateUrlCategory.INACTIVE_SITE_SEARCH))
                .thenReturn(List.of(templateUrl));
        when(templateUrl.getStarterPackId()).thenReturn(StarterPackId.NONE);

        initMediator();

        PropertyModel model = mModelList.get(0).model;
        ListMenuDelegate delegate = model.get(SiteSearchProperties.MENU_DELEGATE);
        assertNotNull(delegate);

        BasicListMenu listMenu = (BasicListMenu) delegate.getListMenu();
        ModelListAdapter adapter = listMenu.getContentAdapter();
        assertEquals(3, adapter.getCount());

        ListItem activateItem = (ListItem) adapter.getItem(0);
        assertEquals(
                R.string.site_search_list_menu_activate,
                activateItem.model.get(ListMenuItemProperties.TITLE_ID));

        ListItem makeDefaultItem = (ListItem) adapter.getItem(1);
        assertEquals(
                R.string.site_search_list_menu_make_default,
                makeDefaultItem.model.get(ListMenuItemProperties.TITLE_ID));

        ListItem deleteItem = (ListItem) adapter.getItem(2);
        assertEquals(
                R.string.site_search_list_menu_delete,
                deleteItem.model.get(ListMenuItemProperties.TITLE_ID));
    }

    @Test
    public void testMenuDelegate_StarterPackUrl() {
        TemplateUrl templateUrl = createMockTemplateUrl("keyword", "shortName");
        when(mTemplateUrlService.getTemplateUrlsByCategory(
                        TemplateUrlCategory.INACTIVE_SITE_SEARCH))
                .thenReturn(List.of(templateUrl));
        when(templateUrl.getStarterPackId()).thenReturn(StarterPackId.GEMINI);

        initMediator();

        PropertyModel model = mModelList.get(0).model;
        ListMenuDelegate delegate = model.get(SiteSearchProperties.MENU_DELEGATE);
        assertNotNull(delegate);

        BasicListMenu listMenu = (BasicListMenu) delegate.getListMenu();
        ModelListAdapter adapter = listMenu.getContentAdapter();
        assertEquals(1, adapter.getCount());

        ListItem activateItem = (ListItem) adapter.getItem(0);
        assertEquals(
                R.string.site_search_list_menu_activate,
                activateItem.model.get(ListMenuItemProperties.TITLE_ID));
    }

    @Test
    public void testActivateClicked() {
        initMediator();
        TemplateUrl templateUrl = createMockTemplateUrl("keyword", "shortName");

        mMediator.onMenuItemClicked(R.string.site_search_list_menu_activate, templateUrl);

        verify(mTemplateUrlService).activateSearchEngine("keyword");
    }

    @Test
    public void testMakeDefaultClicked() {
        initMediator();
        TemplateUrl templateUrl = createMockTemplateUrl("keyword", "shortName");

        mMediator.onMenuItemClicked(R.string.site_search_list_menu_make_default, templateUrl);

        verify(mTemplateUrlService).setSearchEngine("keyword");
    }

    @Test
    public void testDeleteClicked() {
        initMediator();
        TemplateUrl templateUrl = createMockTemplateUrl("keyword", "shortName");

        mMediator.onMenuItemClicked(R.string.site_search_list_menu_delete, templateUrl);

        verify(mOnRemoveSearchEngine).onResult(templateUrl);
    }

    private void verifyCollapsedModelListView(int searchEngineCount) {
        int defaultViewCount = 5;
        boolean expectMoreButton = searchEngineCount > defaultViewCount;
        int visibleSearchEngines = Math.min(searchEngineCount, defaultViewCount);

        int expectedTotalCount = visibleSearchEngines;
        if (expectMoreButton) {
            expectedTotalCount += 1;
        }
        assertEquals(expectedTotalCount, mModelList.size());

        for (int i = 0; i < visibleSearchEngines; i++) {
            ListItem item = mModelList.get(i);
            assertEquals(SiteSearchProperties.ViewType.SEARCH_ENGINE, item.type);
            assertEquals("test site " + i, item.model.get(SiteSearchProperties.SITE_NAME));
            assertEquals("keyword" + i, item.model.get(SiteSearchProperties.SITE_SHORTCUT));
        }

        if (expectMoreButton) {
            assertEquals(
                    SiteSearchProperties.ViewType.MORE, mModelList.get(visibleSearchEngines).type);
        }
    }
}
