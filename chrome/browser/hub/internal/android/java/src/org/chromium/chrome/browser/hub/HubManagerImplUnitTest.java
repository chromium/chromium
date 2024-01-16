// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButtonCoordinator;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController.MenuOrKeyboardActionHandler;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.ui.base.TestActivity;

/** Unit tests for {@link PaneManagerImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
public class HubManagerImplUnitTest {
    private static final int TAB_ID = 8;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private BackPressManager mBackPressManager;
    @Mock private Tab mTab;
    @Mock private Pane mTabSwitcherPane;
    @Mock private ViewGroup mTabSwitcherPaneView;
    @Mock private MenuOrKeyboardActionHandler mTabSwitcherMenuOrKeyboardActionHandler;
    @Mock private Pane mIncognitoTabSwitcherPane;
    @Mock private ViewGroup mIncognitoTabSwitcherPaneView;
    @Mock private MenuOrKeyboardActionHandler mIncognitoTabSwitcherMenuOrKeyboardActionHandler;
    @Mock private HubLayoutController mHubLayoutController;
    @Mock private ObservableSupplier<Integer> mPreviousLayoutTypeSupplier;
    @Mock private MenuOrKeyboardActionController mMenuOrKeyboardActionController;
    @Mock private SnackbarManager mSnackbarManager;
    @Mock private MenuButtonCoordinator mMenuButtonCoordinator;

    private final ObservableSupplierImpl<Tab> mTabSupplier = new ObservableSupplierImpl<>();
    private final ObservableSupplierImpl<DisplayButtonData> mReferenceButtonDataSupplier =
            new ObservableSupplierImpl<>();
    private final ObservableSupplierImpl<FullButtonData> mActionButtonDataSupplier =
            new ObservableSupplierImpl<>();

    private Activity mActivity;
    private FrameLayout mRootView;

    @Before
    public void setUp() {
        when(mTabSwitcherPane.getPaneId()).thenReturn(PaneId.TAB_SWITCHER);
        when(mTabSwitcherPane.getReferenceButtonDataSupplier())
                .thenReturn(mReferenceButtonDataSupplier);
        when(mTabSwitcherPane.getActionButtonDataSupplier()).thenReturn(mActionButtonDataSupplier);
        when(mTabSwitcherPane.getRootView()).thenReturn(mTabSwitcherPaneView);
        when(mTabSwitcherPane.getMenuOrKeyboardActionHandler())
                .thenReturn(mTabSwitcherMenuOrKeyboardActionHandler);

        when(mIncognitoTabSwitcherPane.getPaneId()).thenReturn(PaneId.INCOGNITO_TAB_SWITCHER);
        when(mIncognitoTabSwitcherPane.getReferenceButtonDataSupplier())
                .thenReturn(mReferenceButtonDataSupplier);
        when(mIncognitoTabSwitcherPane.getActionButtonDataSupplier())
                .thenReturn(mActionButtonDataSupplier);
        when(mIncognitoTabSwitcherPane.getRootView()).thenReturn(mIncognitoTabSwitcherPaneView);
        when(mIncognitoTabSwitcherPane.getMenuOrKeyboardActionHandler())
                .thenReturn(mIncognitoTabSwitcherMenuOrKeyboardActionHandler);

        when(mHubLayoutController.getPreviousLayoutTypeSupplier())
                .thenReturn(mPreviousLayoutTypeSupplier);
        when(mTab.getId()).thenReturn(TAB_ID);

        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        (activity) -> {
                            mActivity = activity;
                            mRootView = new FrameLayout(mActivity);
                            mActivity.setContentView(mRootView);
                        });
    }

    @Test
    @SmallTest
    public void testCreatesPaneManager() {
        PaneListBuilder builder =
                new PaneListBuilder(new DefaultPaneOrderController())
                        .registerPane(
                                PaneId.TAB_SWITCHER,
                                LazyOneshotSupplier.fromValue(mTabSwitcherPane))
                        .registerPane(
                                PaneId.INCOGNITO_TAB_SWITCHER,
                                LazyOneshotSupplier.fromValue(mIncognitoTabSwitcherPane));
        HubManager hubManager =
                HubManagerFactory.createHubManager(
                        mActivity,
                        builder,
                        mBackPressManager,
                        mMenuOrKeyboardActionController,
                        mSnackbarManager,
                        mTabSupplier,
                        mMenuButtonCoordinator);

        PaneManager paneManager = hubManager.getPaneManager();
        assertNotNull(paneManager);
        assertNull(paneManager.getFocusedPaneSupplier().get());
        assertTrue(paneManager.focusPane(PaneId.TAB_SWITCHER));
        assertEquals(mTabSwitcherPane, paneManager.getFocusedPaneSupplier().get());
    }

    @Test
    @SmallTest
    public void testHubController() {
        PaneListBuilder builder =
                new PaneListBuilder(new DefaultPaneOrderController())
                        .registerPane(
                                PaneId.TAB_SWITCHER,
                                LazyOneshotSupplier.fromValue(mTabSwitcherPane))
                        .registerPane(
                                PaneId.INCOGNITO_TAB_SWITCHER,
                                LazyOneshotSupplier.fromValue(mIncognitoTabSwitcherPane));
        HubManagerImpl hubManager =
                new HubManagerImpl(
                        mActivity,
                        builder,
                        mBackPressManager,
                        mMenuOrKeyboardActionController,
                        mSnackbarManager,
                        mTabSupplier,
                        mMenuButtonCoordinator);
        hubManager.getPaneManager().focusPane(PaneId.TAB_SWITCHER);

        HubController hubController = hubManager.getHubController();
        hubController.setHubLayoutController(mHubLayoutController);
        assertNull(hubManager.getHubCoordinatorForTesting());

        hubController.onHubLayoutShow();
        HubCoordinator coordinator = hubManager.getHubCoordinatorForTesting();
        assertNotNull(coordinator);
        verify(mBackPressManager).addHandler(eq(coordinator), eq(BackPressHandler.Type.HUB));
        verify(mTabSwitcherPane).setPaneHubController(coordinator);
        verify(mSnackbarManager).setParentView(mTabSwitcherPaneView);
        verify(mMenuOrKeyboardActionController)
                .registerMenuOrKeyboardActionHandler(mTabSwitcherMenuOrKeyboardActionHandler);

        View containerView = hubController.getContainerView();
        assertNotNull(containerView);

        // Attach the container to the parent view.
        mRootView.addView(containerView);
        assertEquals(mRootView, containerView.getParent());

        hubManager.getPaneManager().focusPane(PaneId.INCOGNITO_TAB_SWITCHER);
        verify(mTabSwitcherPane).setPaneHubController(null);
        verify(mSnackbarManager).setParentView(null);
        verify(mMenuOrKeyboardActionController)
                .unregisterMenuOrKeyboardActionHandler(mTabSwitcherMenuOrKeyboardActionHandler);
        verify(mIncognitoTabSwitcherPane).setPaneHubController(coordinator);
        verify(mSnackbarManager).setParentView(mIncognitoTabSwitcherPaneView);
        verify(mMenuOrKeyboardActionController)
                .registerMenuOrKeyboardActionHandler(
                        mIncognitoTabSwitcherMenuOrKeyboardActionHandler);

        hubController.onHubLayoutDoneHiding();
        assertNull(hubManager.getHubCoordinatorForTesting());
        verify(mBackPressManager).removeHandler(eq(coordinator));
        verify(mIncognitoTabSwitcherPane).setPaneHubController(null);
        verify(mSnackbarManager, times(2)).setParentView(null);
        verify(mMenuOrKeyboardActionController)
                .unregisterMenuOrKeyboardActionHandler(
                        mIncognitoTabSwitcherMenuOrKeyboardActionHandler);

        // Container is still attached and will be removed separately.
        assertEquals(mRootView, containerView.getParent());
    }

    @Test
    @SmallTest
    public void testBackNavigation() {
        PaneListBuilder builder = new PaneListBuilder(new DefaultPaneOrderController());
        HubManagerImpl hubManager =
                new HubManagerImpl(
                        mActivity,
                        builder,
                        mBackPressManager,
                        mMenuOrKeyboardActionController,
                        mSnackbarManager,
                        mTabSupplier,
                        mMenuButtonCoordinator);
        HubController hubController = hubManager.getHubController();
        hubController.setHubLayoutController(mHubLayoutController);

        assertFalse(hubController.onHubLayoutBackPressed());

        hubController.onHubLayoutShow();

        assertFalse(hubController.onHubLayoutBackPressed());

        mTabSupplier.set(mTab);
        assertTrue(hubController.onHubLayoutBackPressed());

        verify(mHubLayoutController).selectTabAndHideHubLayout(eq(TAB_ID));
    }
}
