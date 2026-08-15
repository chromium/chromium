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
import android.view.View;
import android.view.ViewGroup.MarginLayoutParams;
import android.widget.FrameLayout;

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
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.AnchorSide;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.HeightType;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiSpecs;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiSpecs.SideUiSize;
import org.chromium.chrome.browser.ui.side_ui.SideUiStateProvider;

import java.util.Map;

/** Unit tests for {@link FindToolbarTablet}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class FindToolbarTabletUnitTest {
    private static final int BASE_MARGIN_END_PX = 16;
    private static final int SIDE_UI_WIDTH_PX = 250;
    private static final int POPUP_WIDTH_PX = 100;
    private static final int POPUP_HEIGHT_PX = 50;

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private SideUiStateProvider mSideUiStateProvider;

    private FindToolbarTablet mFindToolbarTablet;

    @Before
    public void setUp() {
        Activity activity = Robolectric.buildActivity(Activity.class).get();
        activity.setTheme(R.style.Theme_BrowserUI_DayNight);

        mFindToolbarTablet = new FindToolbarTablet(activity, null);
        MarginLayoutParams layoutParams =
                new FrameLayout.LayoutParams(POPUP_WIDTH_PX, POPUP_HEIGHT_PX);
        layoutParams.setMarginEnd(BASE_MARGIN_END_PX);
        mFindToolbarTablet.setLayoutParams(layoutParams);

        SideUiSpecs emptySpecs = createSideUiSpecs(0, HeightType.NOT_APPLICABLE);
        when(mSideUiStateProvider.getCurrentSideUiSpecs()).thenReturn(emptySpecs);
    }

    @Test
    public void testOnSideUiSpecsChanged_rightAnchorWebContentsHeightType_ltr() {
        mFindToolbarTablet.setLayoutDirection(View.LAYOUT_DIRECTION_LTR);
        SideUiSpecs specs = createSideUiSpecs(SIDE_UI_WIDTH_PX, HeightType.WEB_CONTENTS);

        mFindToolbarTablet.onSideUiSpecsChanged(specs);

        MarginLayoutParams lp = (MarginLayoutParams) mFindToolbarTablet.getLayoutParams();
        assertEquals(BASE_MARGIN_END_PX + SIDE_UI_WIDTH_PX, lp.getMarginEnd());
    }

    @Test
    public void testOnSideUiSpecsChanged_leftAnchorWebContentsHeightType_rtl() {
        mFindToolbarTablet.setLayoutDirection(View.LAYOUT_DIRECTION_RTL);
        Map<@AnchorSide Integer, SideUiSize> map = new ArrayMap<>();
        map.put(AnchorSide.LEFT, new SideUiSize(SIDE_UI_WIDTH_PX, HeightType.WEB_CONTENTS));
        SideUiSpecs specs = new SideUiSpecs(map);

        mFindToolbarTablet.onSideUiSpecsChanged(specs);

        MarginLayoutParams lp = (MarginLayoutParams) mFindToolbarTablet.getLayoutParams();
        assertEquals(BASE_MARGIN_END_PX + SIDE_UI_WIDTH_PX, lp.getMarginEnd());
    }

    @Test
    public void testOnSideUiSpecsChanged_toolbarHeightType() {
        SideUiSpecs specs = createSideUiSpecs(SIDE_UI_WIDTH_PX, HeightType.TOOLBAR);

        mFindToolbarTablet.onSideUiSpecsChanged(specs);

        // Toolbar height type should NOT add margin to FindToolbarTablet because
        // ToolbarControlContainer already offsets itself.
        MarginLayoutParams lp = (MarginLayoutParams) mFindToolbarTablet.getLayoutParams();
        assertEquals(BASE_MARGIN_END_PX, lp.getMarginEnd());
    }

    @Test
    public void testOnSideUiSpecsChanged_closed() {
        SideUiSpecs specs = createSideUiSpecs(0, HeightType.NOT_APPLICABLE);

        mFindToolbarTablet.onSideUiSpecsChanged(specs);

        MarginLayoutParams lp = (MarginLayoutParams) mFindToolbarTablet.getLayoutParams();
        assertEquals(BASE_MARGIN_END_PX, lp.getMarginEnd());
    }

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
