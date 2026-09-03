// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ActivityState;
import org.chromium.base.UnownedUserDataHost;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.ui.base.WindowAndroid;

import java.lang.ref.WeakReference;

/** Unit tests for {@link TabBottomSheetUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabBottomSheetUtilsUnitTest {
    private static final float EPSILON = 0.001f;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule().silent();

    @Mock private Context mContext;
    @Mock private Resources mResources;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private TabBottomSheetManager mManager;
    @Mock private CoBrowseViewFactory mFactory;

    private UnownedUserDataHost mUnownedUserDataHost;

    @Before
    public void setUp() {
        mUnownedUserDataHost = new UnownedUserDataHost();
        when(mContext.getResources()).thenReturn(mResources);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_BOTTOM_SHEET)
    public void testIsTabBottomSheetEnabled_Enabled() {
        assertTrue(TabBottomSheetUtils.isTabBottomSheetEnabled());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.TAB_BOTTOM_SHEET)
    public void testIsTabBottomSheetEnabled_Disabled() {
        assertFalse(TabBottomSheetUtils.isTabBottomSheetEnabled());
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.TAB_BOTTOM_SHEET,
        ChromeFeatureList.TAB_BOTTOM_SHEET_RESIZE_WEBVIEW
    })
    public void testCanResizeWebView_Enabled() {
        assertTrue(TabBottomSheetUtils.canResizeWebView());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_BOTTOM_SHEET)
    @DisableFeatures(ChromeFeatureList.TAB_BOTTOM_SHEET_RESIZE_WEBVIEW)
    public void testCanResizeWebView_ResizeDisabled() {
        assertFalse(TabBottomSheetUtils.canResizeWebView());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.TAB_BOTTOM_SHEET)
    @EnableFeatures(ChromeFeatureList.TAB_BOTTOM_SHEET_RESIZE_WEBVIEW)
    public void testCanResizeWebView_BottomSheetDisabled() {
        assertFalse(TabBottomSheetUtils.canResizeWebView());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.TAB_BOTTOM_SHEET_FULL_HEIGHT)
    public void testGetFullHeightRatio_Default() {
        assertEquals(
                TabBottomSheetUtils.FULL_HEIGHT_RATIO,
                TabBottomSheetUtils.getFullHeightRatio(),
                EPSILON);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_BOTTOM_SHEET_FULL_HEIGHT + ":full_height_ratio/0.85")
    public void testGetFullHeightRatio_WithCustomFinchParam() {
        assertEquals(0.85f, TabBottomSheetUtils.getFullHeightRatio(), EPSILON);
    }

    @Test
    public void testGetDefaultHeightRatio_Landscape() {
        Configuration landscapeConfig = new Configuration();
        landscapeConfig.orientation = Configuration.ORIENTATION_LANDSCAPE;
        when(mResources.getConfiguration()).thenReturn(landscapeConfig);

        assertEquals(
                TabBottomSheetUtils.SMALL_SCREEN_HEIGHT_RATIO,
                TabBottomSheetUtils.getDefaultHeightRatio(mContext, /* isKeyboardShowing= */ false),
                EPSILON);
        assertEquals(
                TabBottomSheetUtils.SMALL_SCREEN_HEIGHT_RATIO,
                TabBottomSheetUtils.getDefaultHeightRatio(mContext, /* isKeyboardShowing= */ true),
                EPSILON);
    }

    @Test
    public void testGetDefaultHeightRatio_KeyboardShowing() {
        Configuration portraitConfig = new Configuration();
        portraitConfig.orientation = Configuration.ORIENTATION_PORTRAIT;
        when(mResources.getConfiguration()).thenReturn(portraitConfig);

        assertEquals(
                TabBottomSheetUtils.SMALL_SCREEN_HEIGHT_RATIO,
                TabBottomSheetUtils.getDefaultHeightRatio(mContext, /* isKeyboardShowing= */ true),
                EPSILON);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.TAB_BOTTOM_SHEET_HALF_HEIGHT)
    public void testGetDefaultHeightRatio_PortraitFallback() {
        Configuration portraitConfig = new Configuration();
        portraitConfig.orientation = Configuration.ORIENTATION_PORTRAIT;
        when(mResources.getConfiguration()).thenReturn(portraitConfig);

        assertEquals(
                TabBottomSheetUtils.DEFAULT_HEIGHT_RATIO,
                TabBottomSheetUtils.getDefaultHeightRatio(mContext, /* isKeyboardShowing= */ false),
                EPSILON);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_BOTTOM_SHEET_HALF_HEIGHT + ":half_height_ratio/0.6")
    public void testGetDefaultHeightRatio_CustomFinchParam() {
        Configuration portraitConfig = new Configuration();
        portraitConfig.orientation = Configuration.ORIENTATION_PORTRAIT;
        when(mResources.getConfiguration()).thenReturn(portraitConfig);

        assertEquals(
                0.6f,
                TabBottomSheetUtils.getDefaultHeightRatio(mContext, /* isKeyboardShowing= */ false),
                EPSILON);
    }

    @Test
    public void testManagerLifecycle_AttachGetDetach() {
        assertNull(TabBottomSheetUtils.getManagerFromWindow(null));

        when(mWindowAndroid.getUnownedUserDataHost()).thenReturn(mUnownedUserDataHost);
        assertNull(TabBottomSheetUtils.getManagerFromWindow(mWindowAndroid));

        TabBottomSheetUtils.attachManagerToWindow(mWindowAndroid, mManager);
        assertEquals(mManager, TabBottomSheetUtils.getManagerFromWindow(mWindowAndroid));

        TabBottomSheetUtils.detachManagerFromWindow(mWindowAndroid);
        assertNull(TabBottomSheetUtils.getManagerFromWindow(mWindowAndroid));
    }

    @Test
    public void testFactoryLifecycle_AttachGetDetach() {
        when(mWindowAndroid.getUnownedUserDataHost()).thenReturn(mUnownedUserDataHost);
        assertNull(TabBottomSheetUtils.getFactoryFromWindow(mWindowAndroid));

        TabBottomSheetUtils.attachFactoryToWindow(mWindowAndroid, mFactory);
        assertEquals(mFactory, TabBottomSheetUtils.getFactoryFromWindow(mWindowAndroid));

        TabBottomSheetUtils.detachFactoryFromWindow(mWindowAndroid);
        assertNull(TabBottomSheetUtils.getFactoryFromWindow(mWindowAndroid));
    }

    @Test
    public void testIsActivityFinishingOrDestroyed() {
        assertTrue(TabBottomSheetUtils.isActivityFinishingOrDestroyed(null));

        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<>(null));
        assertTrue(TabBottomSheetUtils.isActivityFinishingOrDestroyed(mWindowAndroid));

        Activity activity = mock(Activity.class);
        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<>(activity));

        when(activity.isFinishing()).thenReturn(true);
        when(activity.isDestroyed()).thenReturn(false);
        assertTrue(TabBottomSheetUtils.isActivityFinishingOrDestroyed(mWindowAndroid));

        when(activity.isFinishing()).thenReturn(false);
        when(activity.isDestroyed()).thenReturn(true);
        assertTrue(TabBottomSheetUtils.isActivityFinishingOrDestroyed(mWindowAndroid));

        when(activity.isFinishing()).thenReturn(false);
        when(activity.isDestroyed()).thenReturn(false);
        assertFalse(TabBottomSheetUtils.isActivityFinishingOrDestroyed(mWindowAndroid));
    }

    @Test
    public void testIsActivityInactive() {
        assertTrue(TabBottomSheetUtils.isActivityInactive(null));

        when(mWindowAndroid.getActivityState()).thenReturn(ActivityState.STOPPED);
        assertTrue(TabBottomSheetUtils.isActivityInactive(mWindowAndroid));

        when(mWindowAndroid.getActivityState()).thenReturn(ActivityState.DESTROYED);
        assertTrue(TabBottomSheetUtils.isActivityInactive(mWindowAndroid));

        when(mWindowAndroid.getActivityState()).thenReturn(ActivityState.RESUMED);
        assertFalse(TabBottomSheetUtils.isActivityInactive(mWindowAndroid));

        when(mWindowAndroid.getActivityState()).thenReturn(ActivityState.CREATED);
        assertFalse(TabBottomSheetUtils.isActivityInactive(mWindowAndroid));
    }
}
