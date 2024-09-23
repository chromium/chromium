// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.lenient;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import android.app.Activity;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupCreationDialogManager;
import org.chromium.chrome.browser.ui.native_page.NativePageHost;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.mojom.WindowOpenDisposition;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.Arrays;
import java.util.List;

/** Unit tests for {@link SuggestionsNavigationDelegate}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SuggestionsNavigationDelegateUnitTest {
    private static final boolean IS_INCOGNITO_SELECTED = false;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Activity mActivity;
    @Mock private Profile mProfile;
    @Mock private NativePageHost mHost;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabGroupCreationDialogManager mTabGroupCreationDialogManager;
    @Mock private Tab mTab;

    @Captor private ArgumentCaptor<LoadUrlParams> mLoadUrlParamsCaptor;

    private SuggestionsNavigationDelegate mSuggestionsNavigationDelegate;

    @Before
    public void setUp() {
        mSuggestionsNavigationDelegate =
                new SuggestionsNavigationDelegate(
                        mActivity,
                        mProfile,
                        mHost,
                        mTabModelSelector,
                        mTabGroupCreationDialogManager,
                        mTab);

        lenient().when(mTabModelSelector.isIncognitoSelected()).thenReturn(IS_INCOGNITO_SELECTED);
    }

    @Test
    @SmallTest
    public void testNavigateToSuggestionUrl() {
        // WindowOpenDisposition.CURRENT_TAB:
        mSuggestionsNavigationDelegate.navigateToSuggestionUrl(
                WindowOpenDisposition.CURRENT_TAB, JUnitTestGURLs.URL_1.getSpec(), false);
        verify(mHost).loadUrl(mLoadUrlParamsCaptor.capture(), eq(IS_INCOGNITO_SELECTED));
        Assert.assertEquals(
                JUnitTestGURLs.URL_1.getSpec(),
                mLoadUrlParamsCaptor.getAllValues().get(0).getUrl());

        // WindowOpenDisposition.NEW_FOREGROUND_TAB:
        mSuggestionsNavigationDelegate.navigateToSuggestionUrl(
                WindowOpenDisposition.NEW_FOREGROUND_TAB, JUnitTestGURLs.URL_2.getSpec(), false);
        verify(mTabModelSelector)
                .openNewTab(
                        mLoadUrlParamsCaptor.capture(),
                        eq(TabLaunchType.FROM_LONGPRESS_FOREGROUND),
                        eq(mTab),
                        eq(false));
        Assert.assertEquals(
                JUnitTestGURLs.URL_2.getSpec(),
                mLoadUrlParamsCaptor.getAllValues().get(1).getUrl());

        // WindowOpenDisposition.NEW_BACKGROUND_TAB:
        mSuggestionsNavigationDelegate.navigateToSuggestionUrl(
                WindowOpenDisposition.NEW_BACKGROUND_TAB, JUnitTestGURLs.URL_3.getSpec(), false);
        verify(mTabModelSelector)
                .openNewTab(
                        mLoadUrlParamsCaptor.capture(),
                        eq(TabLaunchType.FROM_LONGPRESS_BACKGROUND),
                        eq(mTab),
                        eq(false));
        Assert.assertEquals(
                JUnitTestGURLs.URL_3.getSpec(),
                mLoadUrlParamsCaptor.getAllValues().get(2).getUrl());

        // WindowOpenDisposition.OFF_THE_RECORD:
        mSuggestionsNavigationDelegate.navigateToSuggestionUrl(
                WindowOpenDisposition.OFF_THE_RECORD, JUnitTestGURLs.RED_1.getSpec(), false);
        verify(mHost).loadUrl(mLoadUrlParamsCaptor.capture(), eq(true));
        Assert.assertEquals(
                JUnitTestGURLs.RED_1.getSpec(),
                mLoadUrlParamsCaptor.getAllValues().get(3).getUrl());

        // Skip WindowOpenDisposition.{NEW_WINDOW,SAVE_TO_DISK}, which are difficult to mock.
    }

    @Test
    @SmallTest
    public void testMaybeSelectTabWithUrl_NoMatch() {
        TabModel tabModel =
                createTabModelFromList(Arrays.asList(JUnitTestGURLs.URL_1, JUnitTestGURLs.URL_2));
        doReturn(tabModel).when(mTabModelSelector).getModel(/* incognito= */ false);

        // Mismatched URL.
        Assert.assertFalse(
                mSuggestionsNavigationDelegate.maybeSelectTabWithUrl(JUnitTestGURLs.RED_1));

        // By default, require identical match.
        Assert.assertFalse(
                mSuggestionsNavigationDelegate.maybeSelectTabWithUrl(
                        new GURL(JUnitTestGURLs.URL_1.getSpec() + "/path")));
        Assert.assertFalse(
                mSuggestionsNavigationDelegate.maybeSelectTabWithUrl(
                        new GURL(JUnitTestGURLs.URL_1.getSpec() + "?query=1")));
        Assert.assertFalse(
                mSuggestionsNavigationDelegate.maybeSelectTabWithUrl(
                        new GURL(JUnitTestGURLs.URL_1.getSpec() + "#ref")));
    }

    @Test
    @SmallTest
    public void testMaybeSelectTabWithUrl_Match() {
        TabModel tabModel =
                createTabModelFromList(Arrays.asList(JUnitTestGURLs.URL_1, JUnitTestGURLs.URL_2));
        doReturn(tabModel).when(mTabModelSelector).getModel(/* incognito= */ false);

        Assert.assertTrue(
                mSuggestionsNavigationDelegate.maybeSelectTabWithUrl(JUnitTestGURLs.URL_2));
        verify(tabModel).setIndex(eq(1), anyInt());
        verify(tabModel).closeTabs(argThat(params -> params.tabs.get(0) == mTab));
    }

    private TabModel createTabModelFromList(List<GURL> urlList) {
        TabModel tabModel = mock(TabModel.class);
        for (int i = 0; i < urlList.size(); ++i) {
            Tab tab = mock(Tab.class);
            lenient().doReturn(urlList.get(i)).when(tab).getUrl();
            lenient().doReturn(tab).when(tabModel).getTabAt(i);
        }
        lenient().doReturn(urlList.size()).when(tabModel).getCount();
        return tabModel;
    }
}
