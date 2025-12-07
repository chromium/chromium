// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.content.res.ColorStateList;
import android.graphics.Bitmap;
import android.graphics.drawable.Drawable;
import android.view.LayoutInflater;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.SavedTabGroupTab;
import org.chromium.ui.base.TestActivity;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link TabGroupFaviconCluster}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabGroupFaviconClusterUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private FaviconResolver mFaviconResolver;
    @Mock private Drawable mDrawable;
    @Mock private Callback<Bitmap> mBitmapCallback;
    @Captor private ArgumentCaptor<Callback<Drawable>> mDrawableCallbackCaptor;

    private Activity mActivity;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);
    }

    private void newTab(SavedTabGroup savedTabGroup, GURL url) {
        SavedTabGroupTab tab = new SavedTabGroupTab();
        tab.url = url;
        savedTabGroup.savedTabs.add(tab);
    }

    @Test
    public void testCreateBitmapFrom() {
        SavedTabGroup savedTabGroup = new SavedTabGroup();
        newTab(savedTabGroup, JUnitTestGURLs.URL_1);
        newTab(savedTabGroup, JUnitTestGURLs.URL_2);

        TabGroupFaviconCluster.createBitmapFrom(
                savedTabGroup, mActivity, mFaviconResolver, mBitmapCallback);
        verify(mBitmapCallback, times(0)).onResult(any());

        verify(mFaviconResolver, times(2)).resolve(any(), any());
        verify(mFaviconResolver)
                .resolve(eq(JUnitTestGURLs.URL_1), mDrawableCallbackCaptor.capture());
        mDrawableCallbackCaptor.getValue().onResult(mDrawable);
        verify(mBitmapCallback, times(0)).onResult(any());

        verify(mFaviconResolver)
                .resolve(eq(JUnitTestGURLs.URL_2), mDrawableCallbackCaptor.capture());
        mDrawableCallbackCaptor.getValue().onResult(mDrawable);
        verify(mBitmapCallback).onResult(any());
    }

    @Test
    public void testCreateBitmapFrom_NoTabs() {
        SavedTabGroup savedTabGroup = new SavedTabGroup();

        TabGroupFaviconCluster.createBitmapFrom(
                savedTabGroup, mActivity, mFaviconResolver, mBitmapCallback);
        ShadowLooper.idleMainLooper();
        verify(mBitmapCallback).onResult(any());
    }

    @Test
    public void testCreateBitmapFrom_FourTabs() {
        SavedTabGroup savedTabGroup = new SavedTabGroup();
        newTab(savedTabGroup, JUnitTestGURLs.URL_1);
        newTab(savedTabGroup, JUnitTestGURLs.URL_2);
        newTab(savedTabGroup, JUnitTestGURLs.URL_3);
        newTab(savedTabGroup, JUnitTestGURLs.BLUE_1);

        TabGroupFaviconCluster.createBitmapFrom(
                savedTabGroup, mActivity, mFaviconResolver, mBitmapCallback);
        verify(mFaviconResolver, times(4)).resolve(any(), any());
    }

    @Test
    public void testCreateBitmapFrom_FiveTabs() {
        SavedTabGroup savedTabGroup = new SavedTabGroup();
        newTab(savedTabGroup, JUnitTestGURLs.URL_1);
        newTab(savedTabGroup, JUnitTestGURLs.URL_2);
        newTab(savedTabGroup, JUnitTestGURLs.URL_3);
        newTab(savedTabGroup, JUnitTestGURLs.BLUE_1);
        newTab(savedTabGroup, JUnitTestGURLs.BLUE_2);

        TabGroupFaviconCluster.createBitmapFrom(
                savedTabGroup, mActivity, mFaviconResolver, mBitmapCallback);
        verify(mFaviconResolver, times(3)).resolve(any(), any());
    }

    @Test
    public void testSetContainmentEnabled() {
        TabGroupFaviconCluster cluster =
                (TabGroupFaviconCluster)
                        LayoutInflater.from(mActivity)
                                .inflate(R.layout.tab_group_favicon_cluster, null, false);
        cluster.setContainmentEnabled(true);
        ColorStateList tint = cluster.getBackgroundTintList();
        assertEquals(SemanticColorUtils.getColorSurfaceBright(mActivity), tint.getDefaultColor());
    }

    @Test
    public void testSetContainmentDefault() {
        TabGroupFaviconCluster cluster =
                (TabGroupFaviconCluster)
                        LayoutInflater.from(mActivity)
                                .inflate(R.layout.tab_group_favicon_cluster, null, false);
        cluster.setContainmentEnabled(false);
        ColorStateList tint = cluster.getBackgroundTintList();
        assertEquals(
                SemanticColorUtils.getColorSurfaceContainer(mActivity), tint.getDefaultColor());
    }
}
