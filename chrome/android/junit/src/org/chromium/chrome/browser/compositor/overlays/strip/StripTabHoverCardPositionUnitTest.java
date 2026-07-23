// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.anyFloat;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Robolectric.buildActivity;

import android.app.Activity;
import android.content.Context;
import android.view.View;

import androidx.coordinatorlayout.widget.CoordinatorLayout.LayoutParams;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.SysUtils;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_management.TabHoverCardView;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link TabHoverCardView} positioning on the tab strip. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, qualifiers = "sw600dp")
public class StripTabHoverCardPositionUnitTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Tab mHoveredTab;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabContentManager mTabContentManager;

    private final SettableMonotonicObservableSupplier<TabContentManager>
            mTabContentManagerSupplier = ObservableSuppliers.createMonotonic();
    private final SettableMonotonicObservableSupplier<TabModel> mTabModelSupplier =
            ObservableSuppliers.createMonotonic();

    private static final float STRIP_STACK_HEIGHT = 500.f;
    private static final float TAB_WIDTH = 100f;

    // Used as a @Spy.
    private TabHoverCardView mTabHoverCardView;
    private Context mContext;
    private int mHoverCardWidth;

    @Before
    public void setUp() {
        mTabContentManagerSupplier.set(mTabContentManager);

        Activity activity = buildActivity(Activity.class).setup().get();
        activity.setTheme(R.style.Theme_BrowserUI_DayNight);
        var tabHoverCardView =
                (TabHoverCardView)
                        activity.getLayoutInflater().inflate(R.layout.tab_hover_card_holder, null);
        mTabHoverCardView = spy(tabHoverCardView);

        mContext = mTabHoverCardView.getContext();
        mContext.getResources().getDisplayMetrics().density = 1f;

        when(mTabModelSelector.getCurrentTabModelSupplier()).thenReturn(mTabModelSupplier);
        mTabHoverCardView.initialize(mTabModelSelector, mTabContentManagerSupplier);

        mHoverCardWidth =
                mContext.getResources().getDimensionPixelSize(R.dimen.tab_hover_card_width);

        var originalLayoutParams = new LayoutParams((int) mHoverCardWidth, 200);
        when(mTabHoverCardView.getLayoutParams()).thenReturn(originalLayoutParams);

        SysUtils.setIsLowEndDeviceForTesting(false);
    }

    @Test
    @SuppressWarnings("DirectInvocationOnMock")
    public void show() {
        var url = JUnitTestGURLs.EXAMPLE_URL;
        var title = "Tab 1";
        when(mHoveredTab.getTitle()).thenReturn(title);
        when(mHoveredTab.getUrl()).thenReturn(url);
        when(mHoveredTab.getId()).thenReturn(1);

        float[] position =
                StripLayoutUtils.getHoverCardPosition(
                        mTabHoverCardView, false, 10, 20, STRIP_STACK_HEIGHT, 0f);
        mTabHoverCardView.show(mHoveredTab, position[0], position[1]);

        verify(mTabHoverCardView).setX(anyFloat());
        verify(mTabHoverCardView).setY(anyFloat());
        verify(mTabHoverCardView).setVisibility(eq(View.VISIBLE));
    }

    @Test
    public void hoverCardOffsetsByStripTopPadding() {
        var url = JUnitTestGURLs.EXAMPLE_URL;
        var title = "Tab 1";
        var topPadding = 10f;
        when(mHoveredTab.getTitle()).thenReturn(title);
        when(mHoveredTab.getUrl()).thenReturn(url);
        when(mHoveredTab.getId()).thenReturn(1);

        float[] position =
                StripLayoutUtils.getHoverCardPosition(
                        mTabHoverCardView, false, 10, 20, STRIP_STACK_HEIGHT, topPadding);
        mTabHoverCardView.show(mHoveredTab, position[0], position[1]);

        verify(mTabHoverCardView).setY(STRIP_STACK_HEIGHT + topPadding);
        verify(mTabHoverCardView).setVisibility(eq(View.VISIBLE));
    }

    @Test
    public void getHoverCardPosition() {
        // Set simulated hovered tab drawX for expected hover card position.
        float[] position =
                StripLayoutUtils.getHoverCardPosition(
                        mTabHoverCardView, false, 10, 0, STRIP_STACK_HEIGHT, 0f);
        float inactiveTabCardXOffset =
                mContext.getResources().getDimension(R.dimen.inactive_tab_hover_card_x_offset);
        assertEquals(
                "Card x position is incorrect.", 10f + inactiveTabCardXOffset, position[0], 0f);
        assertEquals("Card y position is incorrect.", STRIP_STACK_HEIGHT, position[1], 0f);
    }

    @Test
    public void getHoverCardPosition_NonZeroStripTopPadding() {
        float topPadding = 20f;
        // Set simulated hovered tab drawX for expected hover card position.
        float[] position =
                StripLayoutUtils.getHoverCardPosition(
                        mTabHoverCardView, false, 10, 0, STRIP_STACK_HEIGHT, topPadding);
        float inactiveTabCardXOffset =
                mContext.getResources().getDimension(R.dimen.inactive_tab_hover_card_x_offset);
        assertEquals(
                "Card x position is incorrect.", 10f + inactiveTabCardXOffset, position[0], 0f);
        assertEquals(
                "Card y position is incorrect.", STRIP_STACK_HEIGHT + topPadding, position[1], 0f);
    }

    @Test
    public void getHoverCardPosition_CardWidthExceedsWindowWidth() {
        // Set window width to be slightly smaller than the default card width.
        mContext.getResources().getDisplayMetrics().widthPixels = (int) (mHoverCardWidth - 1);

        // Set simulated hovered tab drawX for expected hover card position.
        float[] position =
                StripLayoutUtils.getHoverCardPosition(
                        mTabHoverCardView, true, 10f, TAB_WIDTH, STRIP_STACK_HEIGHT, 0f);
        ArgumentCaptor<LayoutParams> captor = ArgumentCaptor.forClass(LayoutParams.class);
        verify(mTabHoverCardView).setLayoutParams(captor.capture());
        assertEquals(
                "Card width is incorrect.",
                Math.round(0.9f * (mHoverCardWidth - 1)),
                captor.getValue().width);
        assertEquals("Card x position is incorrect.", 10f, position[0], 0f);
        assertEquals("Card y position is incorrect.", STRIP_STACK_HEIGHT, position[1], 0f);
    }

    @Test
    public void cardWidthAcrossWindowResizes() {
        // Set window width to be slightly smaller than the default card width.
        mContext.getResources().getDisplayMetrics().widthPixels = (int) (mHoverCardWidth - 1);
        StripLayoutUtils.getHoverCardPosition(
                mTabHoverCardView, false, 10f, TAB_WIDTH, STRIP_STACK_HEIGHT, 0f);

        // Set window width to be big enough to accommodate the default card width.
        mContext.getResources().getDisplayMetrics().widthPixels = (int) (mHoverCardWidth * 2);
        // Last LayoutParams should reflect updated width.
        when(mTabHoverCardView.getLayoutParams())
                .thenReturn(new LayoutParams(Math.round(0.9f * (mHoverCardWidth - 1)), 200));
        StripLayoutUtils.getHoverCardPosition(
                mTabHoverCardView, false, 10f, TAB_WIDTH, STRIP_STACK_HEIGHT, 0f);

        ArgumentCaptor<LayoutParams> captor = ArgumentCaptor.forClass(LayoutParams.class);
        verify(mTabHoverCardView, times(2)).setLayoutParams(captor.capture());
        var paramsList = captor.getAllValues();
        assertEquals(
                "Card width within small window is incorrect.",
                Math.round(0.9f * (mHoverCardWidth - 1)),
                paramsList.get(0).width);
        assertEquals(
                "Card width within big window is incorrect.",
                mHoverCardWidth,
                paramsList.get(1).width);
    }

    @Test
    public void getHoverCardPosition_CardCrossesWindowBounds() {
        float windowHorizontalMargin =
                mContext.getResources()
                        .getDimension(R.dimen.tab_hover_card_window_horizontal_margin);

        // Assume that the tab's hover card is positioned beyond the left edge of the app window.
        float[] position =
                StripLayoutUtils.getHoverCardPosition(
                        mTabHoverCardView, true, -1f, TAB_WIDTH, STRIP_STACK_HEIGHT, 0f);
        assertEquals(
                "Card should maintain a minimum margin from the left edge of the app window.",
                windowHorizontalMargin,
                position[0],
                0f);

        // Assume that the tab's hover card extends beyond the right edge of the app window.
        int windowWidth = mContext.getResources().getDisplayMetrics().widthPixels;
        position =
                StripLayoutUtils.getHoverCardPosition(
                        mTabHoverCardView,
                        true,
                        windowWidth - mHoverCardWidth + 1f,
                        TAB_WIDTH,
                        STRIP_STACK_HEIGHT,
                        0f);
        assertEquals(
                "Card should maintain a minimum margin from the right edge of the app window.",
                windowWidth - mHoverCardWidth - windowHorizontalMargin,
                position[0],
                0f);
    }

    @Test
    public void getHoverCardPosition_RtlLayout() {
        LocalizationUtils.setRtlForTesting(true);

        // Set simulated hovered tab drawX and width for expected hover card position.
        float[] position =
                StripLayoutUtils.getHoverCardPosition(
                        mTabHoverCardView, false, 28, mHoverCardWidth - 2f, STRIP_STACK_HEIGHT, 0f);
        float detachedCardOffset =
                mContext.getResources().getDimension(R.dimen.inactive_tab_hover_card_x_offset);
        assertEquals("Card x position is incorrect.", 26f - detachedCardOffset, position[0], 0f);
    }

    @Test
    public void getHoverCardPosition_LowEndDevice() {
        SysUtils.setIsLowEndDeviceForTesting(true);

        float[] position =
                StripLayoutUtils.getHoverCardPosition(
                        mTabHoverCardView, false, 10f, TAB_WIDTH, STRIP_STACK_HEIGHT, 0f);
        float detachedCardOffset =
                mContext.getResources().getDimension(R.dimen.inactive_tab_hover_card_x_offset);
        float cardShadowLength =
                mContext.getResources().getDimension(R.dimen.tab_hover_card_elevation);
        assertEquals(
                "Card x position is incorrect.",
                10f + detachedCardOffset - cardShadowLength,
                position[0],
                0f);
        assertEquals(
                "Card y position is incorrect.",
                STRIP_STACK_HEIGHT - cardShadowLength,
                position[1],
                0f);
    }
}
