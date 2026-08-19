// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_ui;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.graphics.Bitmap;
import android.view.View;
import android.view.ViewGroup.MarginLayoutParams;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.FeatureOverrides;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab_ui.TabContentManager.TabFinder;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.chrome.browser.ui.vertical_tabs.VerticalTabUtils;

import java.util.List;

/** Unit tests for {@link TabContentManager}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabContentManagerUnitTest {
    private static final long NATIVE_PTR = 12345L;

    @Rule public MockitoRule mockitoRule = MockitoJUnit.rule();

    @Mock private BrowserControlsStateProvider mBrowserControlsStateProvider;
    @Mock private TabFinder mTabFinder;
    @Mock private TabWindowManager mTabWindowManager;
    @Mock private View mViewToDraw;
    @Mock private TabContentManager.Natives mTabContentManagerJni;

    private Context mContext;
    private TabContentManager mTabContentManager;

    @Before
    public void setUp() {
        TabContentManagerJni.setInstanceForTesting(mTabContentManagerJni);
        when(mTabContentManagerJni.init(any(), anyInt(), anyInt(), anyInt(), anyBoolean()))
                .thenReturn(NATIVE_PTR);

        mContext = ContextUtils.getApplicationContext();
        when(mBrowserControlsStateProvider.getTopVisibleContentOffset()).thenReturn(0f);

        MarginLayoutParams params = new MarginLayoutParams(200, 200);
        params.leftMargin = 100;
        when(mViewToDraw.getLayoutParams()).thenReturn(params);
        when(mViewToDraw.getContext()).thenReturn(mContext);
        when(mViewToDraw.getMeasuredWidth()).thenReturn(200);
        when(mViewToDraw.getMeasuredHeight()).thenReturn(200);

        mTabContentManager =
                new TabContentManager(
                        mContext,
                        mBrowserControlsStateProvider,
                        /* snapshotsEnabled= */ true,
                        mTabFinder,
                        mTabWindowManager);
    }

    @After
    public void tearDown() {
        VerticalTabUtils.resetSharedPrefsForTesting();
    }

    @Test
    public void testReadbackNativeView_verticalTabsDisabled_includesLeftMargin() {
        VerticalTabUtils.setVerticalTabsEnabled(false);

        Bitmap bitmap = mTabContentManager.readbackNativeView(mViewToDraw, 1.0f, null);
        assertNotNull(bitmap);
        assertEquals(300, bitmap.getWidth());
    }

    @Test
    @Config(qualifiers = "sw600dp")
    @EnableFeatures(ChromeFeatureList.ANDROID_VERTICAL_TABS)
    public void testReadbackNativeView_verticalTabsEnabled_removesLeftMargin() {
        FeatureOverrides.enable(ChromeFeatureList.ANDROID_VERTICAL_TABS);
        VerticalTabUtils.setVerticalTabsEnabled(true);

        Bitmap bitmap = mTabContentManager.readbackNativeView(mViewToDraw, 1.0f, null);
        assertNotNull(bitmap);
        assertEquals(200, bitmap.getWidth());
    }

    @Test
    public void testUpdateVisibleIds_withNative() {
        mTabContentManager.initWithNative();

        List<Integer> priority = List.of(1, 2);
        mTabContentManager.updateVisibleIds(priority, /* primaryTabId= */ 1);

        verify(mTabContentManagerJni).updateVisibleIds(eq(NATIVE_PTR), eq(new int[] {1, 2}), eq(1));
    }

    @Test
    public void testUpdateVisibleIds_withoutNative() {
        List<Integer> priority = List.of(1, 2);
        mTabContentManager.updateVisibleIds(priority, /* primaryTabId= */ 1);

        verify(mTabContentManagerJni, never()).updateVisibleIds(anyLong(), any(), anyInt());
    }

    @Test
    public void testUpdateVisibleIds_exceedsMaxCacheSize() {
        mTabContentManager.initWithNative();

        List<Integer> priority = List.of(1, 2, 3, 4);
        mTabContentManager.updateVisibleIds(priority, /* primaryTabId= */ 1);

        // Truncated to mFullResThumbnailsMaxSize (which is 2).
        verify(mTabContentManagerJni).updateVisibleIds(eq(NATIVE_PTR), eq(new int[] {1, 2}), eq(1));
    }

    @Test
    public void testUpdateVisibleIds_emptyList() {
        mTabContentManager.initWithNative();

        List<Integer> priority = List.of();
        mTabContentManager.updateVisibleIds(priority, /* primaryTabId= */ 2);

        verify(mTabContentManagerJni).updateVisibleIds(eq(NATIVE_PTR), eq(new int[] {}), eq(2));
    }
}
