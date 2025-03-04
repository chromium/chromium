// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.notNull;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.drawable.Drawable;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper.FaviconImageCallback;
import org.chromium.chrome.browser.ui.favicon.FaviconHelperJni;
import org.chromium.ui.base.TestActivity;
import org.chromium.url.JUnitTestGURLs;

/** Tests for {@link TabGroupListCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabGroupListCoordinatorUnitTest {
    public static final long FAKE_NATIVE_PTR = 1L;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock FaviconHelper.Natives mFaviconHelperJniMock;
    @Mock Profile mProfile;
    @Mock Callback<Drawable> mCallback;
    @Mock Bitmap mBitmap;

    @Captor private ArgumentCaptor<FaviconImageCallback> mFaviconImageCallbackCaptor;

    private Activity mActivity;
    private TabListFaviconProvider mTabListFaviconProvider;

    @Before
    public void setUp() {
        FaviconHelperJni.setInstanceForTesting(mFaviconHelperJniMock);
        when(mFaviconHelperJniMock.init()).thenReturn(FAKE_NATIVE_PTR);

        mActivityScenarioRule.getScenario().onActivity(this::onActivity);
    }

    private void onActivity(Activity activity) {
        mActivity = activity;
        mTabListFaviconProvider =
                new TabListFaviconProvider(
                        activity,
                        /* isTabStrip= */ false,
                        R.dimen.default_favicon_corner_radius,
                        /* tabWebContentsFaviconDelegate= */ null);
    }

    @After
    public void tearDown() {
        mTabListFaviconProvider.destroy();
    }

    @Test
    public void testForeignFavicon() {
        FaviconResolver resolver =
                TabGroupListFaviconResolverFactory.build(
                        mActivity, mProfile, mTabListFaviconProvider);
        resolver.resolve(JUnitTestGURLs.URL_1, mCallback);
        verify(mFaviconHelperJniMock)
                .getForeignFaviconImageForURL(
                        anyLong(), any(), any(), anyInt(), mFaviconImageCallbackCaptor.capture());

        mFaviconImageCallbackCaptor.getValue().onFaviconAvailable(mBitmap, JUnitTestGURLs.URL_2);
        verify(mCallback).onResult(notNull());
    }

    @Test
    public void testFallbackFavicon() {
        FaviconResolver resolver =
                TabGroupListFaviconResolverFactory.build(
                        mActivity, mProfile, mTabListFaviconProvider);
        resolver.resolve(JUnitTestGURLs.URL_1, mCallback);
        verify(mFaviconHelperJniMock)
                .getForeignFaviconImageForURL(
                        anyLong(), any(), any(), anyInt(), mFaviconImageCallbackCaptor.capture());

        mFaviconImageCallbackCaptor.getValue().onFaviconAvailable(null, JUnitTestGURLs.URL_2);
        verify(mCallback).onResult(notNull());
    }

    @Test
    public void testInternalFavicon() {
        FaviconResolver resolver =
                TabGroupListFaviconResolverFactory.build(
                        mActivity, mProfile, mTabListFaviconProvider);
        resolver.resolve(JUnitTestGURLs.NTP_NATIVE_URL, mCallback);
        verify(mCallback).onResult(notNull());
    }
}
