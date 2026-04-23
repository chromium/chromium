// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.settings.common;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyMap;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.withSettings;

import android.content.Context;
import android.view.ContextThemeWrapper;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.R;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.favicon.LargeIconBridgeJni;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.ui.listmenu.ListMenuDelegate;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

/** Unit tests for {@link BaseSiteSearchMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BaseSiteSearchMediatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private TemplateUrlService mTemplateUrlService;
    @Mock private LargeIconBridgeJni mLargeIconBridgeJni;
    @Mock private TemplateUrl mTemplateUrl;
    @Mock private ListMenuDelegate mMenuDelegate;

    private Context mContext;
    private ModelList mModelList;
    private BaseSiteSearchMediator mMediator;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);

        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);
        LargeIconBridgeJni.setInstanceForTesting(mLargeIconBridgeJni);
        mModelList = new ModelList();

        mMediator =
                mock(
                        BaseSiteSearchMediator.class,
                        withSettings()
                                .useConstructor(mContext, mModelList, mProfile)
                                .defaultAnswer(Mockito.CALLS_REAL_METHODS));
    }

    @Test
    public void testInitializeTemplateUrlService() {
        mMediator.initializeTemplateUrlService();

        verify(mTemplateUrlService).addObserver(mMediator);

        ArgumentCaptor<Runnable> runnableCaptor = ArgumentCaptor.forClass(Runnable.class);
        verify(mTemplateUrlService).runWhenLoaded(runnableCaptor.capture());

        runnableCaptor.getValue().run();
        verify(mMediator).refreshList();
    }

    @Test
    public void testDestroy() {
        mMediator.destroy();

        verify(mTemplateUrlService).removeObserver(mMediator);
    }

    @Test
    public void testOnTemplateURLServiceChanged() {
        mMediator.onTemplateURLServiceChanged();

        verify(mMediator).refreshList();
    }

    @Test
    public void testCreateListItem() {
        doReturn("SearchSite").when(mTemplateUrl).getShortName();
        doReturn("site.com").when(mTemplateUrl).getKeyword();
        doReturn(mMenuDelegate).when(mMediator).createMenuDelegate(mTemplateUrl);
        doNothing().when(mMediator).fetchFavicon(eq(mTemplateUrl), any(PropertyModel.class));

        ListItem item = mMediator.createListItem(mTemplateUrl);

        verify(mMediator).createMenuDelegate(mTemplateUrl);
        verify(mMediator).fetchFavicon(eq(mTemplateUrl), eq(item.model));

        assertEquals(SiteSearchProperties.ViewType.SEARCH_ENGINE, item.type);
        assertEquals("SearchSite", item.model.get(SiteSearchProperties.SITE_NAME));
        assertEquals("site.com", item.model.get(SiteSearchProperties.SITE_SHORTCUT));
        assertEquals(mMenuDelegate, item.model.get(SiteSearchProperties.MENU_DELEGATE));
        assertNotNull(item.model.get(SiteSearchProperties.ICON));
    }

    @Test
    public void testFetchFavicon_WithValidUrl() {
        GURL templateUrlHost = new GURL("https://example.com");
        doReturn(templateUrlHost.getSpec()).when(mTemplateUrl).getURL();
        PropertyModel model = new PropertyModel(SiteSearchProperties.ALL_KEYS);
        doNothing()
                .when(mMediator)
                .executeIconUpdate(any(), any(), any(), any(), any(), any(), any());

        mMediator.fetchFavicon(mTemplateUrl, model);

        verify(mMediator)
                .executeIconUpdate(
                        any(Context.class),
                        eq(model),
                        eq(SiteSearchProperties.ICON),
                        eq(mTemplateUrl),
                        eq(templateUrlHost),
                        any(LargeIconBridge.class),
                        anyMap());
    }

    @Test
    public void testUpdatePositions() {
        PropertyModel model1 = new PropertyModel(SiteSearchProperties.ALL_KEYS);
        PropertyModel model2 = new PropertyModel(SiteSearchProperties.ALL_KEYS);
        PropertyModel model3 = new PropertyModel(SiteSearchProperties.ALL_KEYS);

        mModelList.add(new ListItem(SiteSearchProperties.ViewType.SEARCH_ENGINE, model1));
        mMediator.updatePositions(mModelList);
        assertEquals(
                SiteSearchProperties.ItemPosition.SINGLE,
                model1.get(SiteSearchProperties.POSITION));

        mModelList.add(new ListItem(SiteSearchProperties.ViewType.SEARCH_ENGINE, model2));
        mMediator.updatePositions(mModelList);
        assertEquals(
                SiteSearchProperties.ItemPosition.TOP, model1.get(SiteSearchProperties.POSITION));
        assertEquals(
                SiteSearchProperties.ItemPosition.BOTTOM,
                model2.get(SiteSearchProperties.POSITION));

        mModelList.add(new ListItem(SiteSearchProperties.ViewType.SEARCH_ENGINE, model3));
        mMediator.updatePositions(mModelList);
        assertEquals(
                SiteSearchProperties.ItemPosition.TOP, model1.get(SiteSearchProperties.POSITION));
        assertEquals(
                SiteSearchProperties.ItemPosition.MIDDLE,
                model2.get(SiteSearchProperties.POSITION));
        assertEquals(
                SiteSearchProperties.ItemPosition.BOTTOM,
                model3.get(SiteSearchProperties.POSITION));
    }
}
