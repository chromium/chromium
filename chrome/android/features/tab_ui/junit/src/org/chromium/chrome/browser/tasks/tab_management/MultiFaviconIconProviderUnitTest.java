// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.notNull;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.Drawable;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tasks.tab_management.suggestions.TabContext;
import org.chromium.chrome.browser.tasks.tab_management.suggestions.TabSuggestion;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper.FaviconImageCallback;
import org.chromium.chrome.browser.ui.favicon.FaviconHelperJni;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link MultiFaviconIconProvider}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class MultiFaviconIconProviderUnitTest {
    Context mContext;

    @Rule public JniMocker jniMocker = new JniMocker();
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock FaviconHelper.Natives mFaviconHelperJniMock;
    @Mock Profile mProfile;
    @Mock Tab mTab;
    @Mock Callback<Drawable> mCallback;
    @Mock Drawable mDrawable;

    @Captor ArgumentCaptor<FaviconImageCallback> mLeftDrawableCaptor;
    @Captor ArgumentCaptor<FaviconImageCallback> mCentreDrawableCaptor;
    @Captor ArgumentCaptor<FaviconImageCallback> mRightDrawableCaptor;

    private MultiFaviconIconProvider mProvider;
    private FaviconHelper mFaviconHelper;
    private ShadowLooper mShadowLooper;

    @Before
    public void setUp() {
        mContext = RuntimeEnvironment.application;
        ContextUtils.initApplicationContextForTests(mContext);

        when(mFaviconHelperJniMock.init()).thenReturn(1L);
        jniMocker.mock(FaviconHelperJni.TEST_HOOKS, mFaviconHelperJniMock);
        mFaviconHelper = new FaviconHelper();
        mShadowLooper = ShadowLooper.shadowMainLooper();

        List<TabContext.TabInfo> suggestedTabInfo = new ArrayList<>();
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.URL_1);
        when(mTab.getOriginalUrl()).thenReturn(JUnitTestGURLs.URL_1);
        suggestedTabInfo.add(TabContext.TabInfo.createFromTab(mTab));
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.URL_2);
        when(mTab.getOriginalUrl()).thenReturn(JUnitTestGURLs.URL_2);
        suggestedTabInfo.add(TabContext.TabInfo.createFromTab(mTab));
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.URL_3);
        when(mTab.getOriginalUrl()).thenReturn(JUnitTestGURLs.URL_3);
        suggestedTabInfo.add(TabContext.TabInfo.createFromTab(mTab));
        TabSuggestion tabSuggestion =
                new TabSuggestion(suggestedTabInfo, TabSuggestion.TabSuggestionAction.CLOSE, "");

        mProvider = new MultiFaviconIconProvider(mContext, tabSuggestion, mProfile, mFaviconHelper);
    }

    @Test
    public void testFetchIconDrawable() {
        Bitmap bitmap = Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888);

        mProvider.fetchIconDrawable(mCallback);
        mShadowLooper.idle();

        verify(mFaviconHelperJniMock)
                .getLocalFaviconImageForURL(
                        eq(1L),
                        any(),
                        eq(JUnitTestGURLs.URL_1),
                        anyInt(),
                        mLeftDrawableCaptor.capture());
        verify(mFaviconHelperJniMock)
                .getLocalFaviconImageForURL(
                        eq(1L),
                        any(),
                        eq(JUnitTestGURLs.URL_2),
                        anyInt(),
                        mCentreDrawableCaptor.capture());
        verify(mFaviconHelperJniMock)
                .getLocalFaviconImageForURL(
                        eq(1L),
                        any(),
                        eq(JUnitTestGURLs.URL_3),
                        anyInt(),
                        mRightDrawableCaptor.capture());

        mLeftDrawableCaptor.getValue().onFaviconAvailable(bitmap, JUnitTestGURLs.URL_1);
        mShadowLooper.idle();
        verify(mCallback, never()).onResult(any());
        mCentreDrawableCaptor.getValue().onFaviconAvailable(bitmap, JUnitTestGURLs.URL_2);
        mShadowLooper.idle();
        verify(mCallback, never()).onResult(any());
        mRightDrawableCaptor.getValue().onFaviconAvailable(bitmap, JUnitTestGURLs.URL_3);
        mShadowLooper.idle();
        verify(mCallback).onResult(notNull());
    }
}
