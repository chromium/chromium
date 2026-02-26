// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.settings.custom_site_search;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;
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

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.search_engines.settings.common.SiteSearchProperties;
import org.chromium.components.favicon.LargeIconBridgeJni;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlCategory;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link CustomSiteSearchMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class CustomSiteSearchMediatorUnitTest {
    @Rule public MockitoRule mockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private TemplateUrlService mTemplateUrlService;
    @Mock private LargeIconBridgeJni mLargeIconBridgeJni;
    @Mock private Runnable mOnAddSearchEngine;

    private Context mContext;
    private CustomSiteSearchMediator mMediator;
    private ModelList mModelList;

    @Before
    public void setUp() {
        mContext = RuntimeEnvironment.application;
        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);
        LargeIconBridgeJni.setInstanceForTesting(mLargeIconBridgeJni);

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
        when(mTemplateUrlService.getTemplateUrlsByCategory(TemplateUrlCategory.ACTIVE_SITE_SEARCH))
                .thenReturn(urls);
    }

    @Test
    public void testSiteSearchList_underMaxRows() {
        setUpTemplateUrlService(/* searchEngineCount= */ 3);
        mMediator =
                new CustomSiteSearchMediator(mContext, mModelList, mProfile, mOnAddSearchEngine);

        verifyCollapsedModelListView(/* searchEngineCount= */ 3);
    }

    @Test
    public void testSiteSearchList_exactMaxRows() {
        setUpTemplateUrlService(/* searchEngineCount= */ 5);
        mMediator =
                new CustomSiteSearchMediator(mContext, mModelList, mProfile, mOnAddSearchEngine);

        verifyCollapsedModelListView(/* searchEngineCount= */ 5);
    }

    @Test
    public void testSiteSearchList_overMaxRows() {
        setUpTemplateUrlService(/* searchEngineCount= */ 7);
        mMediator =
                new CustomSiteSearchMediator(mContext, mModelList, mProfile, mOnAddSearchEngine);

        verifyCollapsedModelListView(/* searchEngineCount= */ 7);

        // Click "More" to expand
        PropertyModel moreButtonModel = mModelList.get(6).model;
        assertFalse(moreButtonModel.get(SiteSearchProperties.IS_EXPANDED));
        moreButtonModel.get(SiteSearchProperties.ON_CLICK).onClick(null);

        assertTrue(moreButtonModel.get(SiteSearchProperties.IS_EXPANDED));
        // 5 site search items + 1 add button + 1 more button + 2 expanded site search items
        assertEquals(9, mModelList.size());

        for (int i = 7; i < 9; i++) {
            ListItem item = mModelList.get(i);
            assertEquals(SiteSearchProperties.ViewType.SEARCH_ENGINE, item.type);
            assertEquals("test site " + (i - 2), item.model.get(SiteSearchProperties.SITE_NAME));
            assertEquals("keyword" + (i - 2), item.model.get(SiteSearchProperties.SITE_SHORTCUT));
        }

        // Click "More" to collapse
        moreButtonModel.get(SiteSearchProperties.ON_CLICK).onClick(null);

        assertFalse(moreButtonModel.get(SiteSearchProperties.IS_EXPANDED));
        verifyCollapsedModelListView(/* searchEngineCount= */ 7);
    }

    @Test
    public void testSiteSearchList_templateUrlServiceChanged() {
        setUpTemplateUrlService(7);
        mMediator =
                new CustomSiteSearchMediator(mContext, mModelList, mProfile, mOnAddSearchEngine);

        verifyCollapsedModelListView(/* searchEngineCount= */ 7);

        // Expand the list
        PropertyModel moreButtonModel = mModelList.get(6).model;
        moreButtonModel.get(SiteSearchProperties.ON_CLICK).onClick(null);
        assertTrue(mMediator.isExpandedForTesting());

        mMediator.onTemplateURLServiceChanged();

        assertFalse(mMediator.isExpandedForTesting());
        verifyCollapsedModelListView(/* searchEngineCount= */ 7);
    }

    private void verifyCollapsedModelListView(int searchEngineCount) {
        int defaultViewCount = 5;
        boolean expectMoreButton = searchEngineCount > defaultViewCount;
        int visibleSearchEngines = Math.min(searchEngineCount, defaultViewCount);

        int expectedTotalCount = visibleSearchEngines + 1;
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

        assertEquals(SiteSearchProperties.ViewType.ADD, mModelList.get(visibleSearchEngines).type);

        if (expectMoreButton) {
            assertEquals(
                    SiteSearchProperties.ViewType.MORE,
                    mModelList.get(visibleSearchEngines + 1).type);
        }
    }
}
