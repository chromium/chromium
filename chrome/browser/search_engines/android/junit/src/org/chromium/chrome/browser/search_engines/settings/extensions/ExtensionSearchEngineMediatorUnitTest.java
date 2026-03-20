// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.settings.extensions;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;

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

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.R;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.search_engines.settings.common.SiteSearchProperties;
import org.chromium.components.favicon.LargeIconBridgeJni;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlCategory;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.ui.listmenu.BasicListMenu;
import org.chromium.ui.listmenu.ListMenuDelegate;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.ModelListAdapter;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.Arrays;
import java.util.Collections;

/** Unit tests for {@link ExtensionSearchEngineMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ExtensionSearchEngineMediatorUnitTest {
    @Rule public MockitoRule mockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private TemplateUrlService mTemplateUrlService;
    @Mock private LargeIconBridgeJni mLargeIconBridgeJni;
    @Mock private TemplateUrl mTemplateUrl;
    @Mock private ModelList mModelList;

    private Context mContext;
    private ExtensionSearchEngineMediator mMediator;

    @Before
    public void setUp() {
        mContext = RuntimeEnvironment.application;
        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);
        LargeIconBridgeJni.setInstanceForTesting(mLargeIconBridgeJni);

        when(mTemplateUrlService.getTemplateUrlsByCategory(TemplateUrlCategory.EXTENSION))
                .thenReturn(Arrays.asList(mTemplateUrl));
        when(mTemplateUrl.getKeyword()).thenReturn("keyword");
        when(mTemplateUrl.getShortName()).thenReturn("extension");
        when(mTemplateUrl.getFaviconURL()).thenReturn(new GURL("http://example.com/favicon.ico"));

        doAnswer(
                        invocation -> {
                            Runnable runnable = invocation.getArgument(0);
                            runnable.run();
                            return null;
                        })
                .when(mTemplateUrlService)
                .runWhenLoaded(any());

        mMediator = new ExtensionSearchEngineMediator(mContext, mModelList, mProfile);

        Mockito.clearInvocations(mModelList, mTemplateUrlService);
    }

    @Test
    public void testRefreshList() {
        mMediator.onTemplateURLServiceChanged();

        verify(mModelList).clear();
        verify(mTemplateUrlService).getTemplateUrlsByCategory(TemplateUrlCategory.EXTENSION);
        ArgumentCaptor<ListItem> itemCaptor = ArgumentCaptor.forClass(ListItem.class);
        verify(mModelList).add(itemCaptor.capture());

        ListItem item = itemCaptor.getValue();
        assertEquals(SiteSearchProperties.ViewType.SEARCH_ENGINE, item.type);
        assertEquals("extension", item.model.get(SiteSearchProperties.SITE_NAME));
        assertEquals("keyword", item.model.get(SiteSearchProperties.SITE_SHORTCUT));
        assertNotNull(item.model.get(SiteSearchProperties.ICON));
    }

    @Test
    public void testRefreshList_Empty() {
        when(mTemplateUrlService.getTemplateUrlsByCategory(TemplateUrlCategory.EXTENSION))
                .thenReturn(Collections.emptyList());

        mMediator.onTemplateURLServiceChanged();

        verify(mModelList).clear();
        verify(mTemplateUrlService).getTemplateUrlsByCategory(TemplateUrlCategory.EXTENSION);
        verify(mModelList, Mockito.never()).add(any(ListItem.class));
    }

    @Test
    public void testMenuDelegate() {
        // Trigger the refreshList() to call mModelList.add().
        mMediator.onTemplateURLServiceChanged();

        ArgumentCaptor<ListItem> itemCaptor = ArgumentCaptor.forClass(ListItem.class);
        verify(mModelList).add(itemCaptor.capture());

        PropertyModel model = itemCaptor.getValue().model;
        ListMenuDelegate delegate = model.get(SiteSearchProperties.MENU_DELEGATE);

        assertNotNull(delegate);

        BasicListMenu listMenu = (BasicListMenu) delegate.getListMenu();
        ModelListAdapter adapter = listMenu.getContentAdapter();
        assertEquals(2, adapter.getCount());

        ListItem activateItem = (ListItem) adapter.getItem(0);
        assertEquals(
                R.string.site_search_extensions_menu_manage,
                activateItem.model.get(ListMenuItemProperties.TITLE_ID));

        ListItem makeDefaultItem = (ListItem) adapter.getItem(1);
        assertEquals(
                R.string.site_search_extensions_menu_disable,
                makeDefaultItem.model.get(ListMenuItemProperties.TITLE_ID));
    }

    @Test
    public void testDestroy() {
        mMediator.destroy();
        verify(mTemplateUrlService).removeObserver(mMediator);
    }
}
