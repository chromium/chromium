// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.os.Build;
import android.text.TextUtils;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.layouts.LayoutRenderHost;
import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;
import org.chromium.chrome.browser.compositor.layouts.components.CompositorButton;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.layouts.components.VirtualView;
import org.chromium.chrome.browser.tasks.tab_management.TabUiFeatureUtilities;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.ui.base.LocalizationUtils;

import java.util.ArrayList;
import java.util.List;

/** Tests for {@link StripLayoutHelper}. */
@RunWith(BaseRobolectricTestRunner.class)
@Features.EnableFeatures(ChromeFeatureList.TAB_STRIP_IMPROVEMENTS)
@Config(manifest = Config.NONE, sdk = Build.VERSION_CODES.M)
public class StripLayoutHelperTest {
    @Rule
    public TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();
    @Mock
    private LayoutUpdateHost mUpdateHost;
    @Mock
    private LayoutRenderHost mRenderHost;
    @Mock
    private CompositorButton mModelSelectorBtn;

    private Activity mActivity;
    private TestTabModel mModel = new TestTabModel();
    private StripLayoutHelper mStripLayoutHelper;
    private boolean mIncognito;
    private static final String[] TEST_TAB_TITLES = {"Tab 1", "Tab 2", "Tab 3", "", null};
    private static final String CLOSE_TAB = "Close %1$s tab";
    private static final String IDENTIFIER = "Tab";
    private static final String IDENTIFIER_SELECTED = "Selected Tab";
    private static final String INCOGNITO_IDENTIFIER = "Incognito Tab";
    private static final String INCOGNITO_IDENTIFIER_SELECTED = "Selected Incognito Tab";
    private static final float SCREEN_WIDTH = 800.f;
    private static final float SCREEN_HEIGHT = 1600.f;
    private static final float TAB_WIDTH_1 = 140.f;
    private static final float TAB_WIDTH_2 = 160.f;
    private static final float TAB_WIDTH_SMALL = 108.f;
    private static final float TAB_OVERLAP_WIDTH = 24.f;
    private static final float TAB_WIDTH_MEDIUM = 156.f;
    private static final long TIMESTAMP = 5000;
    private static final float NEW_TAB_BTN_X = 700.f;

    private static final float CLOSE_BTN_VISIBILITY_THRESHOLD_END = 72;
    private static final float CLOSE_BTN_VISIBILITY_THRESHOLD_END_MODEL_SELECTOR = 120;

    /** Reset the environment before each test. */
    @Before
    public void beforeTest() {
        MockitoAnnotations.initMocks(this);
        when(mModelSelectorBtn.isVisible()).thenReturn(true);

        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        TabUiFeatureUtilities.setTabMinWidthForTesting(null);
    }

    /**
     * Test method for {@link StripLayoutHelper#getVirtualViews(List<VirtualView>)}.
     *
     * Checks that it returns the correct order of tabs, including correct content.
     */
    @Test
    @Feature({"Accessibility"})
    public void testSimpleTabOrder() {
        initializeTest(false, false, 0);

        assertTabStripAndOrder(getExpectedAccessibilityDescriptions(0));
    }

    /**
     * Test method for {@link StripLayoutHelper#getVirtualViews(List<VirtualView>)}.
     *
     * Checks that it returns the correct order of tabs, even when a tab except the first one is
     * selected.
     */
    @Test
    @Feature({"Accessibility"})
    public void testTabOrderWithIndex() {
        initializeTest(false, false, 1);

        // Tabs should be in left to right order regardless of index
        assertTabStripAndOrder(getExpectedAccessibilityDescriptions(1));
    }

    /**
     * Test method for {@link StripLayoutHelper#getVirtualViews(List<VirtualView>)}.
     *
     * Checks that it returns the correct order of tabs, even in RTL mode.
     */
    @Test
    @Feature({"Accessibility"})
    public void testTabOrderRtl() {
        initializeTest(true, false, 0);

        // Tabs should be in linear order even in RTL.
        // Android will take care of reversing it.
        assertTabStripAndOrder(getExpectedAccessibilityDescriptions(0));
    }

