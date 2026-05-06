// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.enterprise.util.DataProtectionBridge;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactoryJni;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.content_public.browser.WebContents;

/** Unit tests for {@link WebSearchDelegate}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures(ChromeFeatureList.DATA_CONTROLS_SEARCH_WITH)
public class WebSearchDelegateUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabModelSelector mTabModelSelector;
    @Mock private Tab mTab;
    @Mock private WebContents mWebContents;
    @Mock private Profile mProfile;
    @Mock private Tracker mTracker;
    @Mock private TemplateUrlService mTemplateUrlService;
    @Mock private DataProtectionBridge.Natives mDataProtectionBridgeMock;
    @Mock private TemplateUrlServiceFactory.Natives mTemplateUrlServiceFactoryMock;

    private ActivityTabProvider mActivityTabProvider;
    private SettableMonotonicObservableSupplier<TabModelSelector> mTabModelSelectorSupplier;
    private WebSearchDelegate mDelegate;

    @Before
    public void setUp() {
        DataProtectionBridge.setInstanceForTesting(mDataProtectionBridgeMock);
        TemplateUrlServiceFactoryJni.setInstanceForTesting(mTemplateUrlServiceFactoryMock);
        TrackerFactory.setTrackerForTests(mTracker);

        mActivityTabProvider = new ActivityTabProvider();
        mActivityTabProvider.setForTesting(mTab);

        mTabModelSelectorSupplier = ObservableSuppliers.createMonotonic();
        mTabModelSelectorSupplier.set(mTabModelSelector);

        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mTab.getProfile()).thenReturn(mProfile);
        when(mTemplateUrlServiceFactoryMock.getTemplateUrlService(mProfile))
                .thenReturn(mTemplateUrlService);
        when(mTemplateUrlService.getUrlForSearchQuery(anyString()))
                .thenReturn("https://search.com");

        mDelegate = new WebSearchDelegate(mActivityTabProvider, mTabModelSelectorSupplier);
    }

    @Test
    public void testSearch_AllowedByPolicy() {
        doAnswer(
                        invocation -> {
                            ((Runnable) invocation.getArgument(2)).run();
                            return null;
                        })
                .when(mDataProtectionBridgeMock)
                .shouldAllowSearchWith(anyInt(), any(), any());

        mDelegate.performSearch("query");

        verify(mTabModelSelector).openNewTab(any(), anyInt(), any(), anyBoolean());
        verify(mTracker).notifyEvent(EventConstants.WEB_SEARCH_PERFORMED);
    }

    @Test
    public void testSearch_NotAllowedByPolicy() {
        // Do not invoke the callback to simulate the policy blocking the action.
        mDelegate.performSearch("query");

        verify(mTabModelSelector, never()).openNewTab(any(), anyInt(), any(), anyBoolean());
        verify(mTracker, never()).notifyEvent(anyString());
    }
}
