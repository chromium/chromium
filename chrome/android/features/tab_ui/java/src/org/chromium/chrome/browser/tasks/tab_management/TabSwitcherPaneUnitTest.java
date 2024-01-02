// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.view.View;

import androidx.appcompat.content.res.AppCompatResources;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.hub.DisplayButtonData;
import org.chromium.chrome.browser.hub.FullButtonData;
import org.chromium.chrome.browser.hub.HubContainerView;
import org.chromium.chrome.browser.hub.HubLayoutAnimationType;
import org.chromium.chrome.browser.hub.LoadHint;
import org.chromium.chrome.browser.hub.PaneId;
import org.chromium.chrome.browser.toolbar.TabSwitcherDrawable;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler.BackPressResult;

/** Unit tests for {@link TabSwitcherPane} and {@link TabSwitcherPaneBase}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabSwitcherPaneUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabSwitcherPaneCoordinatorFactory mTabSwitcherPaneCoordinatorFactory;
    @Mock private TabSwitcherPaneCoordinator mTabSwitcherPaneCoordinator;
    @Mock private TabSwitcherPaneDrawableCoordinator mTabSwitcherPaneDrawableCoordinator;
    @Mock private TabSwitcherDrawable mTabSwitcherDrawable;
    @Mock private HubContainerView mHubContainerView;
    @Mock private View.OnClickListener mNewTabButtonClickListener;

    private Context mContext;
    private ObservableSupplierImpl<Boolean> mHandleBackPressChangeSupplier =
            new ObservableSupplierImpl<>();
    private TabSwitcherPane mTabSwitcherPane;
    private int mTimesCreated;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        when(mHubContainerView.getContext()).thenReturn(mContext);
        doAnswer(
                        invocation -> {
                            mTimesCreated++;
                            return mTabSwitcherPaneCoordinator;
                        })
                .when(mTabSwitcherPaneCoordinatorFactory)
                .create(any());
        when(mTabSwitcherPaneCoordinator.getHandleBackPressChangedSupplier())
                .thenReturn(mHandleBackPressChangeSupplier);

        mHandleBackPressChangeSupplier.set(false);
        when(mTabSwitcherPaneDrawableCoordinator.getTabSwitcherDrawable())
                .thenReturn(mTabSwitcherDrawable);
        doAnswer(
                        invocation -> {
                            return mHandleBackPressChangeSupplier.get()
                                    ? BackPressResult.SUCCESS
                                    : BackPressResult.FAILURE;
                        })
                .when(mTabSwitcherPaneCoordinator)
                .handleBackPress();

        mTabSwitcherPane =
                new TabSwitcherPane(
                        mContext,
                        mTabSwitcherPaneCoordinatorFactory,
                        mNewTabButtonClickListener,
                        mTabSwitcherPaneDrawableCoordinator);
    }

    @After
    public void tearDown() {
        mTabSwitcherPane.destroy();
        verify(mTabSwitcherPaneCoordinator, times(mTimesCreated)).destroy();
    }

    @Test
    @SmallTest
    public void testInitWithNativeBeforeCoordinatorCreation() {
        mTabSwitcherPane.initWithNative();
        mTabSwitcherPane.createTabSwitcherPaneCoordinator();
        verify(mTabSwitcherPaneCoordinator).initWithNative();
    }

    @Test
    @SmallTest
    public void testInitWithNativeAfterCoordinatorCreation() {
        mTabSwitcherPane.createTabSwitcherPaneCoordinator();
        verify(mTabSwitcherPaneCoordinator, never()).initWithNative();

        mTabSwitcherPane.initWithNative();
        verify(mTabSwitcherPaneCoordinator).initWithNative();
    }

    @Test
    @SmallTest
    public void testPaneId() {
        assertEquals(PaneId.TAB_SWITCHER, mTabSwitcherPane.getPaneId());
    }

    @Test
    @SmallTest
    public void testLoadHint() {
        // TODO(crbug/1505772): this is a noop right now.
        mTabSwitcherPane.notifyLoadHint(LoadHint.COLD);
        mTabSwitcherPane.notifyLoadHint(LoadHint.WARM);
        mTabSwitcherPane.notifyLoadHint(LoadHint.HOT);
    }

    @Test
    @SmallTest
    public void testGetRootView() {
        assertNotNull(mTabSwitcherPane.getRootView());
    }

    @Test
    @SmallTest
    public void testNewTabButton() {
        FullButtonData buttonData = mTabSwitcherPane.getActionButtonDataSupplier().get();

        assertEquals(mContext.getString(R.string.button_new_tab), buttonData.resolveText(mContext));
        assertEquals(
                mContext.getString(R.string.button_new_tab),
                buttonData.resolveContentDescription(mContext));
        assertTrue(
                AppCompatResources.getDrawable(mContext, R.drawable.new_tab_icon)
                        .getConstantState()
                        .equals(buttonData.resolveIcon(mContext).getConstantState()));

        buttonData.getOnPressRunnable().run();
        verify(mNewTabButtonClickListener).onClick(isNull());
    }

    @Test
    @SmallTest
    public void testReferenceButton() {
        DisplayButtonData buttonData = mTabSwitcherPane.getReferenceButtonDataSupplier().get();

        assertEquals(
                mContext.getString(R.string.accessibility_tab_switcher),
                buttonData.resolveText(mContext));
        assertEquals(
                mContext.getString(R.string.accessibility_tab_switcher),
                buttonData.resolveContentDescription(mContext));
        assertEquals(mTabSwitcherDrawable, buttonData.resolveIcon(mContext));
    }

    @Test
    @SmallTest
    public void testBackPress() {
        ObservableSupplier<Boolean> handlesBackPressSupplier =
                mTabSwitcherPane.getHandleBackPressChangedSupplier();
        assertNull(handlesBackPressSupplier.get());
        assertEquals(BackPressResult.FAILURE, mTabSwitcherPane.handleBackPress());

        mTabSwitcherPane.initWithNative();
        mTabSwitcherPane.createTabSwitcherPaneCoordinator();
        assertFalse(handlesBackPressSupplier.get());
        assertEquals(BackPressResult.FAILURE, mTabSwitcherPane.handleBackPress());

        mHandleBackPressChangeSupplier.set(true);
        assertTrue(handlesBackPressSupplier.get());
        assertEquals(BackPressResult.SUCCESS, mTabSwitcherPane.handleBackPress());
    }

    @Test
    @SmallTest
    public void testCreatesAnimatorProviders() {
        // TODO(crbug/1505772): This test will need to be reworked to confirm shrink expand
        // animators are correctly created. This is a temporary test for coverage.
        assertEquals(
                HubLayoutAnimationType.FADE_OUT,
                mTabSwitcherPane
                        .createHideHubLayoutAnimatorProvider(mHubContainerView)
                        .getPlannedAnimationType());
        assertEquals(
                HubLayoutAnimationType.FADE_IN,
                mTabSwitcherPane
                        .createShowHubLayoutAnimatorProvider(mHubContainerView)
                        .getPlannedAnimationType());
    }

    @Test
    @SmallTest
    public void testGetDialogVisibilitySupplier() {
        assertNull(mTabSwitcherPane.getTabGridDialogVisibilitySupplier());

        mTabSwitcherPane.initWithNative();
        mTabSwitcherPane.createTabSwitcherPaneCoordinator();

        assertNull(mTabSwitcherPane.getTabSwitcherCustomViewManager());
        verify(mTabSwitcherPaneCoordinator).getTabSwitcherCustomViewManager();
    }

    @Test
    @SmallTest
    public void testGetCustomViewManager() {
        assertNull(mTabSwitcherPane.getTabSwitcherCustomViewManager());

        mTabSwitcherPane.initWithNative();
        mTabSwitcherPane.createTabSwitcherPaneCoordinator();

        assertNull(mTabSwitcherPane.getTabSwitcherCustomViewManager());
        verify(mTabSwitcherPaneCoordinator).getTabSwitcherCustomViewManager();
    }

    @Test
    @SmallTest
    public void testGetTabListModelSize() {
        assertEquals(0, mTabSwitcherPane.getTabSwitcherTabListModelSize());

        mTabSwitcherPane.initWithNative();
        mTabSwitcherPane.createTabSwitcherPaneCoordinator();

        int tabCount = 5;
        when(mTabSwitcherPaneCoordinator.getTabSwitcherTabListModelSize()).thenReturn(tabCount);

        assertEquals(tabCount, mTabSwitcherPane.getTabSwitcherTabListModelSize());
    }

    @Test
    @SmallTest
    public void testSetRecyclerViewPosition() {
        RecyclerViewPosition position = new RecyclerViewPosition(1, 5);
        mTabSwitcherPane.setTabSwitcherRecyclerViewPosition(position);
        verify(mTabSwitcherPaneCoordinator, never()).setTabSwitcherRecyclerViewPosition(any());

        mTabSwitcherPane.initWithNative();
        mTabSwitcherPane.createTabSwitcherPaneCoordinator();

        mTabSwitcherPane.setTabSwitcherRecyclerViewPosition(position);
        verify(mTabSwitcherPaneCoordinator).setTabSwitcherRecyclerViewPosition(position);
    }
}
