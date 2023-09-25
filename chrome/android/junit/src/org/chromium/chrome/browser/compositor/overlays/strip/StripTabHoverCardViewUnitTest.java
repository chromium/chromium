// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.anyFloat;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.res.ColorStateList;
import android.view.ContextThemeWrapper;
import android.view.View;
import android.widget.TextView;

import androidx.coordinatorlayout.widget.CoordinatorLayout.LayoutParams;
import androidx.core.content.ContextCompat;
import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;
import org.robolectric.annotation.LooperMode.Mode;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_management.TabManagementFieldTrial;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link StripTabHoverCardView}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({ChromeFeatureList.TAB_STRIP_REDESIGN})
@Config(manifest = Config.NONE, qualifiers = "sw600dp")
@LooperMode(Mode.LEGACY)
public class StripTabHoverCardViewUnitTest {
    @Rule
    public TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();
    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock
    private TextView mTitleView;
    @Mock
    private TextView mUrlView;
    @Mock
    private Tab mHoveredTab;
    @Mock
    private StripLayoutTab mHoveredStripTab;
    @Mock
    private TabModelSelector mTabModelSelector;

    private static final float STRIP_STACK_HEIGHT = 500.f;

    private StripTabHoverCardView mTabHoverCardView;
    private Context mContext;

    @Before
    public void setUp() {
        mContext = new ContextThemeWrapper(ApplicationProvider.getApplicationContext(),
                org.chromium.chrome.R.style.Theme_BrowserUI_DayNight);
        mTabHoverCardView = new StripTabHoverCardView(mContext);
        mTabHoverCardView.setTitleViewForTesting(mTitleView);
        mTabHoverCardView.setUrlViewForTesting(mUrlView);
        mContext.getResources().getDisplayMetrics().density = 1f;
    }

    @Test
    public void show() {
        var url = JUnitTestGURLs.EXAMPLE_URL;
        var title = "Tab 1";
        when(mHoveredTab.getTitle()).thenReturn(title);
        when(mHoveredTab.getUrl()).thenReturn(url);

        StripTabHoverCardView cardViewSpy = spy(mTabHoverCardView);
        cardViewSpy.show(mHoveredTab, mHoveredStripTab, false, STRIP_STACK_HEIGHT);

        verify(mTitleView).setText(eq(mHoveredTab.getTitle()));
        verify(mUrlView).setText(eq(mHoveredTab.getUrl().getHost()));
        verify(cardViewSpy).setX(anyFloat());
        verify(cardViewSpy).setY(anyFloat());
        verify(cardViewSpy).setVisibility(eq(View.VISIBLE));

        // Test chrome:// tab hover card display text.
        url = JUnitTestGURLs.NTP_URL;
        when(mHoveredTab.getUrl()).thenReturn(url);

        mTabHoverCardView.show(mHoveredTab, mHoveredStripTab, false, STRIP_STACK_HEIGHT);
        verify(mUrlView).setText(eq(mHoveredTab.getUrl().getSpec().replaceFirst("/$", "")));
    }

    @Test
    public void getHoverCardPosition() {
        // Use TSR detached treatment for additional coverage that includes position adjustments.
        TabManagementFieldTrial.TAB_STRIP_REDESIGN_ENABLE_DETACHED.setForTesting(true);
        // Set simulated hovered StripLayoutTab drawX for expected hover card position.
        when(mHoveredStripTab.getDrawX()).thenReturn(10f);

        float[] position =
                mTabHoverCardView.getHoverCardPosition(mHoveredStripTab, false, STRIP_STACK_HEIGHT);
        float detachedCardOffset =
                mContext.getResources().getDimension(R.dimen.tsr_no_feet_tab_hover_card_x_offset);
        assertEquals("Card x position is incorrect.", 10f + detachedCardOffset, position[0], 0f);
        assertEquals("Card y position is incorrect.",
                STRIP_STACK_HEIGHT + StripLayoutHelper.FOLIO_DETACHED_BOTTOM_MARGIN_DP, position[1],
                0f);
    }

    @Test
    public void getHoverCardPosition_CardWidthExceedsWindowWidth() {
        // Set window width to be slightly smaller than the default card width.
        float cardWidth = mContext.getResources().getDimension(R.dimen.tab_hover_card_width);
        mContext.getResources().getDisplayMetrics().widthPixels = (int) (cardWidth - 1);
        // Set simulated hovered StripLayoutTab drawX for expected hover card position.
        when(mHoveredStripTab.getDrawX()).thenReturn(10f);
        var originalLayoutParams = new LayoutParams((int) cardWidth, 200);
        StripTabHoverCardView cardViewSpy = spy(mTabHoverCardView);
        when(cardViewSpy.getLayoutParams()).thenReturn(originalLayoutParams);

        float[] position =
                cardViewSpy.getHoverCardPosition(mHoveredStripTab, false, STRIP_STACK_HEIGHT);
        ArgumentCaptor<LayoutParams> captor = ArgumentCaptor.forClass(LayoutParams.class);
        verify(cardViewSpy).setLayoutParams(captor.capture());
        assertEquals("Card width is incorrect.", Math.round(0.9f * (cardWidth - 1)),
                captor.getValue().width);
        assertEquals("Card x position is incorrect.", 10f, position[0], 0f);
        assertEquals("Card y position is incorrect.", STRIP_STACK_HEIGHT, position[1], 0f);
    }