    /**
     * Test method for {@link StripLayoutHelper#getVirtualViews(List<VirtualView>)}.
     *
     * Checks that it returns the correct order of tabs, even in incognito mode.
     */
    @Test
    @Feature({"Accessibility"})
    public void testIncognitoAccessibilityDescriptions() {
        initializeTest(false, true, 0);

        assertTabStripAndOrder(getExpectedAccessibilityDescriptions(0));
    }

    @Test
    @Feature("Tab Strip Improvements")
    @Config(qualifiers = "sw800dp")
    public void testStripStacker_TabStripImprovementsEnabled_Scroll() {
        initializeTest(false, true, 0);

        // Assert
        assertFalse(mStripLayoutHelper.shouldCascadeTabs());
    }

    @Test
    @Feature("Tab Strip Improvements")
    @Config(qualifiers = "sw800dp")
    @Features.DisableFeatures(ChromeFeatureList.TAB_STRIP_IMPROVEMENTS)
    public void testStripStacker_TabStripImprovementsDisabled_Cascade() {
        initializeTest(false, true, 0);

        // Assert
        assertTrue(mStripLayoutHelper.shouldCascadeTabs());
    }

    @Test
    public void testAllTabsClosed() {
        initializeTest(false, false, 0);
        assertTrue(mStripLayoutHelper.getStripLayoutTabs().length == TEST_TAB_TITLES.length);

        // Close all tabs
        mModel.closeAllTabs();

        // Notify strip of tab closure
        mStripLayoutHelper.willCloseAllTabs();

        // Verify strip has no tabs.
        assertTrue(mStripLayoutHelper.getStripLayoutTabs().length == 0);
    }

    @Test
    @Feature("Tab Strip Improvements")
    public void testStripStacker_UpdateCloseButtons() {
        // Set fourth tab as selected
        TabUiFeatureUtilities.setTabMinWidthForTesting(TAB_WIDTH_SMALL);
        initializeTest(false, true, 3);
        mStripLayoutHelper.onSizeChanged(SCREEN_WIDTH, SCREEN_HEIGHT);
        StripLayoutTab[] tabs = getMockedStripLayoutTabs(TAB_WIDTH_1);
        mStripLayoutHelper.setStripLayoutTabsForTest(tabs);

        mStripLayoutHelper.tabSelected(1, 3, 0);

        // Close btn should be visible on the selected tab.
        Mockito.verify(tabs[3]).setCanShowCloseButton(true);
        // Close btn is hidden on unselected tabs.
        Mockito.verify(tabs[0]).setCanShowCloseButton(false);
        Mockito.verify(tabs[1]).setCanShowCloseButton(false);
        Mockito.verify(tabs[2]).setCanShowCloseButton(false);
        Mockito.verify(tabs[4]).setCanShowCloseButton(false);
    }

    @Test
    @Feature("Tab Strip Improvements")
    public void testTabSelected_SelectedTab_EdgeTab_HideCloseBtn() {
        // Set fourth tab as selected
        TabUiFeatureUtilities.setTabMinWidthForTesting(TAB_WIDTH_SMALL);
        initializeTest(false, true, 3);
        mStripLayoutHelper.onSizeChanged(SCREEN_WIDTH, SCREEN_HEIGHT);
        StripLayoutTab[] tabs = getMockedStripLayoutTabs(TAB_WIDTH_1);
        mStripLayoutHelper.setStripLayoutTabsForTest(tabs);
        when(tabs[3].getDrawX()).thenReturn(600.f);

        mStripLayoutHelper.tabSelected(1, 3, 0);

        // Close btn should be hidden on the selected tab as its an edge tab.
        Mockito.verify(tabs[3]).setCanShowCloseButton(false);
        // Close btn is hidden on unselected tabs.
        Mockito.verify(tabs[0]).setCanShowCloseButton(false);
        Mockito.verify(tabs[1]).setCanShowCloseButton(false);
        Mockito.verify(tabs[2]).setCanShowCloseButton(false);
        Mockito.verify(tabs[4]).setCanShowCloseButton(false);
    }

