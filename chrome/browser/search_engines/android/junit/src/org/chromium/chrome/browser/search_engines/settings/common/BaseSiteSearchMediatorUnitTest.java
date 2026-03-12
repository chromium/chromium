// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.settings.common;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
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
    @Mock private LargeIconBridgeJni mLargeIconBridgeJni;
    @Mock private TemplateUrlService mTemplateUrlService;
    @Mock private TemplateUrl mTemplateUrl;

    private Context mContext;
    private ModelList mModelList;
    private TestBaseMediator mMediator;
    private int mRefreshCount;

    private class TestBaseMediator extends BaseSiteSearchMediator {
        public TestBaseMediator(Context context, ModelList modelList, Profile profile) {
            super(context, modelList, profile);
            initializeTemplateUrlService();
        }

        @Override
        protected void refreshList() {
            mRefreshCount++;
        }

        @Override
        protected ListMenuDelegate createMenuDelegate(TemplateUrl url) {
            return null;
        }
    }

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        mModelList = new ModelList();
        mRefreshCount = 0;

        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);
        LargeIconBridgeJni.setInstanceForTesting(mLargeIconBridgeJni);

        doAnswer(
                        invocation -> {
                            Runnable runnable = invocation.getArgument(0);
                            runnable.run();
                            return null;
                        })
                .when(mTemplateUrlService)
                .runWhenLoaded(any());

        when(mTemplateUrl.getShortName()).thenReturn("Google");
        when(mTemplateUrl.getKeyword()).thenReturn("google.com");
        when(mTemplateUrl.getFaviconURL())
                .thenReturn(new GURL("https://www.google.com/favicon.ico"));

        mMediator = new TestBaseMediator(mContext, mModelList, mProfile);
    }

    @Test
    public void testInitialization() {
        verify(mTemplateUrlService).addObserver(mMediator);
        assertEquals(1, mRefreshCount);
    }

    @Test
    public void testDestroy() {
        mMediator.destroy();
        verify(mTemplateUrlService).removeObserver(mMediator);
    }

    @Test
    public void testOnTemplateURLServiceChanged() {
        int initialCount = mRefreshCount;
        mMediator.onTemplateURLServiceChanged();
        assertEquals(initialCount + 1, mRefreshCount);
    }

    @Test
    public void testCreateListItem() {
        ListItem item = mMediator.createListItem(mTemplateUrl);
        PropertyModel model = item.model;

        assertEquals(SiteSearchProperties.ViewType.SEARCH_ENGINE, item.type);
        assertEquals("Google", model.get(SiteSearchProperties.SITE_NAME));
        assertEquals("google.com", model.get(SiteSearchProperties.SITE_SHORTCUT));
        assertNull(model.get(SiteSearchProperties.MENU_DELEGATE));
        assertNotNull(model.get(SiteSearchProperties.ICON));
    }
}
