// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.findinpage;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.transition.ChangeBounds;
import android.transition.Transition;
import android.util.ArrayMap;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup.MarginLayoutParams;
import android.widget.FrameLayout;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.AnchorSide;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.HeightType;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiSpecs;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiSpecs.SideUiSize;
import org.chromium.chrome.browser.ui.side_ui.SideUiStateProvider;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.base.WindowAndroid;

import java.util.Map;

/** Unit tests for {@link FindToolbarTablet}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, qualifiers = "sw600dp")
public class FindToolbarTabletUnitTest {
    private static final int SIDE_UI_WIDTH_PX = 250;
    private static final int POPUP_WIDTH_PX = 100;
    private static final int POPUP_HEIGHT_PX = 50;
    private static final int HTS_TOOLBAR_BOTTOM_PX = 96;
    private static final int VT_TOOLBAR_BOTTOM_PX = 56;

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private SideUiStateProvider mSideUiStateProvider;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private Tab mTab;
    @Mock private WebContents mWebContents;
    @Mock private View mAnchorView;
    @Mock private WindowAndroid mWindowAndroid;

    private FindToolbarTablet mFindToolbarTablet;
    private int mBaseMarginEndPx;
    private int mYInsetPx;

    @Before
    public void setUp() {
        LocalizationUtils.setRtlForTesting(false);

        Activity activity = Robolectric.buildActivity(Activity.class).get();
        activity.setTheme(R.style.Theme_BrowserUI_DayNight);

        mFindToolbarTablet =
                (FindToolbarTablet)
                        LayoutInflater.from(activity).inflate(R.layout.find_toolbar, null);
        mBaseMarginEndPx =
                activity.getResources()
                        .getDimensionPixelOffset(R.dimen.find_in_page_popup_margin_end);
        mYInsetPx = (int) (activity.getResources().getDisplayMetrics().density * 8.f);
        MarginLayoutParams layoutParams =
                new FrameLayout.LayoutParams(POPUP_WIDTH_PX, POPUP_HEIGHT_PX);
        layoutParams.setMarginEnd(mBaseMarginEndPx);
        mFindToolbarTablet.setLayoutParams(layoutParams);

        when(mTabModelSelector.getCurrentTab()).thenReturn(mTab);
        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mTab.isNativePage()).thenReturn(false);
        mFindToolbarTablet.setTabModelSelector(mTabModelSelector);
        mFindToolbarTablet.setWindowAndroid(mWindowAndroid);

        SideUiSpecs emptySpecs = createSideUiSpecs(0, HeightType.NOT_APPLICABLE);
        when(mSideUiStateProvider.getCurrentSideUiSpecs()).thenReturn(emptySpecs);
    }

    @After
    public void tearDown() {
        LocalizationUtils.setRtlForTesting(false);
    }

    // Horizontal Margin (Margin End) Tests

    @Test
    public void testOnSideUiSpecsChanged_rightAnchorWebContentsHeightType_ltr() {
        SideUiSpecs specs = createSideUiSpecs(SIDE_UI_WIDTH_PX, HeightType.WEB_CONTENTS);

        mFindToolbarTablet.onSideUiSpecsChanged(specs);

        MarginLayoutParams lp = (MarginLayoutParams) mFindToolbarTablet.getLayoutParams();
        assertEquals(mBaseMarginEndPx + SIDE_UI_WIDTH_PX, lp.getMarginEnd());
    }

    @Test
    public void testOnSideUiSpecsChanged_leftAnchorWebContentsHeightType_rtl() {
        LocalizationUtils.setRtlForTesting(true);
        Map<@AnchorSide Integer, SideUiSize> map = new ArrayMap<>();
        map.put(AnchorSide.LEFT, new SideUiSize(SIDE_UI_WIDTH_PX, HeightType.WEB_CONTENTS));
        SideUiSpecs specs = new SideUiSpecs(map);

        mFindToolbarTablet.onSideUiSpecsChanged(specs);

        MarginLayoutParams lp = (MarginLayoutParams) mFindToolbarTablet.getLayoutParams();
        assertEquals(mBaseMarginEndPx + SIDE_UI_WIDTH_PX, lp.getMarginEnd());
    }

    @Test
    public void testOnSideUiSpecsChanged_bothSidesWebContentsHeightType_ltr() {
        Map<@AnchorSide Integer, SideUiSize> map = new ArrayMap<>();
        map.put(AnchorSide.LEFT, new SideUiSize(200, HeightType.WEB_CONTENTS));
        map.put(AnchorSide.RIGHT, new SideUiSize(SIDE_UI_WIDTH_PX, HeightType.WEB_CONTENTS));
        SideUiSpecs specs = new SideUiSpecs(map);

        mFindToolbarTablet.onSideUiSpecsChanged(specs);

        MarginLayoutParams lp = (MarginLayoutParams) mFindToolbarTablet.getLayoutParams();
        // In LTR, only the right-anchored side UI contributes to marginEnd.
        assertEquals(mBaseMarginEndPx + SIDE_UI_WIDTH_PX, lp.getMarginEnd());
    }

    @Test
    public void testOnSideUiSpecsChanged_rightAnchorWebContentsHeightType_rtl() {
        LocalizationUtils.setRtlForTesting(true);
        SideUiSpecs specs = createSideUiSpecs(SIDE_UI_WIDTH_PX, HeightType.WEB_CONTENTS);

        mFindToolbarTablet.onSideUiSpecsChanged(specs);

        MarginLayoutParams lp = (MarginLayoutParams) mFindToolbarTablet.getLayoutParams();
        // In RTL, the end edge is on the left, so a right-anchored panel does not add marginEnd.
        assertEquals(mBaseMarginEndPx, lp.getMarginEnd());
    }

    @Test
    public void testOnSideUiSpecsChanged_toolbarHeightType() {
        SideUiSpecs specs = createSideUiSpecs(SIDE_UI_WIDTH_PX, HeightType.TOOLBAR);

        mFindToolbarTablet.onSideUiSpecsChanged(specs);

        // Toolbar height type should NOT add margin to FindToolbarTablet because
        // ToolbarControlContainer already offsets itself.
        MarginLayoutParams lp = (MarginLayoutParams) mFindToolbarTablet.getLayoutParams();
        assertEquals(mBaseMarginEndPx, lp.getMarginEnd());
    }

    @Test
    public void testOnSideUiSpecsChanged_closed() {
        SideUiSpecs specs = createSideUiSpecs(0, HeightType.NOT_APPLICABLE);

        mFindToolbarTablet.onSideUiSpecsChanged(specs);

        MarginLayoutParams lp = (MarginLayoutParams) mFindToolbarTablet.getLayoutParams();
        assertEquals(mBaseMarginEndPx, lp.getMarginEnd());
    }

    // Vertical Margin (Top Margin) Tests

    @Test
    public void testTopMargin_withHorizontalTabStrip() {
        when(mAnchorView.getBottom()).thenReturn(HTS_TOOLBAR_BOTTOM_PX);
        mFindToolbarTablet.setAnchorView(mAnchorView);
        mFindToolbarTablet.setVisibility(View.GONE);
        mFindToolbarTablet.handleActivate();

        MarginLayoutParams lp = (MarginLayoutParams) mFindToolbarTablet.getLayoutParams();
        assertEquals(HTS_TOOLBAR_BOTTOM_PX - mYInsetPx, lp.topMargin);
    }

    @Test
    public void testTopMargin_withVerticalTabs() {
        when(mAnchorView.getBottom()).thenReturn(VT_TOOLBAR_BOTTOM_PX);
        mFindToolbarTablet.setAnchorView(mAnchorView);
        mFindToolbarTablet.setVisibility(View.GONE);
        mFindToolbarTablet.handleActivate();

        MarginLayoutParams lp = (MarginLayoutParams) mFindToolbarTablet.getLayoutParams();
        assertEquals(VT_TOOLBAR_BOTTOM_PX - mYInsetPx, lp.topMargin);
    }

    @Test
    public void testTopMarginAndSideUi_withVerticalTabs() {
        // Vertical Tabs suppresses HTS (height = 56px) and opens on LEFT anchor side in LTR.
        when(mAnchorView.getBottom()).thenReturn(VT_TOOLBAR_BOTTOM_PX);
        mFindToolbarTablet.setAnchorView(mAnchorView);

        Map<@AnchorSide Integer, SideUiSize> map = new ArrayMap<>();
        map.put(AnchorSide.LEFT, new SideUiSize(SIDE_UI_WIDTH_PX, HeightType.WEB_CONTENTS));
        SideUiSpecs vtSpecs = new SideUiSpecs(map);

        mFindToolbarTablet.onSideUiSpecsChanged(vtSpecs);
        mFindToolbarTablet.setVisibility(View.GONE);
        mFindToolbarTablet.handleActivate();

        MarginLayoutParams lp = (MarginLayoutParams) mFindToolbarTablet.getLayoutParams();
        assertEquals(VT_TOOLBAR_BOTTOM_PX - mYInsetPx, lp.topMargin);
        // VT on the left does not add margin to the end (right) in LTR.
        assertEquals(mBaseMarginEndPx, lp.getMarginEnd());
    }

    @Test
    public void testTopMargin_nullAnchorView() {
        mFindToolbarTablet.setAnchorView(null);
        mFindToolbarTablet.setVisibility(View.GONE);
        mFindToolbarTablet.handleActivate();

        MarginLayoutParams lp = (MarginLayoutParams) mFindToolbarTablet.getLayoutParams();
        assertEquals(0, lp.topMargin);
    }

    // Transition Animation Tests

    @Test
    public void testOnPreSideUiSpecsChange_visibleReturnsChangeBounds() {
        mFindToolbarTablet.setVisibility(View.VISIBLE);
        SideUiSpecs specs = createSideUiSpecs(SIDE_UI_WIDTH_PX, HeightType.WEB_CONTENTS);

        Transition transition = mFindToolbarTablet.onPreSideUiSpecsChange(specs);

        assertNotNull(transition);
        assertEquals(ChangeBounds.class, transition.getClass());
    }

    @Test
    public void testOnPreSideUiSpecsChange_goneReturnsNull() {
        mFindToolbarTablet.setVisibility(View.GONE);
        SideUiSpecs specs = createSideUiSpecs(SIDE_UI_WIDTH_PX, HeightType.WEB_CONTENTS);

        Transition transition = mFindToolbarTablet.onPreSideUiSpecsChange(specs);

        assertNull(transition);
    }

    // Observer & Lifecycle Tests

    @Test
    public void testSetSideUiStateProvider_attachesAndDetachesObserver() {
        mFindToolbarTablet.setSideUiStateProvider(mSideUiStateProvider);
        verify(mSideUiStateProvider).addObserver(mFindToolbarTablet);

        mFindToolbarTablet.destroy();
        verify(mSideUiStateProvider).removeObserver(mFindToolbarTablet);
    }

    private SideUiSpecs createSideUiSpecs(int rightWidth, @HeightType int rightHeightType) {
        Map<@AnchorSide Integer, SideUiSize> map = new ArrayMap<>();
        map.put(AnchorSide.RIGHT, new SideUiSize(rightWidth, rightHeightType));
        return new SideUiSpecs(map);
    }
}