    @Test
    public void getHoverCardPosition_CardCrossesWindowBounds() {
        float windowHorizontalMargin = mContext.getResources().getDimension(
                R.dimen.tab_hover_card_window_horizontal_margin);

        // Assume that the tab's hover card is positioned beyond the left edge of the app window.
        when(mHoveredStripTab.getDrawX()).thenReturn(-1f);
        float[] position =
                mTabHoverCardView.getHoverCardPosition(mHoveredStripTab, false, STRIP_STACK_HEIGHT);
        assertEquals("Card should maintain a minimum margin from the left edge of the app window.",
                windowHorizontalMargin, position[0], 0f);

        // Assume that the tab's hover card extends beyond the right edge of the app window.
        int windowWidth = mContext.getResources().getDisplayMetrics().widthPixels;
        float cardWidth = mContext.getResources().getDimension(R.dimen.tab_hover_card_width);
        when(mHoveredStripTab.getDrawX()).thenReturn(windowWidth - cardWidth + 1);
        position =
                mTabHoverCardView.getHoverCardPosition(mHoveredStripTab, false, STRIP_STACK_HEIGHT);
        assertEquals("Card should maintain a minimum margin from the right edge of the app window.",
                windowWidth - cardWidth - windowHorizontalMargin, position[0], 0f);
    }

    @Test
    public void getHoverCardPosition_RtlLayout() {
        // Use TSR detached treatment for additional coverage that includes position adjustments.
        TabManagementFieldTrial.TAB_STRIP_REDESIGN_ENABLE_DETACHED.setForTesting(true);
        LocalizationUtils.setRtlForTesting(true);

        float cardWidth = mContext.getResources().getDimension(R.dimen.tab_hover_card_width);
        // Set simulated hovered StripLayoutTab drawX and width for expected hover card position.
        when(mHoveredStripTab.getDrawX()).thenReturn(28f);
        when(mHoveredStripTab.getWidth()).thenReturn(cardWidth - 2);

        float[] position =
                mTabHoverCardView.getHoverCardPosition(mHoveredStripTab, false, STRIP_STACK_HEIGHT);
        float detachedCardOffset =
                mContext.getResources().getDimension(R.dimen.tsr_no_feet_tab_hover_card_x_offset);
        assertEquals("Card x position is incorrect.", 26f - detachedCardOffset, position[0], 0f);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ADVANCED_PERIPHERALS_SUPPORT_TAB_STRIP)
    public void updateHoverCardColors() {
        StripTabHoverCardView cardViewSpy = spy(mTabHoverCardView);

        // Test incognito colors.
        cardViewSpy.updateHoverCardColors(true);
        verify(cardViewSpy)
                .setBackgroundTintList(eq(ColorStateList.valueOf(ContextCompat.getColor(
                        mContext, R.color.default_bg_color_dark_elev_5_baseline))));
        verify(mTitleView).setTextColor(eq(mContext.getColor(R.color.default_text_color_light)));
        verify(mUrlView).setTextColor(
                eq(mContext.getColor(R.color.default_text_color_secondary_light)));

        // Test standard colors.
        cardViewSpy.updateHoverCardColors(false);
        verify(cardViewSpy)
                .setBackgroundTintList(eq(ColorStateList.valueOf(ChromeColors.getSurfaceColor(
                        mContext, R.dimen.tab_hover_card_bg_color_elev))));
        verify(mTitleView).setTextColor(eq(SemanticColorUtils.getDefaultTextColor(mContext)));
        verify(mUrlView).setTextColor(
                eq(SemanticColorUtils.getDefaultTextColorSecondary(mContext)));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ADVANCED_PERIPHERALS_SUPPORT_TAB_STRIP)
    public void initialize() {
        StripTabHoverCardView cardViewSpy = spy(mTabHoverCardView);

        // View is inflated in standard tab model.
        when(mTabModelSelector.isIncognitoSelected()).thenReturn(false);
        cardViewSpy.initialize(mTabModelSelector);
        verify(cardViewSpy).updateHoverCardColors(false);

        // View is inflated in incognito tab model.
        when(mTabModelSelector.isIncognitoSelected()).thenReturn(true);
        cardViewSpy.initialize(mTabModelSelector);
        verify(cardViewSpy).updateHoverCardColors(true);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ADVANCED_PERIPHERALS_SUPPORT_TAB_STRIP)
    public void tabModelSelectorObserver_OnTabModelSelected() {
        var standardTabModel = mock(TabModel.class);
        var incognitoTabModel = mock(TabModel.class);
        when(standardTabModel.isIncognito()).thenReturn(false);
        when(incognitoTabModel.isIncognito()).thenReturn(true);

        StripTabHoverCardView cardViewSpy = spy(mTabHoverCardView);

        // Assume standard tab model.
        when(mTabModelSelector.isIncognitoSelected()).thenReturn(false);
        // TabModelSelectorObserver should be added after the view is inflated.
        cardViewSpy.initialize(mTabModelSelector);
        var tabModelSelectorObserver = cardViewSpy.getTabModelSelectorObserverForTesting();
        assertNotNull("TabModelSelectorObserver should be set.", tabModelSelectorObserver);

        // Switch to the incognito tab model.
        tabModelSelectorObserver.onTabModelSelected(incognitoTabModel, standardTabModel);
        verify(cardViewSpy).updateHoverCardColors(true);

        // Switch to the standard tab model.
        tabModelSelectorObserver.onTabModelSelected(standardTabModel, incognitoTabModel);
        // Invoked once in #onInflate(), subsequently in #onTabModelSelected().
        verify(cardViewSpy, times(2)).updateHoverCardColors(false);
    }
}
