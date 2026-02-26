// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.settings.custom_search_engine;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.res.Resources;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.R;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.search_engines.settings.common.SiteSearchProperties;
import org.chromium.components.favicon.LargeIconBridgeJni;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.ui.listmenu.BasicListMenu;
import org.chromium.ui.listmenu.ListMenuDelegate;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.ModelListAdapter;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;

@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class CustomSearchEngineListMediatorUnitTest {
    @Rule public MockitoRule mockitoRule = MockitoJUnit.rule();

    private Context mContext;
    private Resources mResources;

    @Mock private ModelList mModelList;
    @Mock private Profile mProfile;
    @Mock private TemplateUrlService mTemplateUrlService;
    @Mock private TemplateUrl mTemplateUrl;
    @Mock private LargeIconBridgeJni mLargeIconBridgeJni;
    @Mock private Callback<TemplateUrl> mOnEditSearchEngine;

    private CustomSearchEngineListMediator mMediator;

    @Before
    public void setUp() {
        mContext = RuntimeEnvironment.application;
        mResources = mContext.getResources();
        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);
        LargeIconBridgeJni.setInstanceForTesting(mLargeIconBridgeJni);

        List<TemplateUrl> urls = new ArrayList<>();
        urls.add(mTemplateUrl);
        when(mTemplateUrlService.getTemplateUrls()).thenReturn(urls);
        when(mTemplateUrl.getKeyword()).thenReturn("keyword");
        when(mTemplateUrl.getShortName()).thenReturn("My Search Engine");

        doAnswer(
                        invocation -> {
                            Runnable runnable = invocation.getArgument(0);
                            runnable.run();
                            return null;
                        })
                .when(mTemplateUrlService)
                .runWhenLoaded(any());

        mMediator =
                new CustomSearchEngineListMediator(
                        mContext, mModelList, mProfile, mOnEditSearchEngine);
    }

    @Test
    public void testRefreshList() {
        Mockito.clearInvocations(mModelList, mTemplateUrlService);

        mMediator.onTemplateURLServiceChanged();

        verify(mModelList).clear();
        verify(mTemplateUrlService).getTemplateUrls();
        verify(mModelList).add(any(ListItem.class));
    }

    @Test
    public void testMenuDelegate() {
        ArgumentCaptor<ListItem> itemCaptor = ArgumentCaptor.forClass(ListItem.class);
        verify(mModelList).add(itemCaptor.capture());

        PropertyModel model = itemCaptor.getValue().model;
        ListMenuDelegate delegate = model.get(SiteSearchProperties.MENU_DELEGATE);

        assertNotNull(delegate);

        BasicListMenu listMenu = (BasicListMenu) delegate.getListMenu();
        ModelListAdapter adapter = listMenu.getContentAdapter();
        assertEquals(3, adapter.getCount());

        ListItem item0 = (ListItem) adapter.getItem(0);
        assertEquals(
                R.string.site_search_list_menu_edit,
                item0.model.get(ListMenuItemProperties.TITLE_ID));

        ListItem item1 = (ListItem) adapter.getItem(1);
        assertEquals(
                R.string.site_search_list_menu_make_default,
                item1.model.get(ListMenuItemProperties.TITLE_ID));

        ListItem item2 = (ListItem) adapter.getItem(2);
        assertEquals(
                R.string.site_search_list_menu_delete,
                item2.model.get(ListMenuItemProperties.TITLE_ID));
    }

    @Test
    public void testEditClicked() {
        String keyword = "keyword";
        when(mTemplateUrl.getKeyword()).thenReturn(keyword);

        mMediator.onMenuItemClicked(R.string.site_search_list_menu_edit, mTemplateUrl);

        verify(mOnEditSearchEngine).onResult(mTemplateUrl);
    }

    @Test
    public void testMakeDefaultClicked() {
        String keyword = "keyword";
        when(mTemplateUrl.getKeyword()).thenReturn(keyword);

        mMediator.onMenuItemClicked(R.string.site_search_list_menu_make_default, mTemplateUrl);

        verify(mTemplateUrlService).setSearchEngine(keyword);
    }

    @Test
    public void testDeleteClicked() {
        String keyword = "keyword";
        when(mTemplateUrl.getKeyword()).thenReturn(keyword);

        mMediator.onMenuItemClicked(R.string.site_search_list_menu_delete, mTemplateUrl);

        verify(mTemplateUrlService).removeSearchEngine(keyword);
    }

    @Test
    public void testMenuDelegate_DefaultSearchEngine() {
        when(mTemplateUrlService.getDefaultSearchEngineTemplateUrl()).thenReturn(mTemplateUrl);

        Mockito.clearInvocations(mModelList, mTemplateUrlService);
        mMediator.onTemplateURLServiceChanged();

        ArgumentCaptor<ListItem> itemCaptor = ArgumentCaptor.forClass(ListItem.class);
        verify(mModelList).add(itemCaptor.capture());

        PropertyModel model = itemCaptor.getValue().model;
        ListMenuDelegate delegate = model.get(SiteSearchProperties.MENU_DELEGATE);

        assertNull(delegate);
    }
}
