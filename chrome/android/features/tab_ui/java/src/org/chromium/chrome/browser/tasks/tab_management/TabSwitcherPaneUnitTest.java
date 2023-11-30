// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.view.View;
import android.view.ViewGroup;

import androidx.appcompat.content.res.AppCompatResources;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

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

/** Unit tests for {@link TabSwitcherPane}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabSwitcherPaneUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabSwitcher mTabSwitcher;
    @Mock private TabSwitcher.Controller mTabSwitcherController;
    @Mock private TabSwitcherPaneDrawableCoordinator mTabSwitcherPaneDrawableCoordinator;
    @Mock private TabSwitcherDrawable mTabSwitcherDrawable;
    @Mock private HubContainerView mHubContainerView;
    @Mock private ViewGroup mContainerView;
    @Mock private View.OnClickListener mNewTabButtonClickListener;

    private Context mContext;
    private ObservableSupplierImpl<Boolean> mHandleBackPressChangeSupplier =
            new ObservableSupplierImpl<>();
    private TabSwitcherPane mTabSwitcherPane;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        when(mHubContainerView.getContext()).thenReturn(mContext);
        when(mTabSwitcher.getController()).thenReturn(mTabSwitcherController);
        mHandleBackPressChangeSupplier.set(false);
        when(mTabSwitcherController.getTabSwitcherContainer()).thenReturn(mContainerView);
        when(mTabSwitcherController.getHandleBackPressChangedSupplier())
                .thenReturn(mHandleBackPressChangeSupplier);
        when(mTabSwitcherPaneDrawableCoordinator.getTabSwitcherDrawable())
                .thenReturn(mTabSwitcherDrawable);
        doAnswer(
                        invocation -> {
                            return mHandleBackPressChangeSupplier.get()
                                    ? BackPressResult.SUCCESS
                                    : BackPressResult.FAILURE;
                        })
                .when(mTabSwitcherController)
                .handleBackPress();

        mTabSwitcherPane =
                new TabSwitcherPane(
                        mTabSwitcher,
                        mNewTabButtonClickListener,
                        mTabSwitcherPaneDrawableCoordinator);
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
        assertEquals(mContainerView, mTabSwitcherPane.getRootView());
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
}
