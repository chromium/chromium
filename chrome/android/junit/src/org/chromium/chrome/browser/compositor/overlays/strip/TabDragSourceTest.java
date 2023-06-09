// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.atMostOnce;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.Context;
import android.os.Parcel;
import android.view.ContextThemeWrapper;
import android.view.DragEvent;
import android.view.View;
import android.view.ViewGroup.MarginLayoutParams;

import androidx.test.core.app.ApplicationProvider;

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

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerHost;
import org.chromium.chrome.browser.compositor.layouts.LayoutRenderHost;
import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;
import org.chromium.chrome.browser.compositor.layouts.components.CompositorButton;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.ui.base.LocalizationUtils;

/** Tests for {@link TabDragSource}. */
@RunWith(BaseRobolectricTestRunner.class)
// clang-format off
@Features.EnableFeatures({ChromeFeatureList.TAB_DRAG_DROP_ANDROID})
public class TabDragSourceTest {
    // clang-format on
    @Rule
    public TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();
    @Mock
    private MultiInstanceManager mMultiInstanceManager;
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
    private TabModelSelector mTabModelSelector;

    private Activity mActivity;
    private Context mContext;
    private TestTabModel mModel = new TestTabModel();
    private StripLayoutHelper mStripLayoutHelper;
    private TabDragSource mTabDragSource;
    private StripLayoutTab mClickedTab;
    private View mTabsToolbarView;
    private static final String[] TEST_TAB_TITLES = {"Tab 1", "Tab 2", "Tab 3", "Tab 4", "Tab 5"};
    private static final float SCREEN_WIDTH = 800.f;
    private static final float SCREEN_HEIGHT = 1600.f;
    private static final float TAB_WIDTH = 150.f;
    private static final long TIMESTAMP = 5000;

    /** Resets the environment before each test. */
    @Before
    public void beforeTest() {
        MockitoAnnotations.initMocks(this);
        mContext = new ContextThemeWrapper(
                ApplicationProvider.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);

        mActivity = Robolectric.setupActivity(Activity.class);
        mActivity.setTheme(org.chromium.chrome.R.style.Theme_BrowserUI);

        // Get and spy on the singleton TabDragSource.
        mTabDragSource = Mockito.spy(TabDragSource.getInstance());

        // Create and spy on a simulated tab view.
        mTabsToolbarView = Mockito.spy(new View(mActivity));
        mTabsToolbarView.setLayoutParams(new MarginLayoutParams(150, 50));
    }

    @After
    public void tearDown() {
        if (mStripLayoutHelper != null) {
            mStripLayoutHelper.stopReorderModeForTesting();
            mStripLayoutHelper.setTabAtPositionForTesting(null);
        }
        if (mTabDragSource != null) {
            mTabDragSource.resetTabDragSource();
            mTabDragSource = null;
        }
        mTabsToolbarView = null;
    }

    private void initializeTest(boolean rtl, boolean incognito, int tabIndex, int numTabs) {
        mStripLayoutHelper = Mockito.spy(createStripLayoutHelper(rtl, incognito));
        mStripLayoutHelper.disableAnimationsForTesting();
        for (int i = 0; i < numTabs; i++) {
            mModel.addTab(TEST_TAB_TITLES[i]);
            when(mTabGroupModelFilter.getRootId(eq(mModel.getTabAt(i)))).thenReturn(i);
        }
        mModel.setIndex(tabIndex);
        mStripLayoutHelper.setTabModel(mModel, null, false);
        mStripLayoutHelper.setTabGroupModelFilter(mTabGroupModelFilter);
        mStripLayoutHelper.tabSelected(0, tabIndex, 0, false);
        mStripLayoutHelper.onSizeChanged(SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP);
        mStripLayoutHelper.updateLayout(TIMESTAMP);
        StripLayoutTab[] tabs = new StripLayoutTab[mModel.getCount()];
        for (int i = 0; i < numTabs; i++) {
            tabs[i] = mockStripTab(i, TAB_WIDTH);
        }
        mStripLayoutHelper.setStripLayoutTabsForTest(tabs);
        mClickedTab = tabs[tabIndex];

        assertTrue(mStripLayoutHelper.getStripLayoutTabs().length == numTabs);
    }

    private StripLayoutHelper createStripLayoutHelper(boolean rtl, boolean incognito) {
        LocalizationUtils.setRtlForTesting(rtl);
        final StripLayoutHelper stripLayoutHelper = new StripLayoutHelper(
                mActivity, mManagerHost, mUpdateHost, mRenderHost, incognito, mModelSelectorBtn);
        stripLayoutHelper.onContextChanged(mActivity);
        return stripLayoutHelper;
    }

