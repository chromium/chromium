// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyFloat;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.Context;
import android.content.res.Resources;
import android.text.TextUtils;
import android.util.DisplayMetrics;
import android.view.HapticFeedbackConstants;
import android.view.View;
import android.view.ViewGroup.MarginLayoutParams;

import org.junit.After;
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
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerHost;
import org.chromium.chrome.browser.compositor.layouts.LayoutRenderHost;
import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;
import org.chromium.chrome.browser.compositor.layouts.components.CompositorButton;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutTab.StripLayoutTabDelegate;
import org.chromium.chrome.browser.compositor.overlays.strip.TabLoadTracker.TabLoadTrackerCallback;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.layouts.animation.CompositorAnimationHandler;
import org.chromium.chrome.browser.layouts.animation.CompositorAnimator;
import org.chromium.chrome.browser.layouts.components.VirtualView;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.TabManagementFieldTrial;
import org.chromium.chrome.browser.tasks.tab_management.TabUiFeatureUtilities;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.shadows.ShadowAppCompatResources;

import java.util.ArrayList;
import java.util.List;

/** Tests for {@link StripLayoutHelper}. */
@RunWith(BaseRobolectricTestRunner.class)
// clang-format off
@Features.EnableFeatures({
        ChromeFeatureList.TAB_STRIP_REDESIGN,
        ChromeFeatureList.TAB_GROUPS_FOR_TABLETS})
@Config(manifest = Config.NONE, qualifiers = "sw600dp", shadows = {ShadowAppCompatResources.class})

public class StripLayoutHelperTest {
    // clang-format on
    @Rule
    public TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();
    @Mock
    private View mInteractingTabView;
    @Mock
    private LayoutManagerHost mManagerHost;
    @Mock
    private LayoutUpdateHost mUpdateHost;
    @Mock
    private LayoutRenderHost mRenderHost;
    @Mock
    private CompositorButton mModelSelectorBtn;
    @Mock
    private TabGroupModelFilter mTabGroupModelFilter;
    @Mock
    private StripLayoutHelperManager mStripLayoutHelperManager;

    private Activity mActivity;
    private TestTabModel mModel = new TestTabModel();
    private StripLayoutHelper mStripLayoutHelper;
    private boolean mIncognito;
    private static final String[] TEST_TAB_TITLES = {"Tab 1", "Tab 2", "Tab 3", "", null};
    private static final String EXPECTED_MARGIN = "The tab should have a trailing margin.";
    private static final String EXPECTED_NO_MARGIN = "The tab should not have a trailing margin.";
    private static final String CLOSE_TAB = "Close %1$s tab";
    private static final String IDENTIFIER = "Tab";
    private static final String IDENTIFIER_SELECTED = "Selected Tab";
    private static final String INCOGNITO_IDENTIFIER = "Incognito Tab";
    private static final String INCOGNITO_IDENTIFIER_SELECTED = "Selected Incognito Tab";
    private static final float SCREEN_WIDTH = 800.f;
    private static final float SCREEN_WIDTH_LANDSCAPE = 1200.f;
    private static final float SCREEN_HEIGHT = 1600.f;
    private static final float TAB_WIDTH_1 = 140.f;
    private static final float TAB_WIDTH_2 = 160.f;
    private static final float TAB_WIDTH_SMALL = 108.f;
    private static final float TAB_OVERLAP_WIDTH = 28.f;
    private static final float TAB_WIDTH_MEDIUM = 156.f;
    private static final float TAB_MARGIN_WIDTH = 54.f;
    private static final long TIMESTAMP = 5000;
    private static final float NEW_TAB_BTN_X = 700.f;
    private static final float NEW_TAB_BTN_Y = 1400.f;
    private static final float NEW_TAB_BTN_WIDTH = 100.f;
    private static final float NEW_TAB_BTN_HEIGHT = 100.f;
    private static final float NEW_TAB_BUTTON_WITH_MODEL_SELECTOR_BUTTON_PADDING = 8.f;
    private static final float BUTTON_END_PADDING_FOLIO = 10.f;
    private static final float BUTTON_END_PADDING_DETACHED = 9.f;
    private static final float MODEL_SELECTOR_BUTTON_BG_WIDTH_FOLIO = 36.f;
    private static final float MODEL_SELECTOR_BUTTON_BG_WIDTH_DETACHED = 38.f;

    private static final float CLOSE_BTN_VISIBILITY_THRESHOLD_END = 72;
    private static final float CLOSE_BTN_VISIBILITY_THRESHOLD_END_MODEL_SELECTOR = 120;

    private static final float EPSILON = 0.001f;

    /** Reset the environment before each test. */
    @Before
    public void beforeTest() {
        MockitoAnnotations.initMocks(this);
        when(mModelSelectorBtn.isVisible()).thenReturn(true);
        when(mTabGroupModelFilter.hasOtherRelatedTabs(any())).thenReturn(false);

        mActivity = Robolectric.setupActivity(Activity.class);
        mActivity.setTheme(org.chromium.chrome.R.style.Theme_BrowserUI);
    }