    @Test
    @Feature("Tab Strip Improvements")
    public void testTabSelected_EdgeTab_Start_Ltr_HideCloseBtn() {
        // Arrange
        TabUiFeatureUtilities.setTabMinWidthForTesting(TAB_WIDTH_MEDIUM);
        initializeTest(false, true, 3);
        StripLayoutTab[] tabs = getMockedStripLayoutTabs(TAB_WIDTH_2);
        // Set mWidth value to 800.f
        mStripLayoutHelper.onSizeChanged(SCREEN_WIDTH, SCREEN_HEIGHT);
        mStripLayoutHelper.getNewTabButton().setX(600.f);
        // The leftmost tab is partially hidden
        when(tabs[0].getDrawX()).thenReturn(-80.f);
        mStripLayoutHelper.setStripLayoutTabsForTest(tabs);

        // Act
        mStripLayoutHelper.tabSelected(1, 3, 0);

        // Assert
        // Close btn should be hidden for the partially visible edge tab.
        Mockito.verify(tabs[0]).setCanShowCloseButton(false);
        // Close button is visible for the rest of the tabs.
        Mockito.verify(tabs[1]).setCanShowCloseButton(true);
        Mockito.verify(tabs[2]).setCanShowCloseButton(true);
        Mockito.verify(tabs[3]).setCanShowCloseButton(true);
        Mockito.verify(tabs[4]).setCanShowCloseButton(true);
    }

    @Test
    @Feature("Tab Strip Improvements")
    public void testTabSelected_EdgeTab_End_Ltr_HideCloseBtn() {
        // Arrange
        TabUiFeatureUtilities.setTabMinWidthForTesting(TAB_WIDTH_MEDIUM);
        initializeTest(false, false, 3);
        StripLayoutTab[] tabs = getMockedStripLayoutTabs(TAB_WIDTH_2);
        // Set mWidth value to 800.f
        mStripLayoutHelper.onSizeChanged(SCREEN_WIDTH, SCREEN_HEIGHT);
        mStripLayoutHelper.getNewTabButton().setX(700);
        float drawXToHideCloseBtn = SCREEN_WIDTH - CLOSE_BTN_VISIBILITY_THRESHOLD_END_MODEL_SELECTOR
                - TAB_WIDTH_2 + TAB_OVERLAP_WIDTH + 1; // 1 is to cross threshold
        when(tabs[3].getDrawX()).thenReturn(drawXToHideCloseBtn);
        mStripLayoutHelper.setStripLayoutTabsForTest(tabs);

        // Act
        mStripLayoutHelper.tabSelected(1, 3, 0);

        // Assert
        // Close button is visible for the rest of the tabs.
        Mockito.verify(tabs[0]).setCanShowCloseButton(true);
        Mockito.verify(tabs[1]).setCanShowCloseButton(true);
        Mockito.verify(tabs[2]).setCanShowCloseButton(true);
        Mockito.verify(tabs[4]).setCanShowCloseButton(true);
        // Close btn should be hidden for the partially visible edge tab.
        Mockito.verify(tabs[3]).setCanShowCloseButton(false);
    }

    @Test
    @Feature("Tab Strip Improvements")
    public void testTabSelected_LastTab_ShowCloseBtn() {
        TabUiFeatureUtilities.setTabMinWidthForTesting(TAB_WIDTH_MEDIUM);
        initializeTest(false, false, 3);
        StripLayoutTab[] tabs = getMockedStripLayoutTabs(TAB_WIDTH_2);
        // Set mWidth value to 800.f
        mStripLayoutHelper.onSizeChanged(SCREEN_WIDTH, SCREEN_HEIGHT);
        mStripLayoutHelper.getNewTabButton().setX(NEW_TAB_BTN_X);
        // newTabBtn.X(700.f) - tab.width(160.f) + mTabOverlapWidth(24.f)
        when(tabs[4].getDrawX()).thenReturn(564.f);
        mStripLayoutHelper.setStripLayoutTabsForTest(tabs);

        // Act
        mStripLayoutHelper.tabSelected(1, 3, 0);

        // Assert
        // Close button is visible for all tabs.
        Mockito.verify(tabs[0]).setCanShowCloseButton(true);
        Mockito.verify(tabs[1]).setCanShowCloseButton(true);
        Mockito.verify(tabs[2]).setCanShowCloseButton(true);
        Mockito.verify(tabs[3]).setCanShowCloseButton(true);
        Mockito.verify(tabs[4]).setCanShowCloseButton(true);
    }