    private StripLayoutTab mockStripTab(int id, float tabWidth) {
        StripLayoutTab tab = mock(StripLayoutTab.class);
        when(tab.getWidth()).thenReturn(tabWidth);
        when(tab.getId()).thenReturn(id);
        return tab;
    }

    private DragEvent createDragEvent(int action, float x, float y, int result) {
        Parcel parcel = Parcel.obtain();
        parcel.writeInt(action);
        parcel.writeFloat(x);
        parcel.writeFloat(y);
        parcel.writeInt(result); // Result
        parcel.writeInt(0); // No Clipdata
        parcel.writeInt(0); // No Clip Description
        parcel.setDataPosition(0);
        return DragEvent.CREATOR.createFromParcel(parcel);
    }

    /**
     * Tests method for {@link TabDragSource#startTabDragAction()}.
     *
     * Checks that it successfully starts drag action process.
     */
    @Test
    public void test_startTabDragAction_ReturnsTrueForValidTab() {
        initializeTest(false, false, 1, 5);
        Tab tabBeingDragged = mStripLayoutHelper.getTabById(mClickedTab.getId());

        // Act and verify.
        mTabDragSource.prepareForDragDrop(mTabsToolbarView, mMultiInstanceManager);
        assertTrue("Failed to start the tag drag action.",
                mTabDragSource.startTabDragAction(
                        mTabsToolbarView, mStripLayoutHelper, tabBeingDragged));
        verify(mTabsToolbarView, atLeastOnce()).startDragAndDrop(any(), any(), any(), anyInt());
        verify(mTabsToolbarView, atMostOnce()).startDragAndDrop(any(), any(), any(), anyInt());
    }

    /**
     * Tests method for {@link TabDragSource#startTabDragAction()}.
     *
     * Checks that it fails to starts drag acion process.
     */
    @Test
    public void test_startTabDragAction_ReturnsFalseForInvalidTab() {
        initializeTest(false, false, 1, 5);

        // Create a StripLayoutTab with bad id, do fail to not get a tab.
        StripLayoutTab invalidIdStripTab = mStripLayoutHelper.createStripTab(Tab.INVALID_TAB_ID);
        Tab tabBeingDragged = mStripLayoutHelper.getTabById(invalidIdStripTab.getId());

        // Act and verify.
        mTabDragSource.prepareForDragDrop(mTabsToolbarView, mMultiInstanceManager);
        assertFalse(mTabDragSource.startTabDragAction(
                mTabsToolbarView, mStripLayoutHelper, tabBeingDragged));
        verify(mTabsToolbarView, never()).startDragAndDrop(any(), any(), any(), anyInt());
    }

    /**
     * Tests method for {@link TabDragSource#prepareForDragDrop()}.
     *
     * Checks that it successfully prepares for the drag process.
     */
    @Test
    public void test_prepareForDragDrop_ReturnTrueForSettingListenerOnce() {
        // Check state
        assertTrue(mTabDragSource.getDropContentReceiver() == null);
        assertTrue(mTabDragSource.getOnDragListenerImpl() == null);

        // Act
        mTabDragSource.prepareForDragDrop(mTabsToolbarView, mMultiInstanceManager);

        // Verify flow.
        assertTrue(mTabDragSource.getDropContentReceiver() != null);
        assertTrue(mTabDragSource.getOnDragListenerImpl() != null);
        verify(mTabsToolbarView, atLeastOnce()).setOnDragListener(any());
        verify(mTabsToolbarView, atMostOnce()).setOnDragListener(any());
        assertTrue(mTabDragSource.getPxToDp() != 0.0f);
    }

    /**
     * Tests method {@link TabDragSource#getDragSourceTabsToolbarHashCode()}.
     *
     * Checks that it successfully gets and sets the source vew hashcode.
     */
    @Test
    public void test_getDragSourceTabsToolbarHashCode_ReturnHashCodeAfterDragAction() {
        initializeTest(false, false, 1, 5);
        Tab tabBeingDragged = mStripLayoutHelper.getTabById(mClickedTab.getId());

        // Act and verify.
        mTabDragSource.prepareForDragDrop(mTabsToolbarView, mMultiInstanceManager);
        assertTrue(mTabDragSource.getDragSourceTabsToolbarHashCode() == 0);
        assertTrue(mTabDragSource.startTabDragAction(
                mTabsToolbarView, mStripLayoutHelper, tabBeingDragged));
        assertTrue(mTabDragSource.getDragSourceTabsToolbarHashCode()
                == System.identityHashCode(mTabsToolbarView));
    }
}
