// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.os.SystemClock;
import android.view.MotionEvent;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.FrameLayout.LayoutParams;

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
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButtonCoordinator;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityClient;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController.MenuOrKeyboardActionHandler;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.feature_engagement.Tracker;
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
    @Mock private HubShowPaneHelper mHubShowPaneHelper;
    @Mock private DisplayButtonData mReferenceButtonData;
    @Mock private ProfileProvider mProfileProvider;
    @Mock private Profile mProfile;
    @Mock private Tracker mTracker;
    @Mock private SearchActivityClient mSearchActivityClient;

    private final ObservableSupplierImpl<Tab> mTabSupplier = new ObservableSupplierImpl<>();
    private final ObservableSupplierImpl<DisplayButtonData> mReferenceButtonDataSupplier =
            new ObservableSupplierImpl<>();
    private final ObservableSupplierImpl<FullButtonData> mActionButtonDataSupplier =
            new ObservableSupplierImpl<>();
    private final OneshotSupplierImpl<ProfileProvider> mProfileProviderSupplier =
            new OneshotSupplierImpl<>();
    private final ObservableSupplierImpl<EdgeToEdgeController> mEdgeToEdgeSupplier =
            new ObservableSupplierImpl<>();
    private final int mSnackbarOverrideToken = 1;

    private Activity mActivity;
    private FrameLayout mRootView;

    @Before
    public void setUp() {
        TrackerFactory.setTrackerForTests(mTracker);
        mReferenceButtonDataSupplier.set(mReferenceButtonData);
        mProfileProviderSupplier.set(mProfileProvider);
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
        when(mProfileProvider.getOriginalProfile()).thenReturn(mProfile);

        when(mSnackbarManager.pushParentViewToOverrideStack(any()))
                .thenReturn(mSnackbarOverrideToken);

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
                        mProfileProviderSupplier,
                        builder,
                        mBackPressManager,
                        mMenuOrKeyboardActionController,
                        mSnackbarManager,
                        mTabSupplier,
                        mMenuButtonCoordinator,
                        mHubShowPaneHelper,
                        mEdgeToEdgeSupplier,
                        mSearchActivityClient);

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
                        mProfileProviderSupplier,
                        builder,
                        mBackPressManager,
                        mMenuOrKeyboardActionController,
                        mSnackbarManager,
                        mTabSupplier,
                        mMenuButtonCoordinator,
                        mHubShowPaneHelper,
                        mEdgeToEdgeSupplier,
                        mSearchActivityClient);
        hubManager.getPaneManager().focusPane(PaneId.TAB_SWITCHER);

        HubController hubController = hubManager.getHubController();
        hubController.setHubLayoutController(mHubLayoutController);
        assertNull(hubManager.getHubCoordinatorForTesting());

        hubController.onHubLayoutShow();
        HubCoordinator coordinator = hubManager.getHubCoordinatorForTesting();
        assertNotNull(coordinator);
        verify(mBackPressManager).addHandler(eq(coordinator), eq(BackPressHandler.Type.HUB));
        verify(mTabSwitcherPane).setPaneHubController(coordinator);
        verify(mMenuOrKeyboardActionController)
                .registerMenuOrKeyboardActionHandler(mTabSwitcherMenuOrKeyboardActionHandler);

        FrameLayout containerView = hubController.getContainerView();
        assertNotNull(containerView);
        verify(mSnackbarManager).pushParentViewToOverrideStack(any());

        // Attach the container to the parent view.
        mRootView.addView(containerView);
        assertEquals(mRootView, containerView.getParent());

        hubManager.getPaneManager().focusPane(PaneId.INCOGNITO_TAB_SWITCHER);
        verify(mTabSwitcherPane).setPaneHubController(null);
        verify(mSnackbarManager).popParentViewFromOverrideStack(mSnackbarOverrideToken);
        verify(mMenuOrKeyboardActionController)
                .unregisterMenuOrKeyboardActionHandler(mTabSwitcherMenuOrKeyboardActionHandler);
        verify(mIncognitoTabSwitcherPane).setPaneHubController(coordinator);
        verify(mSnackbarManager, times(2)).pushParentViewToOverrideStack(any());
        verify(mMenuOrKeyboardActionController)
                .registerMenuOrKeyboardActionHandler(
                        mIncognitoTabSwitcherMenuOrKeyboardActionHandler);

        hubController.onHubLayoutDoneHiding();
        assertNull(hubManager.getHubCoordinatorForTesting());
        verify(mBackPressManager).removeHandler(eq(coordinator));
        verify(mIncognitoTabSwitcherPane).setPaneHubController(null);
        verify(mSnackbarManager, times(2)).popParentViewFromOverrideStack(mSnackbarOverrideToken);
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
                        mProfileProviderSupplier,
                        builder,
                        mBackPressManager,
                        mMenuOrKeyboardActionController,
                        mSnackbarManager,
                        mTabSupplier,
                        mMenuButtonCoordinator,
                        mHubShowPaneHelper,
                        mEdgeToEdgeSupplier,
                        mSearchActivityClient);
        HubController hubController = hubManager.getHubController();
        hubController.setHubLayoutController(mHubLayoutController);

        assertFalse(hubController.onHubLayoutBackPressed());

        hubController.onHubLayoutShow();

        assertFalse(hubController.onHubLayoutBackPressed());

        mTabSupplier.set(mTab);
        assertTrue(hubController.onHubLayoutBackPressed());

        verify(mHubLayoutController).selectTabAndHideHubLayout(eq(TAB_ID));
    }

    @Test
    @SmallTest
    public void testConsumeTouchEvents() {
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
                        mProfileProviderSupplier,
                        builder,
                        mBackPressManager,
                        mMenuOrKeyboardActionController,
                        mSnackbarManager,
                        mTabSupplier,
                        mMenuButtonCoordinator,
                        mHubShowPaneHelper,
                        mEdgeToEdgeSupplier,
                        mSearchActivityClient);
        hubManager.getPaneManager().focusPane(PaneId.TAB_SWITCHER);

        HubController hubController = hubManager.getHubController();
        hubController.setHubLayoutController(mHubLayoutController);
        hubController.onHubLayoutShow();

        FrameLayout containerView = hubController.getContainerView();
        long eventTime = SystemClock.uptimeMillis();
        assertTrue(
                containerView.onTouchEvent(
                        MotionEvent.obtain(
                                eventTime + 100,
                                eventTime,
                                MotionEvent.ACTION_DOWN,
                                /* x= */ 100,
                                /* y= */ 100,
                                0)));
    }

    @Test
    @SmallTest
    public void testStatusIndicatorHeight() {
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
                        mProfileProviderSupplier,
                        builder,
                        mBackPressManager,
                        mMenuOrKeyboardActionController,
                        mSnackbarManager,
                        mTabSupplier,
                        mMenuButtonCoordinator,
                        mHubShowPaneHelper,
                        mEdgeToEdgeSupplier,
                        mSearchActivityClient);
        hubManager.getPaneManager().focusPane(PaneId.TAB_SWITCHER);

        HubController hubController = hubManager.getHubController();
        hubController.setHubLayoutController(mHubLayoutController);
        hubController.onHubLayoutShow();

        int statusIndicatorHeight = 50;
        hubManager.setStatusIndicatorHeight(statusIndicatorHeight);
        FrameLayout containerView = hubController.getContainerView();
        assertEquals(
                statusIndicatorHeight, ((LayoutParams) containerView.getLayoutParams()).topMargin);

        mRootView.addView(containerView);
        assertEquals(
                statusIndicatorHeight, ((LayoutParams) containerView.getLayoutParams()).topMargin);

        statusIndicatorHeight = 0;
        hubManager.setStatusIndicatorHeight(statusIndicatorHeight);
        assertEquals(
                statusIndicatorHeight, ((LayoutParams) containerView.getLayoutParams()).topMargin);

        mRootView.removeView(containerView);
        assertEquals(
                statusIndicatorHeight, ((LayoutParams) containerView.getLayoutParams()).topMargin);
    }

    @Test
    @SmallTest
    public void testAppHeaderHeight() {
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
                        mProfileProviderSupplier,
                        builder,
                        mBackPressManager,
                        mMenuOrKeyboardActionController,
                        mSnackbarManager,
                        mTabSupplier,
                        mMenuButtonCoordinator,
                        mHubShowPaneHelper,
                        mEdgeToEdgeSupplier,
                        mSearchActivityClient);
        hubManager.getPaneManager().focusPane(PaneId.TAB_SWITCHER);

        HubController hubController = hubManager.getHubController();
        hubController.setHubLayoutController(mHubLayoutController);
        hubController.onHubLayoutShow();

        int appHeaderHeight = 75;
        hubManager.setAppHeaderHeight(appHeaderHeight);
        FrameLayout containerView = hubController.getContainerView();
        assertEquals(appHeaderHeight, ((LayoutParams) containerView.getLayoutParams()).topMargin);

        mRootView.addView(containerView);
        assertEquals(appHeaderHeight, ((LayoutParams) containerView.getLayoutParams()).topMargin);

        appHeaderHeight = 0;
        hubManager.setAppHeaderHeight(appHeaderHeight);
        assertEquals(appHeaderHeight, ((LayoutParams) containerView.getLayoutParams()).topMargin);

        mRootView.removeView(containerView);
        assertEquals(appHeaderHeight, ((LayoutParams) containerView.getLayoutParams()).topMargin);
    }
}