    @Test
    @Feature("Tab Strip Improvements")
    public void testTabSelected_LastTab_EdgeTab_HideCloseBtn() {
        TabUiFeatureUtilities.setTabMinWidthForTesting(TAB_WIDTH_MEDIUM);
        initializeTest(false, false, 3);
        StripLayoutTab[] tabs = getMockedStripLayoutTabs(TAB_WIDTH_2);

        // Set mWidth value to 800.f
        mStripLayoutHelper.onSizeChanged(SCREEN_WIDTH, SCREEN_HEIGHT);
        mStripLayoutHelper.getNewTabButton().setX(NEW_TAB_BTN_X);
        // newTabBtn.X(700.f) - tab.width(160.f) + mTabOverlapWidth(24.f) + 1
        when(tabs[4].getDrawX()).thenReturn(565.f);
        mStripLayoutHelper.setStripLayoutTabsForTest(tabs);

        // Act
        mStripLayoutHelper.tabSelected(1, 3, 0);

        // Assert
        // Close btn should be visible for rest of the tabs.
        Mockito.verify(tabs[0]).setCanShowCloseButton(true);
        Mockito.verify(tabs[1]).setCanShowCloseButton(true);
        Mockito.verify(tabs[2]).setCanShowCloseButton(true);
        Mockito.verify(tabs[3]).setCanShowCloseButton(true);
        // Close btn should be hidden for the partially visible edge tab.
        Mockito.verify(tabs[4]).setCanShowCloseButton(false);
    }

    @Test
    @Feature("Tab Strip Improvements")
    public void testTabSelected_EdgeTab_End_Ltr_NoModelSelBtn_HideCloseBtn() {
        // Arrange
        TabUiFeatureUtilities.setTabMinWidthForTesting(TAB_WIDTH_MEDIUM);
        when(mModelSelectorBtn.isVisible()).thenReturn(false);

        initializeTest(false, false, 3);
        StripLayoutTab[] tabs = getMockedStripLayoutTabs(TAB_WIDTH_2);
        // Set mWidth value to 800.f
        mStripLayoutHelper.onSizeChanged(SCREEN_WIDTH, SCREEN_HEIGHT);
        mStripLayoutHelper.getNewTabButton().setX(700);
        float drawXToHideCloseBtn = SCREEN_WIDTH - CLOSE_BTN_VISIBILITY_THRESHOLD_END - TAB_WIDTH_2
                + TAB_OVERLAP_WIDTH + 1; // 1 is to cross threshold
        when(tabs[3].getDrawX()).thenReturn(drawXToHideCloseBtn);
        mStripLayoutHelper.setStripLayoutTabsForTest(tabs);

        // Act
        mStripLayoutHelper.tabSelected(1, 3, 0);

        // Assert
        // Close button is visible for the rest of the tabs.
        Mockito.verify(tabs[0]).setCanShowCloseButton(true);
        Mockito.verify(tabs[1]).setCanShowCloseButton(true);
        Mockito.verify(tabs[2]).setCanShowCloseButton(true);
        Mockito.verify(tabs[4]).setCanShowCloseButton(true);
        // Close btn should be hidden for the partially visible edge tab.
        Mockito.verify(tabs[3]).setCanShowCloseButton(false);
    }