    @After
    public void tearDown() {
        if (mStripLayoutHelper != null) {
            mStripLayoutHelper.stopReorderModeForTesting();
            mStripLayoutHelper.setTabAtPositionForTesting(null);
        }
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
        mStripLayoutHelper.onSizeChanged(SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP);
        mStripLayoutHelper.updateLayout(TIMESTAMP);

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
        mStripLayoutHelper.onSizeChanged(SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP);
        mStripLayoutHelper.updateLayout(TIMESTAMP);

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
        mStripLayoutHelper.onSizeChanged(SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP);
        mStripLayoutHelper.updateLayout(TIMESTAMP);

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
        mStripLayoutHelper.onSizeChanged(SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP);
        mStripLayoutHelper.updateLayout(TIMESTAMP);

        assertTabStripAndOrder(getExpectedAccessibilityDescriptions(0));
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
    // Test show selected tab(non-last tab) close button.
    public void testTabSelected_SelectedTab_NonLastTab_ShowCloseBtn() {
        initializeTest(false, true, 3);
        mStripLayoutHelper.onSizeChanged(SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP);
        StripLayoutTab[] tabs = getMockedStripLayoutTabs(TAB_WIDTH_1);
        mStripLayoutHelper.setStripLayoutTabsForTest(tabs);

        mStripLayoutHelper.tabSelected(1, 3, 0, false);

        // Close btn is visible on the selected tab.
        Mockito.verify(tabs[3]).setCanShowCloseButton(true, false);
        // Close btn is hidden for the rest of tabs.
        Mockito.verify(tabs[0]).setCanShowCloseButton(false, false);
        Mockito.verify(tabs[1]).setCanShowCloseButton(false, false);
        Mockito.verify(tabs[2]).setCanShowCloseButton(false, false);
        Mockito.verify(tabs[4]).setCanShowCloseButton(false, false);
    }

    @Test
    // Test hide selected tab(non-last tab) close button.
    public void testTabSelected_SelectedTab_NonLastTab_HideCloseBtn() {
        // Set fourth tab as selected
        initializeTest(false, true, 3);
        mStripLayoutHelper.onSizeChanged(SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP);
        StripLayoutTab[] tabs = getMockedStripLayoutTabs(TAB_WIDTH_1);
        mStripLayoutHelper.setStripLayoutTabsForTest(tabs);
        when(tabs[3].getDrawX()).thenReturn(600.f);

        mStripLayoutHelper.tabSelected(1, 3, 0, false);

        // Close btn is hidden on the selected tab.
        Mockito.verify(tabs[3]).setCanShowCloseButton(false, false);
        // Close btn is hidden for the rest of tabs as well.
        Mockito.verify(tabs[0]).setCanShowCloseButton(false, false);
        Mockito.verify(tabs[1]).setCanShowCloseButton(false, false);
        Mockito.verify(tabs[2]).setCanShowCloseButton(false, false);
        Mockito.verify(tabs[4]).setCanShowCloseButton(false, false);
    }

    @Test
    // Test show selected tab(last tab) close button.
    public void testTabSelected_SelectedTab_LastTab_ShowCloseBtn() {
        initializeTest(false, true, 4);
        StripLayoutTab[] tabs = getMockedStripLayoutTabs(TAB_WIDTH_1);
        // Set mWidth value to 800.f
        mStripLayoutHelper.onSizeChanged(SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP);
        mStripLayoutHelper.getNewTabButton().setX(600.f);
        mStripLayoutHelper.setStripLayoutTabsForTest(tabs);

        // Act
        mStripLayoutHelper.tabSelected(1, 4, 0, false);

        // Assert
        // Close btn is visible on the selected last tab.
        Mockito.verify(tabs[4]).setCanShowCloseButton(true, false);
        // Close button is hidden for the rest of tabs.
        Mockito.verify(tabs[0]).setCanShowCloseButton(false, false);
        Mockito.verify(tabs[1]).setCanShowCloseButton(false, false);
        Mockito.verify(tabs[2]).setCanShowCloseButton(false, false);
        Mockito.verify(tabs[3]).setCanShowCloseButton(false, false);
    }

    @Test
    // Test hide selected tab(last tab) close button.
    public void testTabSelected_SelectedTab_LastTab_HideCloseBtn() {
        initializeTest(false, true, 4);
        StripLayoutTab[] tabs = getMockedStripLayoutTabs(TAB_WIDTH_1);
        // Set mWidth value to 800.f
        mStripLayoutHelper.onSizeChanged(SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP);
        mStripLayoutHelper.getNewTabButton().setX(NEW_TAB_BTN_X);
        // mNewTabButton.getX(700.f) + TabOverlapWidth(28.f) - tab.getWidth(140) + 1 to cross
        // threshold
        when(tabs[4].getDrawX()).thenReturn(589.f);
        mStripLayoutHelper.setStripLayoutTabsForTest(tabs);

        // Act
        mStripLayoutHelper.tabSelected(1, 4, 0, false);

        // Assert
        // Close btn is hidden on the selected last tab.
        Mockito.verify(tabs[4]).setCanShowCloseButton(false, false);
        // Close button is hidden for the rest of tabs.
        Mockito.verify(tabs[0]).setCanShowCloseButton(false, false);
        Mockito.verify(tabs[1]).setCanShowCloseButton(false, false);
        Mockito.verify(tabs[2]).setCanShowCloseButton(false, false);
        Mockito.verify(tabs[3]).setCanShowCloseButton(false, false);
    }

    @Test
    // Test hide selected tab(non-last tab) close button when there is no model selector button.
    public void testTabSelected_NonLastTab_Ltr_NoModelSelBtn_HideCloseBtn() {
        // Arrange
        when(mModelSelectorBtn.isVisible()).thenReturn(false);

        initializeTest(false, false, 3);
        StripLayoutTab[] tabs = getMockedStripLayoutTabs(TAB_WIDTH_1);
        // Set mWidth value to 800.f
        mStripLayoutHelper.onSizeChanged(SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP);
        mStripLayoutHelper.getNewTabButton().setX(NEW_TAB_BTN_X);
        float drawXToHideCloseBtn = SCREEN_WIDTH - CLOSE_BTN_VISIBILITY_THRESHOLD_END - TAB_WIDTH_1
                + TAB_OVERLAP_WIDTH + 1; // 1 is to cross threshold
        when(tabs[3].getDrawX()).thenReturn(drawXToHideCloseBtn);
        mStripLayoutHelper.setStripLayoutTabsForTest(tabs);

        // Act
        mStripLayoutHelper.tabSelected(1, 3, 0, false);

        // Assert
        // Close button is hidden for selected tab.
        Mockito.verify(tabs[3]).setCanShowCloseButton(false, false);
        // Close button is hidden for the rest of tabs as well.
        Mockito.verify(tabs[0]).setCanShowCloseButton(false, false);
        Mockito.verify(tabs[1]).setCanShowCloseButton(false, false);
        Mockito.verify(tabs[2]).setCanShowCloseButton(false, false);
        Mockito.verify(tabs[4]).setCanShowCloseButton(false, false);
    }

    @Test
    // Test show selected tab(non-last tab) close button when there is no model selector button.
    public void testTabSelected_NonLastTab_Ltr_NoModelSelBtn_ShowCloseBtn() {
        // Arrange
        when(mModelSelectorBtn.isVisible()).thenReturn(false);

        initializeTest(false, false, 3);
        StripLayoutTab[] tabs = getMockedStripLayoutTabs(TAB_WIDTH_1);
        // Set mWidth value to 800.f
        mStripLayoutHelper.onSizeChanged(SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP);
        mStripLayoutHelper.getNewTabButton().setX(NEW_TAB_BTN_X);
        mStripLayoutHelper.setStripLayoutTabsForTest(tabs);

        // Act
        mStripLayoutHelper.tabSelected(1, 3, 0, false);

        // Assert
        // Close button is visible for selected tab
        Mockito.verify(tabs[3]).setCanShowCloseButton(true, false);
        // Close button is hidden for the rest of tabs.
        Mockito.verify(tabs[0]).setCanShowCloseButton(false, false);
        Mockito.verify(tabs[1]).setCanShowCloseButton(false, false);
        Mockito.verify(tabs[2]).setCanShowCloseButton(false, false);
        Mockito.verify(tabs[4]).setCanShowCloseButton(false, false);
    }

    @Test
    // Test hide selected tab(last tab) close button when RTL.
    public void testTabSelected_SelectedTab_LastTab_Rtl_HideCloseBtn() {
        initializeTest(true, false, 4);
        StripLayoutTab[] tabs = getMockedStripLayoutTabs(TAB_WIDTH_1);
        // Set mWidth value to 800.f
        mStripLayoutHelper.onSizeChanged(SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP);
        mStripLayoutHelper.getNewTabButton().setX(NEW_TAB_BTN_X);
        mStripLayoutHelper.setStripLayoutTabsForTest(tabs);

        // Act
        mStripLayoutHelper.tabSelected(1, 4, 0, false);

        // Assert
        // Close button is hidden for the selected last tab.
        Mockito.verify(tabs[4]).setCanShowCloseButton(false, false);
        // Close button is hidden for the rest of tabs as well.
        Mockito.verify(tabs[0]).setCanShowCloseButton(false, false);
        Mockito.verify(tabs[1]).setCanShowCloseButton(false, false);
        Mockito.verify(tabs[2]).setCanShowCloseButton(false, false);
        Mockito.verify(tabs[3]).setCanShowCloseButton(false, false);
    }

    @Test
    // Test show selected tab(last tab) close button when RTL.
    public void testTabSelected_SelectedTab_LastTab_Rtl_ShowCloseBtn() {
        initializeTest(true, false, 4);
        StripLayoutTab[] tabs = getMockedStripLayoutTabs(TAB_WIDTH_1);
        // Set mWidth value to 800.f
        mStripLayoutHelper.onSizeChanged(SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP);
        mStripLayoutHelper.getNewTabButton().setX(600.f);
        // newTabBtn.X(600.f) + newTabBtn.width(38.f) - mTabOverlapWidth(28.f) + 1
        when(tabs[4].getDrawX()).thenReturn(611.f);
        mStripLayoutHelper.setStripLayoutTabsForTest(tabs);

        // Act
        mStripLayoutHelper.tabSelected(1, 4, 0, false);

        // Close button is visible for selected last tab.
        Mockito.verify(tabs[4]).setCanShowCloseButton(true, false);
        // Assert
        // Close button is hidden for the rest of tabs.
        Mockito.verify(tabs[0]).setCanShowCloseButton(false, false);
        Mockito.verify(tabs[1]).setCanShowCloseButton(false, false);
        Mockito.verify(tabs[2]).setCanShowCloseButton(false, false);
        Mockito.verify(tabs[3]).setCanShowCloseButton(false, false);
    }

    @Test
    // Test hide selected tab(non-last tab) close button when RTL.
    public void testTabSelected_SelectedTab_NonLastTab_Rtl_HideCloseBtn() {
        initializeTest(true, false, 3);
        StripLayoutTab[] tabs = getMockedStripLayoutTabs(TAB_WIDTH_1);

        // Set mWidth value to 800.f
        mStripLayoutHelper.onSizeChanged(SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP);
        mStripLayoutHelper.getNewTabButton().setX(NEW_TAB_BTN_X);
        mStripLayoutHelper.setStripLayoutTabsForTest(tabs);

        // Act
        mStripLayoutHelper.tabSelected(1, 3, 0, false);

        // Assert
        // Close btn is hidden for selected tab.
        Mockito.verify(tabs[3]).setCanShowCloseButton(false, false);
        // Close btn is hidden for all the rest of tabs as well.
        Mockito.verify(tabs[0]).setCanShowCloseButton(false, false);
        Mockito.verify(tabs[1]).setCanShowCloseButton(false, false);
        Mockito.verify(tabs[2]).setCanShowCloseButton(false, false);
        Mockito.verify(tabs[4]).setCanShowCloseButton(false, false);
    }

    @Test
    // Test show selected tab(non-last tab) close button when RTL.
    public void testTabSelected_SelectedTab_NonLastTab_Rtl_ShowCloseBtn() {
        initializeTest(true, false, 3);
        StripLayoutTab[] tabs = getMockedStripLayoutTabs(TAB_WIDTH_1);
        // Set mWidth value to 800.f.
        mStripLayoutHelper.onSizeChanged(SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP);
        mStripLayoutHelper.getNewTabButton().setX(NEW_TAB_BTN_X);
        // getCloseBtnVisibilityThreshold(120.f) - mTabOverlapWidth(28.f) + 1
        when(tabs[3].getDrawX()).thenReturn(93.f);
        mStripLayoutHelper.setStripLayoutTabsForTest(tabs);

        // Act
        mStripLayoutHelper.tabSelected(1, 3, 0, false);

        // Assert
        // Close button is visible for the selected tab.
        Mockito.verify(tabs[3]).setCanShowCloseButton(true, false);
        // Close button is hidden for the rest of tabs.
        Mockito.verify(tabs[0]).setCanShowCloseButton(false, false);
        Mockito.verify(tabs[1]).setCanShowCloseButton(false, false);
        Mockito.verify(tabs[2]).setCanShowCloseButton(false, false);
        Mockito.verify(tabs[4]).setCanShowCloseButton(false, false);
    }

    @Test
    @Feature("Tab Strip Redesign")
    public void testUpdateDividers_WithTabSelected() {
        // Setup with 5 tabs. Select tab 2.
        initializeTest(false, false, 2);
        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabs();

        // Trigger update to set divider values.
        mStripLayoutHelper.updateLayout(TIMESTAMP);

        // Verify tabs 2 and 3's start dividers are hidden due to selection.
        assertFalse(
                "First start divider should always be hidden.", tabs[0].isStartDividerVisible());
        assertTrue("Start divider should be visible.", tabs[1].isStartDividerVisible());
        assertFalse("Start divider is for selected tab and should be hidden.",
                tabs[2].isStartDividerVisible());
        assertFalse("Start divider is adjacent to selected tab and should be hidden.",
                tabs[3].isStartDividerVisible());
        assertTrue("Start divider should be visible.", tabs[4].isStartDividerVisible());

        // Verify only last tab's end divider is visible.
        assertFalse("End divider should be hidden.", tabs[0].isEndDividerVisible());
        assertFalse("End divider should be hidden.", tabs[1].isEndDividerVisible());
        assertFalse("End divider should be hidden.", tabs[2].isEndDividerVisible());
        assertFalse("End divider should be hidden.", tabs[3].isEndDividerVisible());
        assertTrue("End divider should be visible.", tabs[4].isEndDividerVisible());
    }

    @Test
    @Feature("Tab Strip Redesign")
    public void testUpdateDividers_InReorderMode() {
        // Setup with 5 tabs. Select 2nd tab.
        initializeTest(false, false, true, 1, 5);
        mStripLayoutHelper.onSizeChanged(SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP);

        // Start reorder mode at 2nd tab
        mStripLayoutHelper.startReorderModeAtIndexForTesting(1);
        // Trigger update to set divider values.
        mStripLayoutHelper.updateLayout(TIMESTAMP);

        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabs();
        // Verify only 4th and 5th tab's start divider is visible.
        assertFalse(
                "First start divider should always be hidden.", tabs[0].isStartDividerVisible());
        assertFalse("Start divider should be hidden.", tabs[1].isStartDividerVisible());
        assertFalse("Start divider should be hidden.", tabs[2].isStartDividerVisible());
        assertTrue("Start divider should be hidden.", tabs[3].isStartDividerVisible());
        assertTrue("Start divider should be visible.", tabs[4].isStartDividerVisible());

        // Verify end divider visible only for 5th tab.
        assertFalse("End divider should be hidden.", tabs[0].isEndDividerVisible());
        assertFalse("End divider should be hidden.", tabs[1].isEndDividerVisible());
        assertFalse("End divider should be hidden.", tabs[2].isEndDividerVisible());
        assertFalse("End divider should be hidden.", tabs[3].isEndDividerVisible());
        assertTrue("End divider should be visible.", tabs[4].isEndDividerVisible());
    }

    @Test
    @Feature("Tab Strip Redesign")
    public void testUpdateDividers_InReorderModeWithTabGroups() {
        // Setup with 5 tabs. Select 2nd tab.
        initializeTest(false, false, true, 1, 5);
        mStripLayoutHelper.onSizeChanged(SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP);
        // group 2nd and 3rd tab.
        groupTabs(1, 3);

        // Start reorder mode at 2nd tab
        mStripLayoutHelper.startReorderModeAtIndexForTesting(1);
        // Trigger update to set divider values.
        mStripLayoutHelper.updateLayout(TIMESTAMP);

        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabs();
        // Verify only 4th and 5th tab's start divider is visible.
        assertFalse(
                "First start divider should always be hidden.", tabs[0].isStartDividerVisible());
        assertFalse("Start divider should be hidden.", tabs[1].isStartDividerVisible());
        assertFalse("Start divider should be hidden.", tabs[2].isStartDividerVisible());
        assertTrue("Start divider should be hidden.", tabs[3].isStartDividerVisible());
        assertTrue("Start divider should be visible.", tabs[4].isStartDividerVisible());

        // Verify end divider visible for 1st and 5th tab.
        assertTrue("End divider should be visible.", tabs[0].isEndDividerVisible());
        assertFalse("End divider should be hidden.", tabs[1].isEndDividerVisible());
        assertFalse("End divider should be hidden.", tabs[2].isEndDividerVisible());
        assertFalse("End divider should be hidden.", tabs[3].isEndDividerVisible());
        assertTrue("End divider should be visible.", tabs[4].isEndDividerVisible());
    }

    @Test
    @Feature("Tab Strip Redesign")
    public void testUpdateForegroundTabContainers() {
        // Setup with 5 tabs. Select tab 2.
        initializeTest(false, false, 2);
        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabs();

        // Trigger update to set foreground container visibility.
        mStripLayoutHelper.updateLayout(TIMESTAMP);

        // Verify tabs 2 and 3's dividers are hidden due to selection.
        float hiddenOpacity = StripLayoutHelper.TAB_OPACITY_HIDDEN;
        float visibleOpacity = StripLayoutHelper.TAB_OPACITY_VISIBLE_FOREGROUND;
        assertEquals("Tab is not selected and container should not be visible.", hiddenOpacity,
                tabs[0].getContainerOpacity(), EPSILON);
        assertEquals("Tab is not selected and container should not be visible.", hiddenOpacity,
                tabs[1].getContainerOpacity(), EPSILON);
        assertEquals("Tab is selected and container should be visible.", visibleOpacity,
                tabs[2].getContainerOpacity(), EPSILON);
        assertEquals("Tab is not selected and container should not be visible.", hiddenOpacity,
                tabs[3].getContainerOpacity(), EPSILON);
        assertEquals("Tab is not selected and container should not be visible.", hiddenOpacity,
                tabs[4].getContainerOpacity(), EPSILON);
    }

    @Test
    @Feature("Tab Strip Redesign")
    public void testNewTabButtonPosition_Folio() {
        // Setup
        TabManagementFieldTrial.TAB_STRIP_REDESIGN_ENABLE_FOLIO.setForTesting(true);
        int tabCount = 4;
        initializeTest(false, false, false, 3, tabCount);
        mStripLayoutHelper.onSizeChanged(SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP);

        // Set New tab button position.
        mStripLayoutHelper.updateLayout(TIMESTAMP);

        // Verify new tab button position.
        // stripWidth(800) - stripEndPadding(10) - NtbWidth(36) = 754
        assertEquals("New tab button position is not as expected", 754.f,
                mStripLayoutHelper.getNewTabButton().getX(), EPSILON);
    }

    @Test
    @Feature("Tab Strip Redesign")
    public void testNewTabButtonPosition_Detached() {
        // Setup
        TabManagementFieldTrial.TAB_STRIP_REDESIGN_ENABLE_DETACHED.setForTesting(true);
        int tabCount = 4;
        initializeTest(false, false, false, 3, tabCount);
        mStripLayoutHelper.onSizeChanged(SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP);

        // Set New tab button position.
        mStripLayoutHelper.updateLayout(TIMESTAMP);

        // Verify new tab button position.
        // stripWidth(800) - stripEndPadding(9) - NtbWidth(38) = 753
        assertEquals("New tab button position is not as expected", 753.f,
                mStripLayoutHelper.getNewTabButton().getX(), EPSILON);
    }

    @Test
    @Feature("Tab Strip Redesign")
    public void testNewTabButtonPosition_Rtl_Folio() {
        // Setup
        TabManagementFieldTrial.TAB_STRIP_REDESIGN_ENABLE_FOLIO.setForTesting(true);
        int tabCount = 4;
        initializeTest(true, false, false, 3, tabCount);

        // Set New tab button position.
        mStripLayoutHelper.updateLayout(TIMESTAMP);

        // Verify new tab button position.
        assertEquals("New tab button position is not as expected", BUTTON_END_PADDING_FOLIO,
                mStripLayoutHelper.getNewTabButton().getX(), EPSILON);
    }

    @Test
    @Feature("Tab Strip Redesign")
    public void testNewTabButtonPosition_Rtl_Detached() {
        // Setup
        TabManagementFieldTrial.TAB_STRIP_REDESIGN_ENABLE_DETACHED.setForTesting(true);
        int tabCount = 4;
        initializeTest(true, false, false, 3, tabCount);

        // Set New tab button position.
        mStripLayoutHelper.updateLayout(TIMESTAMP);

        // Verify new tab button position.
        assertEquals("New tab button position is not as expected", BUTTON_END_PADDING_DETACHED,
                mStripLayoutHelper.getNewTabButton().getX(), EPSILON);
    }

    @Test
    @Feature("Tab Strip Redesign")
    public void testNewTabButtonPosition_Incognito_Folio() {
        // Setup
        TabManagementFieldTrial.TAB_STRIP_REDESIGN_ENABLE_FOLIO.setForTesting(true);
        int tabCount = 4;
        when(mModelSelectorBtn.getWidth()).thenReturn(MODEL_SELECTOR_BUTTON_BG_WIDTH_FOLIO);
        initializeTest(false, true, false, 3, tabCount);
        mStripLayoutHelper.onSizeChanged(SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP);

        // Set New tab button position.
        mStripLayoutHelper.setEndMargin(
                MODEL_SELECTOR_BUTTON_BG_WIDTH_FOLIO + BUTTON_END_PADDING_FOLIO, true);
        mStripLayoutHelper.updateLayout(TIMESTAMP);

        // Verify new tab button position.
        // stripWidth(800) - buttonEndPadding(10) - NtbWidth(36) - NTB_With_MSB_Padding(8) -
        // MSBWidth(36) = 710
        assertEquals("New tab button position is not as expected", 710.f,
                mStripLayoutHelper.getNewTabButton().getX(), EPSILON);
    }

    @Test
    @Feature("Tab Strip Redesign")
    public void testNewTabButtonPosition_Incognito_Detached() {
        // Setup
        TabManagementFieldTrial.TAB_STRIP_REDESIGN_ENABLE_DETACHED.setForTesting(true);
        int tabCount = 4;
        when(mModelSelectorBtn.getWidth()).thenReturn(MODEL_SELECTOR_BUTTON_BG_WIDTH_DETACHED);
        initializeTest(false, true, false, 3, tabCount);
        mStripLayoutHelper.onSizeChanged(SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP);

        // Set New tab button position.
        mStripLayoutHelper.setEndMargin(
                MODEL_SELECTOR_BUTTON_BG_WIDTH_DETACHED + BUTTON_END_PADDING_DETACHED, true);
        mStripLayoutHelper.updateLayout(TIMESTAMP);

        // Verify new tab button position.
        // stripWidth(800) - buttonEndPadding(9) - NtbWidth(38) - NTB_With_MSB_Padding(8) -
        // MSBWidth(38) = 707
        assertEquals("New tab button position is not as expected", 707.f,
                mStripLayoutHelper.getNewTabButton().getX(), EPSILON);
    }

    @Test
    @Feature("Tab Strip Redesign")
    public void testNewTabButtonPosition_Rtl_Incognito_Folio() {
        // Setup
        int tabCount = 4;
        TabManagementFieldTrial.TAB_STRIP_REDESIGN_ENABLE_FOLIO.setForTesting(true);
        initializeTest(true, true, false, 3, tabCount);
        when(mModelSelectorBtn.getWidth()).thenReturn(MODEL_SELECTOR_BUTTON_BG_WIDTH_FOLIO);
        mStripLayoutHelper.onSizeChanged(SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP);

        // Set New tab button position.
        mStripLayoutHelper.setEndMargin(
                MODEL_SELECTOR_BUTTON_BG_WIDTH_FOLIO + BUTTON_END_PADDING_FOLIO, true);
        mStripLayoutHelper.updateLayout(TIMESTAMP);

        // Verify new tab button position.
        // buttonEndPadding(10) + MsbWidth(36) + NTB_With_MSB_Padding(8) = 54
        assertEquals("New tab button position is not as expected", 54.f,
                mStripLayoutHelper.getNewTabButton().getX(), EPSILON);
    }

    @Test
    @Feature("Tab Strip Redesign")
    public void testNewTabButtonPosition_Rtl_Incognito_Detached() {
        // Setup
        int tabCount = 4;
        TabManagementFieldTrial.TAB_STRIP_REDESIGN_ENABLE_DETACHED.setForTesting(true);
        initializeTest(true, true, false, 3, tabCount);
        when(mModelSelectorBtn.getWidth()).thenReturn(MODEL_SELECTOR_BUTTON_BG_WIDTH_DETACHED);
        mStripLayoutHelper.onSizeChanged(SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP);

        // Set New tab button position.
        mStripLayoutHelper.setEndMargin(
                MODEL_SELECTOR_BUTTON_BG_WIDTH_DETACHED + BUTTON_END_PADDING_DETACHED, true);
        mStripLayoutHelper.updateLayout(TIMESTAMP);

        // Verify new tab button position.
        // buttonEndPadding(9) + MsbWidth(38) + NTB_With_MSB_Padding(8) = 55
        assertEquals("New tab button position is not as expected", 55.f,
                mStripLayoutHelper.getNewTabButton().getX(), EPSILON);
    }

    @Test
    @Feature("Tab Strip Redesign")
    public void testNewTabButtonPosition_NotAnchored() {
        // Setup
        TabManagementFieldTrial.TAB_STRIP_REDESIGN_ENABLE_FOLIO.setForTesting(true);
        TabUiFeatureUtilities.TAB_STRIP_REDESIGN_DISABLE_NTB_ANCHOR.setForTesting(true);
        int tabCount = 1;
        initializeTest(false, false, false, 0, tabCount);
        mStripLayoutHelper.onSizeChanged(SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP);
        mStripLayoutHelper.updateLayout(TIMESTAMP);

        // Verify new tab button position.
        // tabWidth(237) + tabOverLapWidth(28) = 265(Same for both TSR arms)
        assertEquals("New tab button position is not as expected", 265.f,
                mStripLayoutHelper.getNewTabButton().getX(), EPSILON);
    }

    @Test
    @Feature("Tab Strip Redesign")
    public void testNewTabButtonPosition_NotAnchored_Rtl() {
        // Setup
        TabManagementFieldTrial.TAB_STRIP_REDESIGN_ENABLE_FOLIO.setForTesting(true);
        TabUiFeatureUtilities.TAB_STRIP_REDESIGN_DISABLE_NTB_ANCHOR.setForTesting(true);
        int tabCount = 1;
        initializeTest(true, false, false, 0, tabCount);
        mStripLayoutHelper.onSizeChanged(SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP);
        mStripLayoutHelper.updateLayout(TIMESTAMP);

        // Verify new tab button position.
        // stripWidth(800) - tabWidth(237) - tabOverLapWidth(28) - NtbWidth(36) = 499
        assertEquals("New tab button position is not as expected", 499,
                mStripLayoutHelper.getNewTabButton().getX(), EPSILON);
    }

    @Test
    public void testScrollOffset_OnResume_StartOnLeft_SelectedRightmostTab() {
        // Arrange: Initialize tabs with last tab selected.
        initializeTest(false, true, false, 9, 10);
        StripLayoutTab[] tabs = getMockedStripLayoutTabs(TAB_WIDTH_MEDIUM, 150.f, 10);
        mStripLayoutHelper.setStripLayoutTabsForTest(tabs);

        // Set screen width to 800dp.
        mStripLayoutHelper.onSizeChanged(SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP);

        mStripLayoutHelper.scrollTabToView(TIMESTAMP, false);

        int expectedFinalX = -148; // delta(optimalRight(-120) - scrollOffset(0)
                                   // - tabOverlapWidth(28)) + scrollOffset(0)
        assertEquals(expectedFinalX, mStripLayoutHelper.getScroller().getFinalX());
    }

    @Test
    public void testScrollOffset_OnResume_StartOnLeft_NoModelSelBtn_SelectedRightmostTab() {
        // Arrange: Initialize tabs with last tab selected.
        when(mModelSelectorBtn.isVisible()).thenReturn(false);
        initializeTest(false, true, false, 9, 10);
        StripLayoutTab[] tabs = getMockedStripLayoutTabs(TAB_WIDTH_MEDIUM, 150.f, 10);
        mStripLayoutHelper.setStripLayoutTabsForTest(tabs);

        // Set screen width to 800dp.
        mStripLayoutHelper.onSizeChanged(SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP);

        mStripLayoutHelper.scrollTabToView(TIMESTAMP, false);

        System.out.println();
        int expectedFinalX = -100; // delta(optimalRight(-72) - scrollOffset(0)
                                   // - tabOverlapWidth(28)) + scrollOffset(0)
        assertEquals(expectedFinalX, mStripLayoutHelper.getScroller().getFinalX());
    }

    @Test
    public void testScrollOffset_OnResume_StartOnRight_SelectedLeftmostTab() {
        // Arrange: Initialize tabs with first tab selected.
        initializeTest(false, true, false, 0, 10);
        StripLayoutTab[] tabs = getMockedStripLayoutTabs(TAB_WIDTH_MEDIUM, 150.f, 10);
        mStripLayoutHelper.setStripLayoutTabsForTest(tabs);
        mStripLayoutHelper.testSetScrollOffset(-1200);

        // Set screen width to 800dp.
        mStripLayoutHelper.onSizeChanged(SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP);

        mStripLayoutHelper.scrollTabToView(TIMESTAMP, false);

        int expectedFinalX = 0; // optimalLeft(0) - scrollOffset(-1200)) + scrollOffset(-1200)
        assertEquals(expectedFinalX, mStripLayoutHelper.getScroller().getFinalX());
    }

    @Test
    public void testScrollOffset_OnResume_StartOnRight_NoModelSelBtn_SelectedRightmostTab() {
        // Arrange: Initialize tabs with first tab selected.
        when(mModelSelectorBtn.isVisible()).thenReturn(false);
        initializeTest(false, true, false, 0, 10);
        StripLayoutTab[] tabs = getMockedStripLayoutTabs(TAB_WIDTH_MEDIUM, 150.f, 10);
        mStripLayoutHelper.setStripLayoutTabsForTest(tabs);
        mStripLayoutHelper.testSetScrollOffset(-1200);

        // Set screen width to 800dp.
        mStripLayoutHelper.onSizeChanged(SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP);

        mStripLayoutHelper.scrollTabToView(TIMESTAMP, false);

        int expectedFinalX = 0; // optimalLeft(0) - scrollOffset(-1200)) + scrollOffset(-1200)
        assertEquals(expectedFinalX, mStripLayoutHelper.getScroller().getFinalX());
    }

    @Test
    public void testScrollOffset_OnOrientationChange_SelectedTabVisible() {
        // Arrange: Initialize tabs with last tab selected.
        when(mModelSelectorBtn.isVisible()).thenReturn(false);
        initializeTest(false, true, false, 9, 10);
        StripLayoutTab[] tabs = getMockedStripLayoutTabs(TAB_WIDTH_MEDIUM, 150.f, 10);
        when(tabs[9].isVisible()).thenReturn(true);
        mStripLayoutHelper.setStripLayoutTabsForTest(tabs);
        mStripLayoutHelper.testSetScrollOffset(1000);

        // Set screen width to 1200 to start.
        mStripLayoutHelper.onSizeChanged(SCREEN_WIDTH_LANDSCAPE, SCREEN_HEIGHT, false, TIMESTAMP);

        // Assert: finalX value before orientation change.
        int initialFinalX = -720;
        assertEquals(initialFinalX, mStripLayoutHelper.getScroller().getFinalX());

        // Act: change orientation.
        when(tabs[9].getDrawX()).thenReturn(-1.f);
        mStripLayoutHelper.onSizeChanged(SCREEN_WIDTH, SCREEN_HEIGHT, true, TIMESTAMP);

        // Assert: finalX value after orientation change.
        int expectedFinalX = -100; // delta(optimalRight(-72) - tabOverlapWidth(28)) -
                                   // scrollOffset(1000) + scrollOffset(1000)
        assertEquals(expectedFinalX, mStripLayoutHelper.getScroller().getFinalX());
    }

    @Test
    public void testScrollOffset_OnOrientationChange_SelectedTabNotVisible() {
        // Arrange: Initialize tabs with last tab selected.
        when(mModelSelectorBtn.isVisible()).thenReturn(false);
        initializeTest(false, true, false, 9, 10);
        StripLayoutTab[] tabs = getMockedStripLayoutTabs(TAB_WIDTH_MEDIUM, 150.f, 10);
        when(tabs[9].isVisible()).thenReturn(false);
        mStripLayoutHelper.setStripLayoutTabsForTest(tabs);
        mStripLayoutHelper.testSetScrollOffset(1000);

        // Set screen width to 1200 to start
        mStripLayoutHelper.onSizeChanged(SCREEN_WIDTH_LANDSCAPE, SCREEN_HEIGHT, false, TIMESTAMP);

        // Assert: finalX value before orientation change.
        int initialFinalX = -720;
        assertEquals(initialFinalX, mStripLayoutHelper.getScroller().getFinalX());

        // Act: change orientation.
        when(tabs[9].getDrawX()).thenReturn(-1.f);
        mStripLayoutHelper.onSizeChanged(SCREEN_WIDTH, SCREEN_HEIGHT, true, TIMESTAMP);

        // Assert: finalX value remains the same on orientation change.
        assertEquals(initialFinalX, mStripLayoutHelper.getScroller().getFinalX());
    }

    @Test
    public void testTabSelected_AfterTabClose_SkipsAutoScroll() {
        initializeTest(false, true, 3);
        StripLayoutTab[] tabs = getMockedStripLayoutTabs(TAB_WIDTH_MEDIUM);
        mStripLayoutHelper.setStripLayoutTabsForTest(tabs);
        // Set initial scroller position to 1000.
        mStripLayoutHelper.getScroller().setFinalX(1000);

        // Act: close a non selected tab.
        mStripLayoutHelper.handleCloseButtonClick(tabs[1], TIMESTAMP);

        // Assert: scroller position is not modified.
        assertEquals(1000, mStripLayoutHelper.getScroller().getFinalX());
    }

    @Test
    public void testTabSelected_AfterSelectedTabClose_SkipsAutoScroll() {
        initializeTest(false, true, 3);
        StripLayoutTab[] tabs = getMockedStripLayoutTabs(TAB_WIDTH_MEDIUM);
        mStripLayoutHelper.setStripLayoutTabsForTest(tabs);
        // Set initial scroller position to 1000.
        mStripLayoutHelper.getScroller().setFinalX(1000);

        // Act: close the selected tab.
        mStripLayoutHelper.handleCloseButtonClick(tabs[3], TIMESTAMP);

        // Assert: scroller position is not modified.
        assertEquals(1000, mStripLayoutHelper.getScroller().getFinalX());
    }

    @Test
    public void testTabCreated_Animation() {
        // Initialize with default amount of tabs. Clear any animations.
        initializeTest(false, false, 3);
        mStripLayoutHelper.finishAnimationsAndPushTabUpdates();
        assertNull("Animation should not be running.",
                mStripLayoutHelper.getRunningAnimatorForTesting());

        // Act: Create new tab in model and trigger update in tab strip.
        mModel.addTab("new tab");
        mStripLayoutHelper.tabCreated(TIMESTAMP, 5, 3, true, false, false);

        // Assert: Animation is running.
        assertNotNull(
                "Animation should running.", mStripLayoutHelper.getRunningAnimatorForTesting());
    }

    @Test
    public void testTabCreated_RestoredTab_SkipsAutoscroll() {
        initializeTest(false, true, 3);
        StripLayoutTab[] tabs = getMockedStripLayoutTabs(TAB_WIDTH_MEDIUM);
        mStripLayoutHelper.setStripLayoutTabsForTest(tabs);
        // Set initial scroller position to 1000.
        mStripLayoutHelper.getScroller().setFinalX(1000);

        // Act: Tab was restored after undoing a tab closure.
        boolean closureCancelled = true;
        mStripLayoutHelper.tabCreated(TIMESTAMP, 6, 3, false, closureCancelled, false);

        // Assert: scroller position is not modified.
        assertEquals(1000, mStripLayoutHelper.getScroller().getFinalX());
    }

    @Test
    public void testTabCreated_NonRestoredTab_SkipsAutoscroll() {
        initializeTest(false, true, 3);
        StripLayoutTab[] tabs = getMockedStripLayoutTabs(TAB_WIDTH_MEDIUM);
        mStripLayoutHelper.setStripLayoutTabsForTest(tabs);
        // Set initial scroller position to 1000.
        mStripLayoutHelper.getScroller().setFinalX(1000);

        // Act: Tab was restored after undoing a tab closure.
        boolean closureCancelled = false;
        mStripLayoutHelper.tabCreated(TIMESTAMP, 6, 3, false, closureCancelled, false);

        // Assert: scroller position is modified.
        assertNotEquals(1000, mStripLayoutHelper.getScroller().getFinalX());
    }

    @Test
    public void testTabCreated_BringSelectedTabToVisibleArea_StartupRestoredUnselectedTab() {
        initializeTest(false, false, true, 1, 11);
        mStripLayoutHelper.onSizeChanged(SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP);
        // Set initial scroller position to -500.
        mStripLayoutHelper.testSetScrollOffset(-500);
        mStripLayoutHelper.updateLayout(TIMESTAMP);

        // Act: Tab was restored during startup.
        boolean selected = false;
        boolean onStartup = true;
        mStripLayoutHelper.tabCreated(TIMESTAMP, 12, 12, selected, false, onStartup);

        // Assert: We don't scroll to the created tab. The selected tab is not already visible, so
        // we scroll to it. Offset = -(1 tab width) = -80.
        float expectedOffset = -80f;
        assertEquals("We should scroll to the selected tab", expectedOffset,
                mStripLayoutHelper.getScrollOffset(), EPSILON);
    }

    @Test
    public void testScrollDuration() {
        initializeTest(false, true, 3);

        // Act: Set scroll offset greater than -960.
        mStripLayoutHelper.testSetScrollOffset(-800);

        // Assert: Expand duration is 250.
        assertEquals(mStripLayoutHelper.getExpandDurationForTesting(), 250);
    }

    @Test
    public void testScrollDuration_Medium() {
        initializeTest(false, true, false, 3, 12);

        // Act: Set scroll offset between -960 and -1920.
        mStripLayoutHelper.testSetScrollOffset(-1000);

        // Assert: Expand duration is 350.
        assertEquals(mStripLayoutHelper.getExpandDurationForTesting(), 350);
    }

    @Test
    public void testScrollDuration_Large() {
        initializeTest(false, true, false, 3, 24);

        // Act: Set scroll offset less than -1920
        mStripLayoutHelper.testSetScrollOffset(-2000);

        // Assert: Expand duration is 450.
        assertEquals(mStripLayoutHelper.getExpandDurationForTesting(), 450);
    }

    @Test
    public void testOnDown_OnNewTabButton() {
        // Initialize.
        initializeTest(false, false, 0);

        // Set new tab button location and dimensions.
        mStripLayoutHelper.getNewTabButton().setX(NEW_TAB_BTN_X);
        mStripLayoutHelper.getNewTabButton().setY(NEW_TAB_BTN_Y);
        mStripLayoutHelper.getNewTabButton().setWidth(NEW_TAB_BTN_WIDTH);
        mStripLayoutHelper.getNewTabButton().setHeight(NEW_TAB_BTN_HEIGHT);

        // Press down on new tab button.
        // CenterX = getX() + (getWidth() / 2) = 700 + (100 / 2) = 750
        // CenterY = getY() + (getHeight() / 2) = 1400 + (100 / 2) = 1450
        mStripLayoutHelper.onDown(TIMESTAMP, 750f, 1450f, false, 0);

        // Verify.
        assertTrue("New tab button should be pressed.",
                mStripLayoutHelper.getNewTabButton().isPressed());
        assertNull("Should not set an interacting tab when pressing down on new tab button.",
                mStripLayoutHelper.getInteractingTab());
        assertFalse("Should not start reorder mode when pressing down on new tab button.",
                mStripLayoutHelper.getInReorderModeForTesting());
    }

    @Test
    public void testOnDown_OnTab() {
        // Initialize.
        initializeTest(false, false, 0);
        StripLayoutTab[] tabs = getMockedStripLayoutTabs(150f);
        mStripLayoutHelper.setStripLayoutTabsForTest(tabs);

        // Press down on second tab.
        when(tabs[1].checkCloseHitTest(anyFloat(), anyFloat())).thenReturn(false);
        mStripLayoutHelper.setTabAtPositionForTesting(tabs[1]);
        mStripLayoutHelper.onDown(TIMESTAMP, 150f, 0f, false, 0);

        // Verify.
        assertFalse("New tab button should not be pressed.",
                mStripLayoutHelper.getNewTabButton().isPressed());
        assertEquals("Second tab should be interacting tab.", tabs[1],
                mStripLayoutHelper.getInteractingTab());
        assertFalse("Should not start reorder mode when pressing down on tab without mouse.",
                mStripLayoutHelper.getInReorderModeForTesting());
        verify(tabs[1], never()).setClosePressed(anyBoolean());
    }

    @Test
    public void testOnDown_OnTab_WithMouse() {
        // Initialize.
        initializeTest(false, false, 0);
        StripLayoutTab[] tabs = getMockedStripLayoutTabs(150f);
        mStripLayoutHelper.setStripLayoutTabsForTest(tabs);

        // Press down on second tab with mouse.
        when(tabs[1].checkCloseHitTest(anyFloat(), anyFloat())).thenReturn(false);
        mStripLayoutHelper.setTabAtPositionForTesting(tabs[1]);
        mStripLayoutHelper.onDown(TIMESTAMP, 150f, 0f, true, 0);

        // Verify.
        assertFalse("New tab button should not be pressed.",
                mStripLayoutHelper.getNewTabButton().isPressed());
        assertEquals("Second tab should be interacting tab.", tabs[1],
                mStripLayoutHelper.getInteractingTab());
        assertTrue("Should start reorder mode when pressing down on tab with mouse.",
                mStripLayoutHelper.getInReorderModeForTesting());
        verify(tabs[1], never()).setClosePressed(anyBoolean());
    }

    @Test
    public void testOnDown_OnTabCloseButton() {
        // Initialize.
        initializeTest(false, false, 0);
        StripLayoutTab[] tabs = getMockedStripLayoutTabs(150f);
        mStripLayoutHelper.setStripLayoutTabsForTest(tabs);

        // Press down on second tab's close button.
        when(tabs[1].checkCloseHitTest(anyFloat(), anyFloat())).thenReturn(true);
        mStripLayoutHelper.setTabAtPositionForTesting(tabs[1]);
        mStripLayoutHelper.onDown(TIMESTAMP, 150f, 0f, false, 0);

        // Verify.
        assertFalse("New tab button should not be pressed.",
                mStripLayoutHelper.getNewTabButton().isPressed());
        assertEquals("Second tab should be interacting tab.", tabs[1],
                mStripLayoutHelper.getInteractingTab());
        assertFalse("Should not start reorder mode from close button.",
                mStripLayoutHelper.getInReorderModeForTesting());
        verify(tabs[1]).setClosePressed(eq(true));
    }

    @Test
    public void testOnDown_OnTabCloseButton_WithMouse() {
        // Initialize.
        initializeTest(false, false, 0);
        StripLayoutTab[] tabs = getMockedStripLayoutTabs(150f);
        mStripLayoutHelper.setStripLayoutTabsForTest(tabs);

        // Press down on second tab's close button with mouse.
        when(tabs[1].checkCloseHitTest(anyFloat(), anyFloat())).thenReturn(true);
        mStripLayoutHelper.setTabAtPositionForTesting(tabs[1]);
        mStripLayoutHelper.onDown(TIMESTAMP, 150f, 0f, true, 0);

        // Verify.
        assertFalse("New tab button should not be pressed.",
                mStripLayoutHelper.getNewTabButton().isPressed());
        assertEquals("Second tab should be interacting tab.", tabs[1],
                mStripLayoutHelper.getInteractingTab());
        assertFalse("Should not start reorder mode from close button.",
                mStripLayoutHelper.getInReorderModeForTesting());
        verify(tabs[1]).setClosePressed(eq(true));
    }

    @Test
    public void testOnDown_WhileScrolling() {
        // Initialize and assert scroller is finished.
        initializeTest(false, false, 0);
        StripLayoutTab[] tabs = getMockedStripLayoutTabs(150f);
        mStripLayoutHelper.setStripLayoutTabsForTest(tabs);
        assertTrue("Scroller should be finished right after initializing.",
                mStripLayoutHelper.getScroller().isFinished());

        // Start scroll and assert scroller is not finished.
        mStripLayoutHelper.getScroller().startScroll(0, 0, 0, 0, TIMESTAMP, 1000);
        assertFalse("Scroller should not be finished after starting scroll.",
                mStripLayoutHelper.getScroller().isFinished());

        // Press down on second tab and assert scroller is finished.
        mStripLayoutHelper.setTabAtPositionForTesting(tabs[1]);
        mStripLayoutHelper.onDown(TIMESTAMP, 150f, 0f, false, 0);
        assertFalse("New tab button should not be pressed.",
                mStripLayoutHelper.getNewTabButton().isPressed());
        assertNull("Should not set an interacting tab when pressing down to stop scrolling.",
                mStripLayoutHelper.getInteractingTab());
        assertTrue("Scroller should be force finished after pressing down on strip.",
                mStripLayoutHelper.getScroller().isFinished());
    }

    @Test
    public void testOnLongPress_OnTab() {
        // Initialize.
        initializeTest(false, false, 0);
        StripLayoutTab[] tabs = getMockedStripLayoutTabs(150f);
        mStripLayoutHelper.setStripLayoutTabsForTest(tabs);

        // Long press on second tab.
        when(tabs[1].checkCloseHitTest(anyFloat(), anyFloat())).thenReturn(false);
        mStripLayoutHelper.setTabAtPositionForTesting(tabs[1]);
        mStripLayoutHelper.onLongPress(TIMESTAMP, 150f, 0f);

        // Verify that we enter reorder mode.
        assertTrue("Should be in reorder mode after long press on tab.",
                mStripLayoutHelper.getInReorderModeForTesting());
        assertFalse("Should not show tab menu after long press on tab.",
                mStripLayoutHelper.isTabMenuShowing());
    }

    @Test
    public void testOnLongPress_OnCloseButton() {
        // Initialize.
        initializeTest(false, false, 0);
        StripLayoutTab[] tabs = getMockedStripLayoutTabs(150f);
        mStripLayoutHelper.setStripLayoutTabsForTest(tabs);

        // Mock tab's view.
        View tabView = new View(mActivity);
        tabView.setLayoutParams(new MarginLayoutParams(150, 50));
        when(mModel.getTabAt(1).getView()).thenReturn(tabView);

        // Long press on second tab's close button.
        when(tabs[1].checkCloseHitTest(anyFloat(), anyFloat())).thenReturn(true);
        mStripLayoutHelper.setTabAtPositionForTesting(tabs[1]);
        mStripLayoutHelper.onLongPress(TIMESTAMP, 150f, 0f);

        // Verify that we show the "Close all tabs" popup menu.
        assertFalse("Should not be in reorder mode after long press on tab close button.",
                mStripLayoutHelper.getInReorderModeForTesting());
        assertTrue("Should show tab menu after long press on tab close button.",
                mStripLayoutHelper.isTabMenuShowing());
    }

    @Test
    public void testOnLongPress_OffTab() {
        // Initialize.
        initializeTest(false, false, 0);
        StripLayoutTab[] tabs = getMockedStripLayoutTabs(150f);
        mStripLayoutHelper.setStripLayoutTabsForTest(tabs);

        // Long press past the last tab.
        mStripLayoutHelper.setTabAtPositionForTesting(null);
        mStripLayoutHelper.onLongPress(TIMESTAMP, 150f, 0f);

        // Verify that we show the "Close all tabs" popup menu.
        assertFalse("Should not be in reorder mode after long press on empty space on tab strip.",
                mStripLayoutHelper.getInReorderModeForTesting());
        assertFalse("Should not show after long press on empty space on tab strip.",
                mStripLayoutHelper.isTabMenuShowing());
    }

    @Test
    @Feature("Tab Groups on Tab Strip")
    public void testTabGroupMargins_BetweenTabs() {
        // Initialize with 3 tabs.
        initializeTest(false, false, true, 0, 3);

        // Start reorder.
        mStripLayoutHelper.startReorderModeAtIndexForTesting(0);

        // Verify no tabs have a trailing margin, since there are no tab groups.
        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabs();
        assertEquals(EXPECTED_NO_MARGIN, tabs[0].getTrailingMargin(), 0f, EPSILON);
        assertEquals(EXPECTED_NO_MARGIN, tabs[1].getTrailingMargin(), 0f, EPSILON);
        assertEquals(EXPECTED_NO_MARGIN, tabs[2].getTrailingMargin(), 0f, EPSILON);
    }

    @Test
    @Feature("Tab Groups on Tab Strip")
    public void testTabGroupMargins_TabToLeft() {
        // Mock 1 tab to the left of a tab group with 3 tabs.
        initializeTest(false, false, true, 0, 4);
        groupTabs(1, 4);

        // Start reorder.
        mStripLayoutHelper.startReorderModeAtIndexForTesting(0);

        // Verify the leftmost and final tabs have a trailing margin.
        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabs();
        assertEquals(EXPECTED_MARGIN, tabs[0].getTrailingMargin(), TAB_MARGIN_WIDTH, EPSILON);
        assertEquals(EXPECTED_NO_MARGIN, tabs[1].getTrailingMargin(), 0f, EPSILON);
        assertEquals(EXPECTED_NO_MARGIN, tabs[2].getTrailingMargin(), 0f, EPSILON);
        assertEquals(EXPECTED_MARGIN, tabs[3].getTrailingMargin(), TAB_MARGIN_WIDTH, EPSILON);
    }

    @Test
    @Feature("Tab Groups on Tab Strip")
    public void testTabGroupMargins_TabToRight() {
        // Mock 1 tab to the right of a tab group with 3 tabs.
        initializeTest(false, false, true, 0, 4);
        groupTabs(0, 3);

        // Start reorder.
        mStripLayoutHelper.startReorderModeAtIndexForTesting(0);

        // Verify the rightmost tab in the tab group has a trailing margin.
        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabs();
        assertEquals(EXPECTED_NO_MARGIN, tabs[0].getTrailingMargin(), 0f, EPSILON);
        assertEquals(EXPECTED_NO_MARGIN, tabs[1].getTrailingMargin(), 0f, EPSILON);
        assertEquals(EXPECTED_MARGIN, tabs[2].getTrailingMargin(), TAB_MARGIN_WIDTH, EPSILON);
        assertEquals(EXPECTED_NO_MARGIN, tabs[3].getTrailingMargin(), 0f, EPSILON);
    }

    @Test
    @Feature("Tab Groups on Tab Strip")
    public void testTabGroupMargins_BetweenGroups() {
        // Mock a tab group with 2 tabs to the left of a tab group with 3 tabs.
        initializeTest(false, false, true, 0, 5);
        groupTabs(0, 2);
        groupTabs(2, 5);

        // Start reorder.
        mStripLayoutHelper.startReorderModeAtIndexForTesting(0);

        // Verify the rightmost tab in the first group has a trailing margin.
        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabs();
        assertEquals(EXPECTED_NO_MARGIN, tabs[0].getTrailingMargin(), 0f, EPSILON);
        assertEquals(EXPECTED_MARGIN, tabs[1].getTrailingMargin(), TAB_MARGIN_WIDTH, EPSILON);
        assertEquals(EXPECTED_NO_MARGIN, tabs[2].getTrailingMargin(), 0f, EPSILON);
        assertEquals(EXPECTED_NO_MARGIN, tabs[3].getTrailingMargin(), 0f, EPSILON);
        assertEquals(EXPECTED_MARGIN, tabs[4].getTrailingMargin(), TAB_MARGIN_WIDTH, EPSILON);
    }

    @Test
    @Feature("Tab Groups on Tab Strip")
    public void testTabGroupMargins_BetweenGroups_Rtl() {
        // Mock a tab group with 2 tabs to the right of a tab group with 3 tabs.
        initializeTest(true, false, true, 0, 5);
        groupTabs(0, 2);
        groupTabs(2, 5);

        // Start reorder.
        mStripLayoutHelper.startReorderModeAtIndexForTesting(0);

        // Verify the leftmost tab in the first group has a trailing margin.
        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabs();
        assertEquals(EXPECTED_NO_MARGIN, tabs[0].getTrailingMargin(), 0f, EPSILON);
        assertEquals(EXPECTED_MARGIN, tabs[1].getTrailingMargin(), TAB_MARGIN_WIDTH, EPSILON);
        assertEquals(EXPECTED_NO_MARGIN, tabs[2].getTrailingMargin(), 0f, EPSILON);
        assertEquals(EXPECTED_NO_MARGIN, tabs[3].getTrailingMargin(), 0f, EPSILON);
        assertEquals(EXPECTED_MARGIN, tabs[4].getTrailingMargin(), TAB_MARGIN_WIDTH, EPSILON);
    }

    @Test
    @Feature("Tab Groups on Tab Strip")
    public void testTabGroupMargins_StartReorder_Animated() {
        // Mock 1 tab to the left of a tab group with 3 tabs.
        initializeTest(false, false, false, 0, 4);
        groupTabs(1, 4);

        // Start reorder.
        mStripLayoutHelper.startReorderModeAtIndexForTesting(0);

        // Verify that only the last tab has a margin, since that one is not animated.
        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabs();
        assertEquals(EXPECTED_NO_MARGIN, tabs[0].getTrailingMargin(), 0f, EPSILON);
        assertEquals(EXPECTED_NO_MARGIN, tabs[1].getTrailingMargin(), 0f, EPSILON);
        assertEquals(EXPECTED_NO_MARGIN, tabs[2].getTrailingMargin(), 0f, EPSILON);
        assertEquals(EXPECTED_MARGIN, tabs[3].getTrailingMargin(), TAB_MARGIN_WIDTH, EPSILON);

        // Complete the currently running animations.
        assertNotNull(mStripLayoutHelper.getRunningAnimatorForTesting());
        mStripLayoutHelper.getRunningAnimatorForTesting().end();

        // Verify the leftmost and final tabs have a trailing margin.
        assertEquals(EXPECTED_MARGIN, tabs[0].getTrailingMargin(), TAB_MARGIN_WIDTH, EPSILON);
        assertEquals(EXPECTED_NO_MARGIN, tabs[1].getTrailingMargin(), 0f, EPSILON);
        assertEquals(EXPECTED_NO_MARGIN, tabs[2].getTrailingMargin(), 0f, EPSILON);
        assertEquals(EXPECTED_MARGIN, tabs[3].getTrailingMargin(), TAB_MARGIN_WIDTH, EPSILON);
    }

    @Test
    @Feature("Tab Groups on Tab Strip")
    public void testTabGroupMargins_StopReorder_Animated() {
        // Mock 1 tab to the left of a tab group with 3 tabs.
        initializeTest(false, false, false, 0, 4);
        groupTabs(1, 4);

        // Finish starting reorder, then begin stopping reorder.
        mStripLayoutHelper.startReorderModeAtIndexForTesting(0);
        mStripLayoutHelper.getRunningAnimatorForTesting().end();
        mStripLayoutHelper.stopReorderModeForTesting();

        // Verify the leftmost and final tabs have a trailing margin.
        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabs();
        assertEquals(EXPECTED_MARGIN, tabs[0].getTrailingMargin(), TAB_MARGIN_WIDTH, EPSILON);
        assertEquals(EXPECTED_NO_MARGIN, tabs[1].getTrailingMargin(), 0f, EPSILON);
        assertEquals(EXPECTED_NO_MARGIN, tabs[2].getTrailingMargin(), 0f, EPSILON);
        assertEquals(EXPECTED_MARGIN, tabs[3].getTrailingMargin(), TAB_MARGIN_WIDTH, EPSILON);

        // Complete the currently running animations.
        assertNotNull(mStripLayoutHelper.getRunningAnimatorForTesting());
        mStripLayoutHelper.getRunningAnimatorForTesting().end();

        // Verify that there are no margins as we have stopped reordering.
        assertEquals(EXPECTED_NO_MARGIN, tabs[0].getTrailingMargin(), 0f, EPSILON);
        assertEquals(EXPECTED_NO_MARGIN, tabs[1].getTrailingMargin(), 0f, EPSILON);
        assertEquals(EXPECTED_NO_MARGIN, tabs[2].getTrailingMargin(), 0f, EPSILON);
        assertEquals(EXPECTED_NO_MARGIN, tabs[3].getTrailingMargin(), 0f, EPSILON);
    }

    @Test
    @Feature("Tab Groups on Tab Strip")
    public void testTabGroupMargins_ResetMarginsOnStopReorder() {
        // Mock 1 tab to the left of a tab group with 3 tabs.
        initializeTest(false, false, true, 0, 4);
        groupTabs(1, 4);
        mStripLayoutHelper.onSizeChanged(SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP);

        // Start then stop reorder.
        mStripLayoutHelper.startReorderModeAtIndexForTesting(0);
        mStripLayoutHelper.stopReorderModeForTesting();

        // Verify no tabs have a trailing margin when reordering is stopped.
        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabs();
        assertEquals(EXPECTED_NO_MARGIN, tabs[0].getTrailingMargin(), 0f, EPSILON);
        assertEquals(EXPECTED_NO_MARGIN, tabs[1].getTrailingMargin(), 0f, EPSILON);
        assertEquals(EXPECTED_NO_MARGIN, tabs[2].getTrailingMargin(), 0f, EPSILON);
        assertEquals(EXPECTED_NO_MARGIN, tabs[3].getTrailingMargin(), 0f, EPSILON);
    }

    @Test
    @Feature("Tab Groups on Tab Strip")
    public void testTabGroupMargins_NoScrollOnReorder() {
        // Mock 1 tab to the right of 2 tab groups with 2 tabs each.
        initializeTest(false, false, true, 0, 5);
        groupTabs(2, 4);
        mStripLayoutHelper.onSizeChanged(SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP);
        mStripLayoutHelper.testSetScrollOffset(0);

        // Start reorder on leftmost tab. No margins to left of tab, so shouldn't scroll.
        // Verify the scroll offset is still 0.
        mStripLayoutHelper.startReorderModeAtIndexForTesting(0);
        assertEquals("There are no margins left of the selected tab, so we shouldn't scroll.", 0f,
                mStripLayoutHelper.getScrollOffset(), EPSILON);

        // Stop reorder. Verify the scroll offset is still 0.
        mStripLayoutHelper.stopReorderModeForTesting();
        assertEquals("Scroll offset should return to 0 after stopping reorder mode.", 0f,
                mStripLayoutHelper.getScrollOffset(), EPSILON);
    }

    @Test
    @Feature("Tab Groups on Tab Strip")
    public void testTabGroupMargins_ScrollOnReorder() {
        // Mock 6 tabs to the right of 2 tab groups with 2 tabs each.
        initializeTest(false, false, true, 0, 10);
        groupTabs(0, 2);
        groupTabs(2, 4);
        mStripLayoutHelper.onSizeChanged(SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP);
        mStripLayoutHelper.testSetScrollOffset(0);

        // Start reorder on tab to the right of groups. 2 margins to left of tab, so should scroll.
        // Verify the scroll offset is 2 * (-marginWidth) + startMargin = 2 * -54 + -54 = -162
        // marginWidth is half of 0.5 * minTabWidth = 108 / 2 = 54.
        float expectedOffset = -162f;
        mStripLayoutHelper.startReorderModeAtIndexForTesting(4);
        assertEquals("There are margins left of the selected tab, so we should scroll.",
                expectedOffset, mStripLayoutHelper.getScrollOffset(), EPSILON);

        // Stop reorder. Verify the scroll offset is once again 0.
        mStripLayoutHelper.stopReorderModeForTesting();
        assertEquals("Scroll offset should return to 0 after stopping reorder mode.", 0,
                mStripLayoutHelper.getScrollOffset(), EPSILON);
    }

    @Test
    @Feature("Tab Groups on Tab Strip")
    public void testTabGroupMargins_ScrollOnReorder_Animated() {
        // Mock 6 tabs to the right of 2 tab groups with 2 tabs each.
        initializeTest(false, false, false, 0, 10);
        groupTabs(0, 2);
        groupTabs(2, 4);
        mStripLayoutHelper.onSizeChanged(SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP);
        mStripLayoutHelper.testSetScrollOffset(0);

        // Start reorder on tab to the right of groups. 2 margins to left of tab, so should scroll.
        // Verify the scroll offset has not yet changed.
        mStripLayoutHelper.startReorderModeAtIndexForTesting(4);
        assertEquals("The scroller has not finished yet, so the offset shouldn't change.", 0f,
                mStripLayoutHelper.getScrollOffset(), EPSILON);

        // Finish animations.
        // Verify the scroll offset is 2 * (-marginWidth) + startMargin = 2 * -54 + -54 = -162
        // marginWidth is half of 0.5 * minTabWidth = 108 / 2 = 54.
        float expectedOffset = -162f;
        mStripLayoutHelper.getRunningAnimatorForTesting().end();
        assertEquals("The scroller has finished, so the offset should change.", expectedOffset,
                mStripLayoutHelper.getScrollOffset(), EPSILON);

        // Stop reorder. Verify the scroll offset is still -285.
        mStripLayoutHelper.stopReorderModeForTesting();
        assertEquals("The scroller has not finished yet, so the offset shouldn't change.",
                expectedOffset, mStripLayoutHelper.getScrollOffset(), EPSILON);

        // Finish animations.
        // Verify the scroll offset is once again 0.
        mStripLayoutHelper.getRunningAnimatorForTesting().end();
        assertEquals("The scroller has finished, so the offset should change.", 0,
                mStripLayoutHelper.getScrollOffset(), EPSILON);
    }

    @Test
    @Feature("Tab Groups on Tab Strip")
    @Features.DisableFeatures(ChromeFeatureList.TAB_STRIP_REDESIGN)
    public void testReorder_SetBackgroundTabsDimmed() {
        // Mock 5 tabs.
        initializeTest(false, false, true, 0, 5);
        mStripLayoutHelper.onSizeChanged(SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP);

        // Start reorder mode on first tab.
        mStripLayoutHelper.startReorderModeAtIndexForTesting(0);

        // Verify background tabs are dimmed.
        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabs();
        float expectedNotDimmed = StripLayoutHelper.BACKGROUND_TAB_BRIGHTNESS_DEFAULT;
        float expectedDimmed = StripLayoutHelper.BACKGROUND_TAB_BRIGHTNESS_DIMMED;
        assertEquals("Selected tab should not dim.", expectedNotDimmed, tabs[0].getBrightness(),
                EPSILON);
        assertEquals(
                "Background tab should dim.", expectedDimmed, tabs[1].getBrightness(), EPSILON);
        assertEquals(
                "Background tab should dim.", expectedDimmed, tabs[2].getBrightness(), EPSILON);
        assertEquals(
                "Background tab should dim.", expectedDimmed, tabs[3].getBrightness(), EPSILON);
        assertEquals(
                "Background tab should dim.", expectedDimmed, tabs[4].getBrightness(), EPSILON);
    }

    @Test
    @Feature("Tab Groups on Tab Strip")
    @Features.DisableFeatures(ChromeFeatureList.TAB_STRIP_REDESIGN)
    public void testReorder_SetSelectedTabGroupNotDimmed() {
        // Mock 5 tabs. Group the first two tabs.
        initializeTest(false, false, true, 0, 5);
        mStripLayoutHelper.onSizeChanged(SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP);
        groupTabs(0, 2);

        // Start reorder mode on third tab. Drag to hover over the tab group.
        // -100 < -marginWidth = -95
        mStripLayoutHelper.startReorderModeAtIndexForTesting(2);
        float dragDistance = -100f;
        float startX = mStripLayoutHelper.getLastReorderX();
        mStripLayoutHelper.drag(TIMESTAMP, startX + dragDistance, 0f, dragDistance, 0f, 0f, 0f);

        // Verify background tabs are dimmed, while interacting tab and hovered group are not.
        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabs();
        float expectedNotDimmed = StripLayoutHelper.BACKGROUND_TAB_BRIGHTNESS_DEFAULT;
        float expectedDimmed = StripLayoutHelper.BACKGROUND_TAB_BRIGHTNESS_DIMMED;
        assertEquals("Tab in hovered group should not dim.", expectedNotDimmed,
                tabs[0].getBrightness(), EPSILON);
        assertEquals("Tab in hovered group should not dim.", expectedNotDimmed,
                tabs[1].getBrightness(), EPSILON);
        assertEquals("Selected tab should not dim.", expectedNotDimmed, tabs[2].getBrightness(),
                EPSILON);
        assertEquals(
                "Background tab should dim.", expectedDimmed, tabs[3].getBrightness(), EPSILON);
        assertEquals(
                "Background tab should dim.", expectedDimmed, tabs[4].getBrightness(), EPSILON);
    }

    @Test
    @Feature("Tab Strip Redesign")
    public void testReorder_SetSelectedTabGroupContainersVisible() {
        // Mock 5 tabs. Group the first two tabs.
        initializeTest(false, false, true, 2, 5);
        mStripLayoutHelper.onSizeChanged(SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP);
        groupTabs(0, 2);

        // Start reorder mode on third tab. Drag to hover over the tab group.
        // -100 < -marginWidth = -95
        mStripLayoutHelper.startReorderModeAtIndexForTesting(2);
        float dragDistance = -100f;
        float startX = mStripLayoutHelper.getLastReorderX();
        mStripLayoutHelper.drag(TIMESTAMP, startX + dragDistance, 0f, dragDistance, 0f, 0f, 0f);

        // Verify hovered group tab containers are visible.
        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabs();
        float expectedHidden = StripLayoutHelper.TAB_OPACITY_HIDDEN;
        float expectedVisibleBackground = StripLayoutHelper.TAB_OPACITY_VISIBLE_BACKGROUND;
        float expectedVisibleForeground = StripLayoutHelper.TAB_OPACITY_VISIBLE_FOREGROUND;
        assertEquals("Container in hovered group should be visible.", expectedVisibleBackground,
                tabs[0].getContainerOpacity(), EPSILON);
        assertEquals("Container in hovered group should be visible.", expectedVisibleBackground,
                tabs[1].getContainerOpacity(), EPSILON);
        assertEquals("Selected container should be visible.", expectedVisibleForeground,
                tabs[2].getContainerOpacity(), EPSILON);
        assertEquals("Background containers should not be visible.", expectedHidden,
                tabs[3].getContainerOpacity(), EPSILON);
        assertEquals("Background containers should not be visible.", expectedHidden,
                tabs[4].getContainerOpacity(), EPSILON);
    }

    @Test
    @Feature("Tab Groups on Tab Strip")
    public void testReorder_HapticFeedback() {
        // Mock 5 tabs.
        initializeTest(false, false, 0);

        // Start reorder mode on first tab.
        mStripLayoutHelper.startReorderModeAtIndexForTesting(0);

        // Verify we performed haptic feedback for a long-press.
        verify(mInteractingTabView).performHapticFeedback(eq(HapticFeedbackConstants.LONG_PRESS));
    }

    @Test
    @Feature("Tab Groups on Tab Strip")
    public void testReorder_NoGroups() {
        // Mock 5 tabs.
        initializeTest(false, false, true, 0, 5);
        mStripLayoutHelper.onSizeChanged(SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP);
        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabs();
        StripLayoutTab thirdTab = tabs[2];
        StripLayoutTab fourthTab = tabs[3];

        // Start reorder on third tab. Drag right to trigger swap with fourth tab.
        // 100 > tabWidth * flipThreshold = (190-24) * 0.53 = 88
        mStripLayoutHelper.startReorderModeAtIndexForTesting(2);
        float dragDistance = 100f;
        float startX = mStripLayoutHelper.getLastReorderX();
        mStripLayoutHelper.drag(TIMESTAMP, startX + dragDistance, 0f, dragDistance, 0f, 0f, 0f);

        // Assert the tabs swapped.
        assertEquals("Third and fourth tabs should have swapped.", thirdTab, tabs[3]);
        assertEquals("Third and fourth tabs should have swapped.", fourthTab, tabs[2]);
    }

    @Test
    @Feature("Tab Groups on Tab Strip")
    public void testReorder_DragOutOfGroup() {
        // Mock a tab group with 3 tabs with 1 tab to the left and 1 tab to the right.
        initializeTest(false, false, true, 0, 5);
        mStripLayoutHelper.onSizeChanged(SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP);
        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabs();
        StripLayoutTab fourthTab = tabs[3];
        groupTabs(1, 4);

        // Start reorder on fourth tab. Drag right out of the tab group.
        // 60 > marginWidth * flipThreshold = 95 * 0.53 = 51
        mStripLayoutHelper.startReorderModeAtIndexForTesting(3);
        float dragDistance = 60f;
        float startX = mStripLayoutHelper.getLastReorderX();
        mStripLayoutHelper.drag(TIMESTAMP, startX + dragDistance, 0f, dragDistance, 0f, 0f, 0f);

        // Verify fourth tab was dragged out of group, but not reordered.
        assertEquals("Fourth tab should not have moved.", fourthTab, tabs[3]);
        verify(mTabGroupModelFilter).moveTabOutOfGroupInDirection(fourthTab.getId(), true);
    }

    @Test
    @Feature("Tab Groups on Tab Strip")
    public void testReorder_DragOutOfGroup_StartOfStrip() {
        // Mock a tab group with 3 tabs with 2 tabs to the right.
        initializeTest(false, false, true, 0, 5);
        mStripLayoutHelper.onSizeChanged(SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP);
        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabs();
        StripLayoutTab firstTab = tabs[0];
        groupTabs(0, 3);

        // Start reorder on first tab. Drag left out of the tab group.
        // -60 < -(marginWidth * flipThreshold) = -(95 * 0.53) = -51
        mStripLayoutHelper.startReorderModeAtIndexForTesting(0);
        float dragDistance = -60f;
        float startX = mStripLayoutHelper.getLastReorderX();
        mStripLayoutHelper.drag(TIMESTAMP, startX + dragDistance, 0f, dragDistance, 0f, 0f, 0f);

        // Verify first tab was dragged out of group, but not reordered.
        assertEquals("First tab should not have moved.", firstTab, tabs[0]);
        verify(mTabGroupModelFilter).moveTabOutOfGroupInDirection(firstTab.getId(), false);
    }

    @Test
    @Feature("Tab Groups on Tab Strip")
    public void testReorder_DragOutOfGroup_EndOfStrip() {
        // Mock a tab group with 3 tabs with 2 tabs to the left.
        initializeTest(false, false, true, 0, 5);
        mStripLayoutHelper.onSizeChanged(SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP);
        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabs();
        StripLayoutTab fifthTab = tabs[4];
        groupTabs(2, 5);

        // Start reorder on fifth tab. Drag right out of the tab group.
        // 60 > marginWidth * flipThreshold = 95 * 0.53 = 51
        mStripLayoutHelper.startReorderModeAtIndexForTesting(4);
        float dragDistance = 60f;
        float startX = mStripLayoutHelper.getLastReorderX();
        mStripLayoutHelper.drag(TIMESTAMP, startX + dragDistance, 0f, dragDistance, 0f, 0f, 0f);

        // Verify fifth tab was dragged out of group, but not reordered.
        assertEquals("Fifth tab should not have moved.", fifthTab, tabs[4]);
        verify(mTabGroupModelFilter).moveTabOutOfGroupInDirection(fifthTab.getId(), true);
    }

    @Test
    @Feature("Tab Groups on Tab Strip")
    public void testReorder_DragPastGroup() {
        // Mock a tab group with 3 tabs with 1 tab to the left and 1 tab to the right.
        initializeTest(false, false, true, 0, 5);
        mStripLayoutHelper.onSizeChanged(SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP);
        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabs();
        StripLayoutTab firstTab = tabs[0];
        groupTabs(1, 4);

        // Start reorder on first tab. Drag right over the tab group.
        // 650 > 3*tabWidth + margin + flipThreshold*margin = 3*(190-24) + 1.53*95 = 644 > 300
        mStripLayoutHelper.startReorderModeAtIndexForTesting(0);
        float dragDistance = 300f;
        float startX = mStripLayoutHelper.getLastReorderX();
        mStripLayoutHelper.drag(TIMESTAMP, startX + dragDistance, 0f, dragDistance, 0f, 0f, 0f);
        // Verify no reordering, since we have not hovered over the tab group long enough.
        assertEquals("First tab should not have moved.", firstTab, tabs[0]);

        // Drag past the tab group.
        dragDistance = 650f;
        mStripLayoutHelper.drag(TIMESTAMP, startX + dragDistance, 0f, dragDistance, 0f, 0f, 0f);
        // Verify reordering, since we have dragged past the tab group.
        assertEquals("First tab should now be the fourth tab.", firstTab.getId(), tabs[3].getId());
    }

    @Test
    @Feature("Tab Groups on Tab Strip")
    public void testReorder_MergeToGroup() {
        // Mock 5 tabs. Group the first two tabs.
        initializeTest(false, false, true, 0, 5);
        mStripLayoutHelper.onSizeChanged(SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP);
        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabs();
        StripLayoutTab thirdTab = tabs[2];
        int oldSecondTabId = tabs[1].getId();
        groupTabs(0, 2);

        // Start reorder mode on third tab. Drag between tabs in group.
        // -300 < -(tabWidth + marginWidth) = -(190 + 95) = -285
        mStripLayoutHelper.startReorderModeAtIndexForTesting(2);
        float dragDistance = -200f;
        float startX = mStripLayoutHelper.getLastReorderX();
        mStripLayoutHelper.drag(TIMESTAMP, startX + dragDistance, 0f, dragDistance, 0f, 0f, 0f);

        // Verify state has not yet changed.
        tabs = mStripLayoutHelper.getStripLayoutTabs();
        assertEquals("Third tab should not have moved.", thirdTab, tabs[2]);
        verify(mTabGroupModelFilter, never()).mergeTabsToGroup(anyInt(), anyInt());
        verify(mTabGroupModelFilter, never()).mergeTabsToGroup(anyInt(), anyInt(), anyBoolean());

        // Wait minimum time to trigger merge.
        // -10 > -(dropMaxDragOffset) = -36
        dragDistance = -10;
        startX = mStripLayoutHelper.getLastReorderX();
        long timeDelta = StripLayoutHelper.DROP_INTO_GROUP_MS;
        mStripLayoutHelper.drag(
                TIMESTAMP + timeDelta, startX + dragDistance, 0f, dragDistance, 0f, 0f, 0f);

        // Verify interacting tab was merged into group at the second index.
        tabs = mStripLayoutHelper.getStripLayoutTabs();
        assertEquals("Third tab should now be second tab.", thirdTab, tabs[1]);
        verify(mTabGroupModelFilter)
                .mergeTabsToGroup(eq(thirdTab.getId()), eq(oldSecondTabId), eq(true));
    }

    @Test
    @Feature("Tab Groups on Tab Strip")
    public void testReorder_NoExtraMinScroll() {
        // Mock 3 tabs. Group the first two tabs.
        initializeTest(false, false, true, 0, 3);
        mStripLayoutHelper.onSizeChanged(SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP);

        // Start reorder mode on third tab.
        mStripLayoutHelper.startReorderModeAtIndexForTesting(2);

        // Verify extra scroll offset.
        assertEquals("Extra min offset should not be set.", 0f,
                mStripLayoutHelper.getReorderExtraMinScrollOffset(), EPSILON);
    }

    @Test
    @Feature("Tab Groups on Tab Strip")
    @Features.DisableFeatures(ChromeFeatureList.TAB_STRIP_REDESIGN)
    public void testReorder_ExtraMinScroll() {
        // Mock 3 tabs. Group the first two tabs.
        initializeTest(false, false, true, 0, 3);
        mStripLayoutHelper.onSizeChanged(SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP);
        groupTabs(0, 2);

        // Start reorder mode on third tab.
        mStripLayoutHelper.startReorderModeAtIndexForTesting(2);

        // Verify extra scroll offset.
        assertNotEquals("Extra min offset should be set.", 0f,
                mStripLayoutHelper.getReorderExtraMinScrollOffset(), EPSILON);
    }

    @Test
    public void testTabClosed() {
        // Initialize with 10 tabs.
        int tabCount = 10;
        initializeTest(false, false, false, 0, tabCount);

        // Remove tab from model and verify that the tab strip has not yet updated.
        int closedTabId = 1;
        int expectedNumTabs = tabCount;
        mModel.closeTab(mModel.getTabAt(closedTabId), false, false, true);
        assertEquals("Tab strip should not yet have changed.", expectedNumTabs,
                mStripLayoutHelper.getStripLayoutTabs().length);

        // Trigger update and verify the tab strip matches the tab model.
        expectedNumTabs = 9;
        mStripLayoutHelper.tabClosed(TIMESTAMP, closedTabId);
        assertEquals("Tab strip should match tab model.", expectedNumTabs,
                mStripLayoutHelper.getStripLayoutTabs().length);
        verify(mUpdateHost, times(2)).requestUpdate();
    }

    @Test
    public void testTabClosing_NoTabResize() {
        // Arrange
        int tabCount = 15;
        initializeTest(false, false, false, 14, tabCount);
        StripLayoutTab[] tabs = getRealStripLayoutTabs(TAB_WIDTH_SMALL, tabCount);
        mStripLayoutHelper.setStripLayoutTabsForTest(tabs);
        mStripLayoutHelper.onSizeChanged(SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP);
        setupForAnimations();

        mStripLayoutHelper.updateLayout(TIMESTAMP);

        // Act: Call on close tab button handler.
        mStripLayoutHelper.handleCloseButtonClick(tabs[14], TIMESTAMP);

        // Assert: Animations started.
        assertTrue("MultiStepAnimations should have started.",
                mStripLayoutHelper.isMultiStepCloseAnimationsRunning());

        // Act: End the tab closing animations to apply final values.
        CompositorAnimator runningAnimator =
                (CompositorAnimator) mStripLayoutHelper.getRunningAnimatorForTesting();
        runningAnimator.end();

        // Assert: Tab is closed and animations are still running.
        int expectedTabCount = 14;
        assertEquals("Unexpected tabs count.", expectedTabCount,
                mStripLayoutHelper.getStripLayoutTabs().length);
        assertTrue("MultiStepAnimations should still be running.",
                mStripLayoutHelper.isMultiStepCloseAnimationsRunning());

        // Act: End next set of animations to apply final values.
        mStripLayoutHelper.getRunningAnimatorForTesting().end();

        // Assert: Animations completed. The tab width is not resized and drawX does not change.
        float expectedDrawX =
                -395.f; // Since we are focused on the last tab, start tabs are off screen.
        StripLayoutTab[] updatedTabs = mStripLayoutHelper.getStripLayoutTabs();
        for (StripLayoutTab stripTab : updatedTabs) {
            assertEquals("Unexpected tab width after resize.", 108.f, stripTab.getWidth(), 0);
            assertEquals("Unexpected tab position.", expectedDrawX, stripTab.getDrawX(), 0);
            expectedDrawX += TAB_WIDTH_SMALL - TAB_OVERLAP_WIDTH;
        }
        assertFalse("MultiStepAnimations should have stopped running.",
                mStripLayoutHelper.isMultiStepCloseAnimationsRunning());
    }

    @Test
    public void testTabClosing_NonLastTab_TabResize() {
        // Arrange
        int tabCount = 4;
        initializeTest(false, false, false, 3, tabCount);
        StripLayoutTab[] tabs = getRealStripLayoutTabs(TAB_WIDTH_MEDIUM, tabCount);
        mStripLayoutHelper.setStripLayoutTabsForTest(tabs);
        mStripLayoutHelper.onSizeChanged(SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP);
        setupForAnimations();

        mStripLayoutHelper.updateLayout(TIMESTAMP);

        // Act: Call on close tab button handler.
        mStripLayoutHelper.handleCloseButtonClick(tabs[2], TIMESTAMP);

        // Assert: Animations started.
        assertTrue("MultiStepAnimations should have started.",
                mStripLayoutHelper.isMultiStepCloseAnimationsRunning());

        // Act: End the animations to apply final values.
        CompositorAnimator runningAnimator =
                (CompositorAnimator) mStripLayoutHelper.getRunningAnimatorForTesting();
        runningAnimator.end();

        // Assert: Tab is closed and animations are still running.
        int expectedTabCount = 3;
        assertEquals(expectedTabCount, mStripLayoutHelper.getStripLayoutTabs().length);
        assertTrue("MultiStepAnimations should still be running.",
                mStripLayoutHelper.isMultiStepCloseAnimationsRunning());

        // Act: Set animation time forward by 250ms for next set of animations.
        mStripLayoutHelper.getRunningAnimatorForTesting().end();

        // Assert: Animations completed. The tab width is resized, tab.drawX is changed and
        // newTabButton.drawX is also changed.
        float expectedDrawX = 0.f;
        float expectedWidthAfterResize = 265.f;
        StripLayoutTab[] updatedTabs = mStripLayoutHelper.getStripLayoutTabs();
        for (int i = 0; i < updatedTabs.length; i++) {
            StripLayoutTab stripTab = updatedTabs[i];
            assertEquals("Unexpected tab width after resize.", expectedWidthAfterResize,
                    stripTab.getWidth(), 0.1f);
            assertEquals("Unexpected tab position.", expectedDrawX, stripTab.getDrawX(), 0.1f);
            expectedDrawX += (expectedWidthAfterResize - TAB_OVERLAP_WIDTH);
        }
        assertFalse("MultiStepAnimations should have ended.",
                mStripLayoutHelper.isMultiStepCloseAnimationsRunning());
    }

    @Test
    public void testFlingLeft() {
        // Arrange
        initializeTest(false, false, false, 11, 12);
        mStripLayoutHelper.onSizeChanged(SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP);
        mStripLayoutHelper.updateLayout(TIMESTAMP);
        mStripLayoutHelper.testSetScrollOffset(-150);

        // Act: Perform a fling and update layout.
        float velocityX = -7000f;
        // The velocityX value is used to calculate the scroller.finalX value.
        mStripLayoutHelper.fling(TIMESTAMP, 0, 0, velocityX, 0);
        // This will use the scroller.finalX value to update the scrollOffset. The timestamp
        // value here will determine the fling duration and affects the final offset value.
        mStripLayoutHelper.updateLayout(TIMESTAMP + 10);

        // Assert: Final scrollOffset.
        // The calculation of this value is done using the velocity. The velocity along a friction
        // constant is used to calculate deceleration and distance. That together with the animation
        // duration determines the final scroll offset position.
        float expectedOffset = -220.f;
        assertEquals("Unexpected scroll offset.", expectedOffset,
                mStripLayoutHelper.getScrollOffset(), 0.0);
    }

    @Test
    public void testFlingRight() {
        // Arrange
        initializeTest(false, false, false, 10, 11);
        mStripLayoutHelper.onSizeChanged(SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP);
        // When updateLayout is called for the first time, bringSelectedTabToVisibleArea() method is
        // invoked. That also affects the scrollOffset value. So we call updateLayout before
        // performing a fling so that bringSelectedTabToVisible area isn't called after the fling.
        mStripLayoutHelper.updateLayout(TIMESTAMP);
        mStripLayoutHelper.testSetScrollOffset(-150);

        // Act: Perform a fling and update layout.
        float velocity = 5000f;
        // The velocityX value is used to calculate the scroller.finalX value.
        mStripLayoutHelper.fling(TIMESTAMP, 0, 0, velocity, 0);
        // This will use the scroller.finalX value to update the scrollOffset. The timestamp
        // value here will determine the fling duration and affects the final offset value.
        mStripLayoutHelper.updateLayout(TIMESTAMP + 20);

        // Assert: Final scrollOffset.
        // The calculation of this value is done using the velocity. The velocity along a friction
        // constant is used to calculate deceleration and distance. That together with the animation
        // duration determines the final scroll offset position.
        float expectedOffset = -50.f;
        assertEquals("Unexpected scroll offset.", expectedOffset,
                mStripLayoutHelper.getScrollOffset(), 0.0);
    }

    @Test
    public void testDrag_UpdatesScrollOffset_ScrollingStrip() {
        // Arrange
        initializeTest(false, false, false, 13, 14);
        mStripLayoutHelper.onSizeChanged(SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP);
        // When updateLayout is called for the first time, bringSelectedTabToVisibleArea() method is
        // invoked. That also affects the scrollOffset value. So we call updateLayout before
        // performing a fling so that bringSelectedTabToVisible area isn't called after the fling.
        mStripLayoutHelper.updateLayout(TIMESTAMP);
        mStripLayoutHelper.testSetScrollOffset(-150);

        // Act: Drag and update layout.
        float dragDeltaX = -200.f;
        float totalY = 85.f; // totalY > 50.f to cross reorder threshold.
        mStripLayoutHelper.drag(TIMESTAMP, 374.74f, 24.276f, dragDeltaX, -0.304f, -16.078f, totalY);

        float expectedOffset = -350; // mScrollOffset + dragDeltaX = -200 - 150 = -350
        // Assert scroll offset position.
        assertEquals("Unexpected scroll offset.", expectedOffset,
                mStripLayoutHelper.getScrollOffset(), 0.0);
        // Reorder mode is disabled for scrolling strip.
        assertFalse(mStripLayoutHelper.isInReorderModeForTesting());
    }

    private void setupForAnimations() {
        CompositorAnimationHandler mHandler = new CompositorAnimationHandler(() -> {});
        // CompositorAnimationHandler.setTestingMode(true);
        when(mUpdateHost.getAnimationHandler()).thenReturn(mHandler);

        // Update layout when updateHost.requestUpdate is called.
        doAnswer(invocation -> {
            mStripLayoutHelper.updateLayout(TIMESTAMP);
            return null;
        })
                .when(mUpdateHost)
                .requestUpdate();
    }

    private void initializeTest(
            boolean rtl, boolean incognito, boolean disableAnimations, int tabIndex, int numTabs) {
        mStripLayoutHelper = createStripLayoutHelper(rtl, incognito);
        if (disableAnimations) mStripLayoutHelper.disableAnimationsForTesting();
        mIncognito = incognito;
        if (numTabs <= 5) {
            for (int i = 0; i < numTabs; i++) {
                mModel.addTab(TEST_TAB_TITLES[i]);
                when(mModel.getTabAt(i).isHidden()).thenReturn(tabIndex != i);
                when(mModel.getTabAt(i).getView()).thenReturn(mInteractingTabView);
                when(mTabGroupModelFilter.getRootId(eq(mModel.getTabAt(i)))).thenReturn(i);
            }
        } else {
            for (int i = 0; i < numTabs; i++) {
                mModel.addTab("Tab " + i);
                when(mModel.getTabAt(i).isHidden()).thenReturn(tabIndex != i);
                when(mModel.getTabAt(i).getView()).thenReturn(mInteractingTabView);
                when(mTabGroupModelFilter.getRootId(eq(mModel.getTabAt(i)))).thenReturn(i);
            }
        }
        mModel.setIndex(tabIndex);
        mStripLayoutHelper.setTabModel(mModel, null);
        mStripLayoutHelper.setTabGroupModelFilter(mTabGroupModelFilter);
        mStripLayoutHelper.tabSelected(0, tabIndex, 0, false);
        // Flush UI updated
    }

    private void initializeTest(boolean rtl, boolean incognito, int tabIndex) {
        initializeTest(rtl, incognito, false, tabIndex, 5);
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
                mActivity, mManagerHost, mUpdateHost, mRenderHost, incognito, mModelSelectorBtn);
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

    private StripLayoutTab[] getRealStripLayoutTabs(float tabWidth, int numTabs) {
        StripLayoutTab[] tabs = new StripLayoutTab[mModel.getCount()];
        for (int i = 0; i < numTabs; i++) {
            tabs[i] = getRealStripTab(i, tabWidth, i * (tabWidth - TAB_OVERLAP_WIDTH));
        }
        return tabs;
    }

    private StripLayoutTab mockStripTab(int id, float tabWidth, float mDrawX) {
        StripLayoutTab tab = mock(StripLayoutTab.class);
        when(tab.getWidth()).thenReturn(tabWidth);
        when(tab.getId()).thenReturn(id);
        when(tab.getDrawX()).thenReturn(mDrawX);
        when(tab.getVisiblePercentage()).thenReturn(1f);
        return tab;
    }

    private StripLayoutTab getRealStripTab(int id, float tabWidth, float mDrawX) {
        Context context = mock(Context.class);
        Resources res = mock(Resources.class);
        DisplayMetrics dm = new DisplayMetrics();
        dm.widthPixels = Math.round(SCREEN_WIDTH);
        dm.heightPixels = Math.round(SCREEN_HEIGHT);
        when(res.getDisplayMetrics()).thenReturn(dm);
        when(context.getResources()).thenReturn(res);

        StripLayoutTabDelegate delegate = mock(StripLayoutTabDelegate.class);
        TabLoadTracker.TabLoadTrackerCallback loadTrackerCallback =
                mock(TabLoadTrackerCallback.class);

        StripLayoutTab tab = new StripLayoutTab(
                context, id, delegate, loadTrackerCallback, mRenderHost, mUpdateHost, false);
        tab.setWidth(tabWidth);
        tab.setDrawX(mDrawX);
        return tab;
    }

    /**
     * Mock that the sequence of tabs from startIndex to endIndex are part of that same tab group.
     * @param startIndex The index where we start including tabs in the group (inclusive).
     * @param endIndex The index where we stop including tabs in the group (exclusive).
     */
    private void groupTabs(int startIndex, int endIndex) {
        int groupRootId = mModel.getTabAt(startIndex).getId();
        int numTabs = endIndex - startIndex;

        for (int i = startIndex; i < endIndex; i++) {
            when(mTabGroupModelFilter.hasOtherRelatedTabs(eq(mModel.getTabAt(i)))).thenReturn(true);
            when(mTabGroupModelFilter.getRootId(eq(mModel.getTabAt(i)))).thenReturn(groupRootId);
        }
        when(mTabGroupModelFilter.getRelatedTabCountForRootId(eq(groupRootId))).thenReturn(numTabs);
    }
}
