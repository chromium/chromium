// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.settings.custom_site_search;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.components.favicon.LargeIconBridgeJni;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlCategory;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.url.GURL;

import java.util.Arrays;

/** Unit tests for {@link CustomSiteSearchMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class CustomSiteSearchMediatorUnitTest {
    @Rule public MockitoRule mockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private TemplateUrlService mTemplateUrlService;
    @Mock private LargeIconBridgeJni mLargeIconBridgeJni;
    @Mock private TemplateUrl mTemplateUrl;
    @Mock private ModelList mModelList;

    private Context mContext;
    private CustomSiteSearchMediator mMediator;

    @Before
    public void setUp() {
        mContext = RuntimeEnvironment.application;
        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);
        LargeIconBridgeJni.setInstanceForTesting(mLargeIconBridgeJni);

        when(mTemplateUrlService.getTemplateUrlsByCategory(TemplateUrlCategory.ACTIVE_SITE_SEARCH))
                .thenReturn(Arrays.asList(mTemplateUrl));
        when(mTemplateUrl.getKeyword()).thenReturn("keyword");
        when(mTemplateUrl.getShortName()).thenReturn("test site");
        when(mTemplateUrl.getFaviconURL()).thenReturn(new GURL("http://example.com/favicon.ico"));

        doAnswer(
                        invocation -> {
                            Runnable runnable = invocation.getArgument(0);
                            runnable.run();
                            return null;
                        })
                .when(mTemplateUrlService)
                .runWhenLoaded(any());

        mMediator = new CustomSiteSearchMediator(mContext, mModelList, mProfile);
    }

    @Test
    public void testRefreshList() {
        Mockito.clearInvocations(mModelList, mTemplateUrlService);

        mMediator.onTemplateURLServiceChanged();

        verify(mModelList).clear();
        verify(mTemplateUrlService)
                .getTemplateUrlsByCategory(TemplateUrlCategory.ACTIVE_SITE_SEARCH);
        verify(mModelList).add(any(ListItem.class));
    }
}