    @Test
    @Feature("Tab Strip Improvements")
    public void testTabSelected_EdgeTab_Start_Rtl_HideCloseBtn() {
        // Arrange
        TabUiFeatureUtilities.setTabMinWidthForTesting(TAB_WIDTH_MEDIUM);
        initializeTest(true, true, 3);
        StripLayoutTab[] tabs = getMockedStripLayoutTabs(TAB_WIDTH_2, 150.f, 5);
        // Set mWidth value to 800.f.
        mStripLayoutHelper.onSizeChanged(SCREEN_WIDTH, SCREEN_HEIGHT);
        // The leftmost tab is partially hidden.
        when(tabs[0].getDrawX()).thenReturn(60.f);
        mStripLayoutHelper.setStripLayoutTabsForTest(tabs);

        // Act
        mStripLayoutHelper.tabSelected(1, 3, 0);

        // Assert
        // Close btn should be hidden for the partially visible edge tab.
        Mockito.verify(tabs[0]).setCanShowCloseButton(false);
        // Close button is visible for the rest of the tabs.
        Mockito.verify(tabs[1]).setCanShowCloseButton(true);
        Mockito.verify(tabs[2]).setCanShowCloseButton(true);
        Mockito.verify(tabs[3]).setCanShowCloseButton(true);
        Mockito.verify(tabs[4]).setCanShowCloseButton(true);
    }

    @Test
    @Feature("Tab Strip Improvements")
    public void testTabSelected_EdgeTab_End_Rtl_HideCloseBtn() {
        // Arrange
        TabUiFeatureUtilities.setTabMinWidthForTesting(TAB_WIDTH_MEDIUM);
        initializeTest(true, true, 3);
        StripLayoutTab[] tabs = getMockedStripLayoutTabs(TAB_WIDTH_2, 150.f, 5);
        // Set mWidth value to 800.f
        mStripLayoutHelper.onSizeChanged(SCREEN_WIDTH, SCREEN_HEIGHT);
        // To make rightmost tab partially hidden value should be gt than (SCREEN_WIDTH - 120.f -
        // TAB_WIDTH_2).
        when(tabs[4].getDrawX()).thenReturn(710.f);
        mStripLayoutHelper.setStripLayoutTabsForTest(tabs);

        // Act
        mStripLayoutHelper.tabSelected(1, 3, 0);

        // Assert
        // Close button is visible for the rest of the tabs.
        Mockito.verify(tabs[0]).setCanShowCloseButton(true);
        Mockito.verify(tabs[1]).setCanShowCloseButton(true);
        Mockito.verify(tabs[2]).setCanShowCloseButton(true);
        Mockito.verify(tabs[3]).setCanShowCloseButton(true);
        // Close btn should be hidden for the partially visible edge tab.
        Mockito.verify(tabs[4]).setCanShowCloseButton(false);
    }

    @Test
    @Feature("Tab Strip Improvements")
    public void testScrollOffset_OnResume_StartOnLeft_SelectedRightmostTab() {
        // Arrange: Initialize tabs with last tab selected.
        initializeTest(false, true, 9, 10);
        TabUiFeatureUtilities.setTabMinWidthForTesting(TAB_WIDTH_MEDIUM);
        StripLayoutTab[] tabs = getMockedStripLayoutTabs(TAB_WIDTH_MEDIUM, 150.f, 10);
        mStripLayoutHelper.setStripLayoutTabsForTest(tabs);

        // Set screen width to 800dp.
        mStripLayoutHelper.onSizeChanged(SCREEN_WIDTH, SCREEN_HEIGHT);

        mStripLayoutHelper.scrollTabToView(TIMESTAMP, false);

        int expectedFinalX = -1004; // delta(optimalRight(-980) - scrollOffset(0)
                                    // - tabOverlapWidth(24)) + scrollOffset(0)
        assertEquals(expectedFinalX, mStripLayoutHelper.getScroller().getFinalX());
    }

    @Test
    @Feature("Tab Strip Improvements")
    public void testScrollOffset_OnResume_StartOnLeft_NoModelSelBtn_SelectedRightmostTab() {
        // Arrange: Initialize tabs with last tab selected.
        when(mModelSelectorBtn.isVisible()).thenReturn(false);
        initializeTest(false, true, 9, 10);
        TabUiFeatureUtilities.setTabMinWidthForTesting(TAB_WIDTH_MEDIUM);
        StripLayoutTab[] tabs = getMockedStripLayoutTabs(TAB_WIDTH_MEDIUM, 150.f, 10);
        mStripLayoutHelper.setStripLayoutTabsForTest(tabs);

        // Set screen width to 800dp.
        mStripLayoutHelper.onSizeChanged(SCREEN_WIDTH, SCREEN_HEIGHT);

        mStripLayoutHelper.scrollTabToView(TIMESTAMP, false);

        int expectedFinalX = -956; // delta(optimalRight(-932) - scrollOffset(0)
        // - tabOverlapWidth(24)) + scrollOffset(0)
        assertEquals(expectedFinalX, mStripLayoutHelper.getScroller().getFinalX());
    }

    @Test
    @Feature("Tab Strip Improvements")
    public void testScrollOffset_OnResume_StartOnRight_SelectedLeftmostTab() {
        // Arrange: Initialize tabs with first tab selected.
        initializeTest(false, true, 0, 10);
        TabUiFeatureUtilities.setTabMinWidthForTesting(TAB_WIDTH_MEDIUM);
        StripLayoutTab[] tabs = getMockedStripLayoutTabs(TAB_WIDTH_MEDIUM, 150.f, 10);
        mStripLayoutHelper.setStripLayoutTabsForTest(tabs);
        mStripLayoutHelper.testSetScrollOffset(1200);

        // Set screen width to 800dp.
        mStripLayoutHelper.onSizeChanged(SCREEN_WIDTH, SCREEN_HEIGHT);

        mStripLayoutHelper.scrollTabToView(TIMESTAMP, false);

        int expectedFinalX = 490; // delta(optimalRight(514) - tabOverlapWidth(24)
                                  // - scrollOffset(1200)) + scrollOffset(1200)
        assertEquals(expectedFinalX, mStripLayoutHelper.getScroller().getFinalX());
    }

    @Test
    @Feature("Tab Strip Improvements")
    public void testScrollOffset_OnResume_StartOnRight_NoModelSelBtn_SelectedRightmostTab() {
        // Arrange: Initialize tabs with first tab selected.
        when(mModelSelectorBtn.isVisible()).thenReturn(false);
        initializeTest(false, true, 0, 10);
        TabUiFeatureUtilities.setTabMinWidthForTesting(TAB_WIDTH_MEDIUM);
        StripLayoutTab[] tabs = getMockedStripLayoutTabs(TAB_WIDTH_MEDIUM, 150.f, 10);
        mStripLayoutHelper.setStripLayoutTabsForTest(tabs);
        mStripLayoutHelper.testSetScrollOffset(1200);

        // Set screen width to 800dp.
        mStripLayoutHelper.onSizeChanged(SCREEN_WIDTH, SCREEN_HEIGHT);

        mStripLayoutHelper.scrollTabToView(TIMESTAMP, false);

        int expectedFinalX = 538; // delta(optimalRight(562) - tabOverlapWidth(24)
        // - scrollOffset(1200)) + scrollOffset(1200)
        assertEquals(expectedFinalX, mStripLayoutHelper.getScroller().getFinalX());
    }

    @Test
    @Feature("Tab Strip Improvements")
    public void testScrollDuration() {
        initializeTest(false, true, 3);

        // Act: Set scroll offset less than 960.
        mStripLayoutHelper.testSetScrollOffset(800);

        // Assert: Expand duration is 250.
        assertEquals(mStripLayoutHelper.getExpandDurationForTesting(), 250);
    }

    @Test
    @Feature("Tab Strip Improvements")
    public void testScrollDuration_Medium() {
        initializeTest(false, true, 3);

        // Act: Set scroll offset between 960 and 1920.
        mStripLayoutHelper.testSetScrollOffset(1000);

        // Assert: Expand duration is 350.
        assertEquals(mStripLayoutHelper.getExpandDurationForTesting(), 350);
    }

    @Test
    @Feature("Tab Strip Improvements")
    public void testScrollDuration_Large() {
        initializeTest(false, true, 3);

        // Act: Set scroll offset greater than 1920
        mStripLayoutHelper.testSetScrollOffset(2000);

        // Assert: Expand duration is 450.
        assertEquals(mStripLayoutHelper.getExpandDurationForTesting(), 450);
    }

    private void initializeTest(boolean rtl, boolean incognito, int tabIndex, int numTabs) {
        mStripLayoutHelper = createStripLayoutHelper(rtl, incognito);
        mIncognito = incognito;
        if (numTabs <= 5) {
            for (int i = 0; i < numTabs; i++) {
                mModel.addTab(TEST_TAB_TITLES[i]);
                when(mModel.getTabAt(i).isHidden()).thenReturn(tabIndex != i);
            }
        } else {
            for (int i = 0; i < numTabs; i++) {
                mModel.addTab("Tab " + i);
                when(mModel.getTabAt(i).isHidden()).thenReturn(tabIndex != i);
            }
        }
        mModel.setIndex(tabIndex);
        mStripLayoutHelper.setTabModel(mModel, null);
        mStripLayoutHelper.tabSelected(0, tabIndex, 0);
        // Flush UI updated
    }

    private void initializeTest(boolean rtl, boolean incognito, int tabIndex) {
        initializeTest(rtl, incognito, tabIndex, 5);
    }

    private void assertTabStripAndOrder(String[] expectedAccessibilityDescriptions) {
        // Each tab has a "close button", and there is one additional "new tab" button
        final int expectedNumberOfViews = 2 * expectedAccessibilityDescriptions.length + 1;

        final List<VirtualView> views = new ArrayList<>();
        mStripLayoutHelper.getVirtualViews(views);
        assertEquals(expectedNumberOfViews, views.size());

        // Tab titles
        for (int i = 0; i < expectedNumberOfViews - 1; i++) {
            final String expectedDescription = i % 2 == 0
                    ? expectedAccessibilityDescriptions[i / 2]
                    : String.format(CLOSE_TAB, TEST_TAB_TITLES[i / 2]);
            assertEquals(expectedDescription, views.get(i).getAccessibilityDescription());
        }

        assertEquals(mActivity.getResources().getString(mIncognito
                                     ? R.string.accessibility_toolbar_btn_new_incognito_tab
                                     : R.string.accessibility_toolbar_btn_new_tab),
                views.get(views.size() - 1).getAccessibilityDescription());
    }

    private StripLayoutHelper createStripLayoutHelper(boolean rtl, boolean incognito) {
        LocalizationUtils.setRtlForTesting(rtl);
        final StripLayoutHelper stripLayoutHelper = new StripLayoutHelper(
                mActivity, mUpdateHost, mRenderHost, incognito, mModelSelectorBtn);
        // Initialize StackScroller
        stripLayoutHelper.onContextChanged(mActivity);
        return stripLayoutHelper;
    }

    private String[] getExpectedAccessibilityDescriptions(int tabIndex) {
        final String[] expectedAccessibilityDescriptions = new String[TEST_TAB_TITLES.length];
        for (int i = 0; i < TEST_TAB_TITLES.length; i++) {
            final boolean isHidden = (i != tabIndex);
            String suffix;
            if (mIncognito) {
                suffix = isHidden ? INCOGNITO_IDENTIFIER : INCOGNITO_IDENTIFIER_SELECTED;
            } else {
                suffix = isHidden ? IDENTIFIER : IDENTIFIER_SELECTED;
            }
            String expectedDescription = "";
            if (!TextUtils.isEmpty(TEST_TAB_TITLES[i])) {
                expectedDescription += TEST_TAB_TITLES[i] + ", ";
            }
            expectedAccessibilityDescriptions[i] = expectedDescription + suffix;
        }
        return expectedAccessibilityDescriptions;
    }

    private StripLayoutTab[] getMockedStripLayoutTabs(float tabWidth, float mDrawX, int numTabs) {
        StripLayoutTab[] tabs = new StripLayoutTab[mModel.getCount()];

        for (int i = 0; i < numTabs; i++) {
            tabs[i] = mockStripTab(i, tabWidth, mDrawX);
        }

        return tabs;
    }

    private StripLayoutTab[] getMockedStripLayoutTabs(float tabWidth) {
        return getMockedStripLayoutTabs(tabWidth, 0.f, 5);
    }

    private StripLayoutTab mockStripTab(int id, float tabWidth, float mDrawX) {
        StripLayoutTab tab = mock(StripLayoutTab.class);
        when(tab.getWidth()).thenReturn(tabWidth);
        when(tab.getId()).thenReturn(id);
        when(tab.getDrawX()).thenReturn(mDrawX);
        return tab;
    }
}
