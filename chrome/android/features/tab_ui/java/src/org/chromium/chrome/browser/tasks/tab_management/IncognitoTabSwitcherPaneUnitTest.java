// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.content.Context;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.test.core.app.ApplicationProvider;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.hub.DisplayButtonData;
import org.chromium.chrome.browser.hub.FullButtonData;
import org.chromium.chrome.browser.hub.LoadHint;
import org.chromium.chrome.browser.hub.PaneHubController;
import org.chromium.chrome.browser.hub.PaneId;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthController;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthManager.IncognitoReauthCallback;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.IncognitoTabModel;
import org.chromium.chrome.browser.tabmodel.IncognitoTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabClosingSource;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.sensitive_content.SensitiveContentFeatures;

import java.util.List;
import java.util.function.DoubleConsumer;

/**
 * Unit tests for {@link IncognitoTabSwitcherPane}. Refer to {@link TabSwitcherPaneUnitTest} for
 * tests for shared functionality with {@link TabSwitcherPaneBase}.
 */
@DisableFeatures({
    ChromeFeatureList.DATA_SHARING,
    ChromeFeatureList.DATA_SHARING_JOIN_ONLY,
    SensitiveContentFeatures.SENSITIVE_CONTENT,
    SensitiveContentFeatures.SENSITIVE_CONTENT_WHILE_SWITCHING_TABS
})
@RunWith(BaseRobolectricTestRunner.class)
public class IncognitoTabSwitcherPaneUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private IncognitoReauthController mIncognitoReauthController;
    @Mock private TabSwitcherPaneCoordinatorFactory mTabSwitcherPaneCoordinatorFactory;
    @Mock private TabSwitcherPaneCoordinator mTabSwitcherPaneCoordinator;
    @Mock private View.OnClickListener mNewTabButtonClickListener;
    @Mock private TabGroupModelFilter mTabGroupModelFilter;
    @Mock private IncognitoTabModel mIncognitoTabModel;
    @Mock private PaneHubController mPaneHubController;
    @Mock private DoubleConsumer mOnAlphaChange;
    @Mock private UserEducationHelper mUserEducationHelper;
    @Mock private Profile mProfile;
    @Mock private TabGroupCreationUiDelegate mUiFlow;

    @Captor private ArgumentCaptor<IncognitoTabModelObserver> mIncognitoTabModelObserverCaptor;
    @Captor private ArgumentCaptor<IncognitoReauthCallback> mIncognitoReauthCallbackCaptor;
    @Captor private ArgumentCaptor<Callback<Integer>> mOnTabClickedCallbackCaptor;
    @Captor private ArgumentCaptor<TabModelObserver> mTabModelObserverCaptor;

    private final OneshotSupplierImpl<IncognitoReauthController>
            mIncognitoReauthControllerSupplier = new OneshotSupplierImpl<>();
    private final SettableMonotonicObservableSupplier<EdgeToEdgeController> mEdgeToEdgeSupplier =
            ObservableSuppliers.createMonotonic();
    private final SettableMonotonicObservableSupplier<CompositorViewHolder>
            mCompositorViewHolderSupplier = ObservableSuppliers.createMonotonic();
    private final SettableNonNullObservableSupplier<Boolean>
            mIsRecyclerViewAnimatorRunningSupplier = ObservableSuppliers.createNonNull(false);
    private final SettableNonNullObservableSupplier<Boolean>
            mTabGridDialogShowingOrAnimationSupplier = ObservableSuppliers.createNonNull(false);
    private final SettableNonNullObservableSupplier<Integer> mRecentlySwipedTabIdSupplier =
            ObservableSuppliers.createNonNull(Tab.INVALID_TAB_ID);

    private Context mContext;
    private IncognitoTabSwitcherPane mIncognitoTabSwitcherPane;
    private int mTimesCreated;
    private List<Tab> mTabList;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();

        doAnswer(
                        invocation -> {
                            mTimesCreated++;
                            return mTabSwitcherPaneCoordinator;
                        })
                .when(mTabSwitcherPaneCoordinatorFactory)
                .create(
                        any(),
                        any(),
                        any(),
                        any(),
                        mOnTabClickedCallbackCaptor.capture(),
                        anyBoolean(),
                        any(),
                        any(),
                        any(),
                        any());

        mTabList = List.of(mock(Tab.class));
        when(mTabGroupModelFilter.getRepresentativeTabList()).thenReturn(mTabList);
        when(mTabGroupModelFilter.getTabModel()).thenReturn(mIncognitoTabModel);
        when(mTabGroupModelFilter.isTabModelRestored()).thenReturn(true);
        when(mTabSwitcherPaneCoordinator.getIsRecyclerViewAnimatorRunning())
                .thenReturn(mIsRecyclerViewAnimatorRunningSupplier);
        when(mTabSwitcherPaneCoordinator.getRecentlySwipedTabIdSupplier())
                .thenReturn(mRecentlySwipedTabIdSupplier);
        when(mTabSwitcherPaneCoordinator.getTabGridDialogShowingOrAnimationSupplier())
                .thenReturn(mTabGridDialogShowingOrAnimationSupplier);

        mIncognitoTabSwitcherPane =
                new IncognitoTabSwitcherPane(
                        mContext,
                        mTabSwitcherPaneCoordinatorFactory,
                        () -> mTabGroupModelFilter,
                        mNewTabButtonClickListener,
                        mIncognitoReauthControllerSupplier,
                        mOnAlphaChange,
                        mUserEducationHelper,
                        mEdgeToEdgeSupplier,
                        mCompositorViewHolderSupplier,
                        mUiFlow,
                        /* xrSpaceModeObservableSupplier= */ null);
    }

    @After
    public void tearDown() {
        mIncognitoTabSwitcherPane.destroy();
        verify(mTabSwitcherPaneCoordinator, times(mTimesCreated)).destroy();

        var incognitoTabModelObservers = mIncognitoTabModelObserverCaptor.getAllValues();
        if (incognitoTabModelObservers.isEmpty()) {
            verify(mIncognitoTabModel, never()).removeIncognitoObserver(any());
        } else {
            verify(mIncognitoTabModel).removeIncognitoObserver(incognitoTabModelObservers.get(0));
        }
    }

    @Test
    public void testInitWithNativeHasIncognitoTabs() {
        when(mIncognitoTabModel.getCount()).thenReturn(1);
        mIncognitoTabSwitcherPane.initWithNative();
        verify(mIncognitoTabModel).addIncognitoObserver(mIncognitoTabModelObserverCaptor.capture());
        DisplayButtonData buttonData =
                mIncognitoTabSwitcherPane.getReferenceButtonDataSupplier().get();
        assertNotNull(buttonData);

        checkIncognitoTabModelObserverAndButtonData();
    }

    @Test
    public void testInitWithNativeHasNoIncognitoTabs() {
        when(mIncognitoTabModel.getCount()).thenReturn(0);
        mIncognitoTabSwitcherPane.initWithNative();
        verify(mIncognitoTabModel).addIncognitoObserver(mIncognitoTabModelObserverCaptor.capture());
        assertNull(mIncognitoTabSwitcherPane.getReferenceButtonDataSupplier().get());

        checkIncognitoTabModelObserverAndButtonData();
    }

    @Test
    public void testPaneId() {
        assertEquals(PaneId.INCOGNITO_TAB_SWITCHER, mIncognitoTabSwitcherPane.getPaneId());
    }

    @Test
    public void testNewTabButtonData() {
        checkNewTabButton(/* enabled= */ false);

        mIncognitoReauthControllerSupplier.set(mIncognitoReauthController);
        RobolectricUtil.runAllBackgroundAndUi();
        verify(mIncognitoReauthController)
                .addIncognitoReauthCallback(mIncognitoReauthCallbackCaptor.capture());
        when(mIncognitoReauthController.isIncognitoReauthPending()).thenReturn(false);
        when(mIncognitoReauthController.isReauthPageShowing()).thenReturn(false);
        mIncognitoTabSwitcherPane.notifyLoadHint(LoadHint.HOT);

        checkNewTabButton(/* enabled= */ true);

        when(mIncognitoReauthController.isIncognitoReauthPending()).thenReturn(true);
        when(mIncognitoReauthController.isReauthPageShowing()).thenReturn(true);
        mIncognitoTabSwitcherPane.showAllTabs();

        checkNewTabButton(/* enabled= */ false);
    }

    @Test
    public void testIncognitoReauthCallback() {
        assertTrue(mIncognitoTabSwitcherPane.getHubSearchEnabledStateSupplier().get());
        checkNewTabButton(/* enabled= */ false);

        mIncognitoReauthControllerSupplier.set(mIncognitoReauthController);
        RobolectricUtil.runAllBackgroundAndUi();
        verify(mIncognitoReauthController)
                .addIncognitoReauthCallback(mIncognitoReauthCallbackCaptor.capture());
        IncognitoReauthCallback callback = mIncognitoReauthCallbackCaptor.getValue();

        mIncognitoTabSwitcherPane.createTabSwitcherPaneCoordinator();
        TabSwitcherPaneCoordinator coordinator =
                mIncognitoTabSwitcherPane.getTabSwitcherPaneCoordinator();
        reset(coordinator);

        callback.onIncognitoReauthNotPossible();
        callback.onIncognitoReauthFailure();
        verifyNoInteractions(coordinator);

        mIncognitoTabSwitcherPane.initWithNative();
        verify(mIncognitoTabModel).addIncognitoObserver(mIncognitoTabModelObserverCaptor.capture());
        verify(coordinator).initWithNative();

        when(mIncognitoReauthController.isIncognitoReauthPending()).thenReturn(true);
        when(mIncognitoReauthController.isReauthPageShowing()).thenReturn(true);
        when(mIncognitoTabModel.isActiveModel()).thenReturn(true);
        mIncognitoTabSwitcherPane.notifyLoadHint(LoadHint.HOT);
        verify(coordinator).resetWithListOfTabs(null);
        checkNewTabButton(/* enabled= */ false);
        assertFalse(mIncognitoTabSwitcherPane.getHubSearchEnabledStateSupplier().get());

        when(mIncognitoReauthController.isIncognitoReauthPending()).thenReturn(false);
        when(mIncognitoReauthController.isReauthPageShowing()).thenReturn(false);
        callback.onIncognitoReauthSuccess();
        verify(coordinator).resetWithListOfTabs(mTabList);
        verify(coordinator, times(2)).setInitialScrollIndexOffset();
        verify(coordinator).requestAccessibilityFocusOnCurrentTab();
        checkNewTabButton(/* enabled= */ true);
        assertTrue(mIncognitoTabSwitcherPane.getHubSearchEnabledStateSupplier().get());

        // Check not called again
        mIncognitoTabSwitcherPane.notifyLoadHint(LoadHint.WARM);
        callback.onIncognitoReauthSuccess();
        verifyNoMoreInteractions(coordinator);
        mIncognitoTabSwitcherPane.notifyLoadHint(LoadHint.HOT);
        verify(coordinator, times(2)).resetWithListOfTabs(mTabList);
        verify(coordinator, times(3)).setInitialScrollIndexOffset();
        verify(coordinator, times(2)).requestAccessibilityFocusOnCurrentTab();
        checkNewTabButton(/* enabled= */ true);
        assertTrue(mIncognitoTabSwitcherPane.getHubSearchEnabledStateSupplier().get());

        when(mIncognitoTabModel.isActiveModel()).thenReturn(false);
        callback.onIncognitoReauthSuccess();
        verifyNoMoreInteractions(coordinator);
    }

    @Test
    public void testResetWithTabList() {
        mIncognitoTabSwitcherPane.resetWithListOfTabs(null);

        mIncognitoTabSwitcherPane.createTabSwitcherPaneCoordinator();
        TabSwitcherPaneCoordinator coordinator =
                mIncognitoTabSwitcherPane.getTabSwitcherPaneCoordinator();

        mIncognitoTabSwitcherPane.resetWithListOfTabs(null);
        verify(coordinator).resetWithListOfTabs(null);

        when(mIncognitoTabModel.isActiveModel()).thenReturn(true);
        mIncognitoTabSwitcherPane.showAllTabs();
        verify(coordinator, times(2)).resetWithListOfTabs(null);
        when(mIncognitoTabModel.isActiveModel()).thenReturn(false);

        mIncognitoTabSwitcherPane.notifyLoadHint(LoadHint.HOT);
        verify(coordinator, times(3)).resetWithListOfTabs(null);

        when(mIncognitoTabModel.isActiveModel()).thenReturn(true);
        mIncognitoTabSwitcherPane.showAllTabs();
        verify(coordinator).resetWithListOfTabs(mTabList);
    }

    @Test
    public void testLoadHintColdWarmHotCold() {
        mIncognitoTabSwitcherPane.notifyLoadHint(LoadHint.COLD);
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        assertNull(mIncognitoTabSwitcherPane.getTabSwitcherPaneCoordinator());

        mIncognitoTabSwitcherPane.notifyLoadHint(LoadHint.WARM);
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        assertNull(mIncognitoTabSwitcherPane.getTabSwitcherPaneCoordinator());

        mIncognitoTabSwitcherPane.notifyLoadHint(LoadHint.HOT);
        TabSwitcherPaneCoordinator coordinator =
                mIncognitoTabSwitcherPane.getTabSwitcherPaneCoordinator();
        assertNotNull(coordinator);
        verify(coordinator, never()).softCleanup();
        verify(coordinator, never()).hardCleanup();

        mIncognitoTabSwitcherPane.notifyLoadHint(LoadHint.COLD);
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        assertNull(mIncognitoTabSwitcherPane.getTabSwitcherPaneCoordinator());
        verify(coordinator).softCleanup();
        verify(coordinator).hardCleanup();
    }

    @Test
    public void testLoadHintColdHot_TabStateNotInitialized() {
        when(mIncognitoTabModel.isActiveModel()).thenReturn(true);
        when(mTabGroupModelFilter.isTabModelRestored()).thenReturn(false);

        mIncognitoTabSwitcherPane.notifyLoadHint(LoadHint.COLD);
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        assertNull(mIncognitoTabSwitcherPane.getTabSwitcherPaneCoordinator());

        mIncognitoTabSwitcherPane.notifyLoadHint(LoadHint.HOT);
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        TabSwitcherPaneCoordinator coordinator =
                mIncognitoTabSwitcherPane.getTabSwitcherPaneCoordinator();
        assertNotNull(coordinator);
        verify(coordinator, never()).resetWithListOfTabs(mTabList);
        verify(coordinator).setInitialScrollIndexOffset();
        verify(coordinator).requestAccessibilityFocusOnCurrentTab();

        when(mTabGroupModelFilter.isTabModelRestored()).thenReturn(true);
        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.GridTabSwitcher.TimeToTabStateInitializedFromShown");
        mIncognitoTabSwitcherPane.showAllTabs();
        verify(coordinator).resetWithListOfTabs(mTabList);
        watcher.assertExpected();
    }

    @Test
    public void testResetWithTabListReauthRequired() {
        mIncognitoReauthControllerSupplier.set(mIncognitoReauthController);
        RobolectricUtil.runAllBackgroundAndUi();
        mIncognitoTabSwitcherPane.createTabSwitcherPaneCoordinator();
        TabSwitcherPaneCoordinator coordinator =
                mIncognitoTabSwitcherPane.getTabSwitcherPaneCoordinator();

        when(mIncognitoTabModel.isActiveModel()).thenReturn(true);
        mIncognitoTabSwitcherPane.notifyLoadHint(LoadHint.HOT);
        verify(coordinator).resetWithListOfTabs(mTabList);

        when(mIncognitoReauthController.isIncognitoReauthPending()).thenReturn(true);
        mIncognitoTabSwitcherPane.showAllTabs();
        verify(coordinator).resetWithListOfTabs(null);
    }

    @Test
    public void testRequestAccessibilityFocusOnCurrentTab() {
        mIncognitoReauthControllerSupplier.set(mIncognitoReauthController);
        RobolectricUtil.runAllBackgroundAndUi();
        mIncognitoTabSwitcherPane.createTabSwitcherPaneCoordinator();
        TabSwitcherPaneCoordinator coordinator =
                mIncognitoTabSwitcherPane.getTabSwitcherPaneCoordinator();

        when(mIncognitoReauthController.isReauthPageShowing()).thenReturn(true);
        mIncognitoTabSwitcherPane.requestAccessibilityFocusOnCurrentTab();
        verify(coordinator, never()).requestAccessibilityFocusOnCurrentTab();

        when(mIncognitoReauthController.isReauthPageShowing()).thenReturn(false);
        mIncognitoTabSwitcherPane.requestAccessibilityFocusOnCurrentTab();
        verify(coordinator).requestAccessibilityFocusOnCurrentTab();
    }

    @Test
    public void testForceCleanup() {
        mIncognitoTabSwitcherPane.createTabSwitcherPaneCoordinator();
        assertNotNull(mIncognitoTabSwitcherPane.getTabSwitcherPaneCoordinator());
        mIncognitoTabSwitcherPane.setPaneHubController(mPaneHubController);
        when(mTabSwitcherPaneCoordinator.getIsRecyclerViewAnimatorRunning()).thenReturn(null);

        mIncognitoTabSwitcherPane.initWithNative();
        verify(mIncognitoTabModel).addIncognitoObserver(mIncognitoTabModelObserverCaptor.capture());
        IncognitoTabModelObserver observer = mIncognitoTabModelObserverCaptor.getValue();

        observer.didBecomeEmpty();

        verify(mPaneHubController).focusPane(PaneId.TAB_SWITCHER);
        assertNull(mIncognitoTabSwitcherPane.getTabSwitcherPaneCoordinator());
    }

    @Test
    public void testForceCleanup_ReauthVisible() {
        when(mIncognitoReauthController.isReauthPageShowing()).thenReturn(true);
        mIncognitoReauthControllerSupplier.set(mIncognitoReauthController);
        RobolectricUtil.runAllBackgroundAndUi();
        mIncognitoTabSwitcherPane.createTabSwitcherPaneCoordinator();
        assertNotNull(mIncognitoTabSwitcherPane.getTabSwitcherPaneCoordinator());
        mIncognitoTabSwitcherPane.setPaneHubController(mPaneHubController);

        mIncognitoTabSwitcherPane.initWithNative();
        verify(mIncognitoTabModel).addIncognitoObserver(mIncognitoTabModelObserverCaptor.capture());
        IncognitoTabModelObserver observer = mIncognitoTabModelObserverCaptor.getValue();

        observer.didBecomeEmpty();

        verify(mPaneHubController).focusPane(PaneId.TAB_SWITCHER);
        assertNull(mIncognitoTabSwitcherPane.getTabSwitcherPaneCoordinator());
    }

    @Test
    public void testFinalIncognitoTabWasSwiped() {
        mIncognitoTabSwitcherPane.createTabSwitcherPaneCoordinator();
        assertNotNull(mIncognitoTabSwitcherPane.getTabSwitcherPaneCoordinator());
        mIncognitoTabSwitcherPane.setPaneHubController(mPaneHubController);

        mIncognitoTabSwitcherPane.initWithNative();
        verify(mIncognitoTabModel).addIncognitoObserver(mIncognitoTabModelObserverCaptor.capture());
        verify(mIncognitoTabModel).addObserver(mTabModelObserverCaptor.capture());
        IncognitoTabModelObserver incognitoObserver = mIncognitoTabModelObserverCaptor.getValue();
        TabModelObserver observer = mTabModelObserverCaptor.getValue();

        MockTab mockTab = new MockTab(0, mProfile);
        mRecentlySwipedTabIdSupplier.set(0);

        observer.onFinishingTabClosure(mockTab, TabClosingSource.UNKNOWN);
        incognitoObserver.didBecomeEmpty();
        RobolectricUtil.runAllBackgroundAndUi();

        verify(mPaneHubController).focusPane(PaneId.TAB_SWITCHER);
        assertNull(mIncognitoTabSwitcherPane.getTabSwitcherPaneCoordinator());
    }

    @Test
    public void testTabGridDialogVisible() {
        mIncognitoTabSwitcherPane.createTabSwitcherPaneCoordinator();
        assertNotNull(mIncognitoTabSwitcherPane.getTabSwitcherPaneCoordinator());
        mIncognitoTabSwitcherPane.setPaneHubController(mPaneHubController);

        mIncognitoTabSwitcherPane.initWithNative();
        verify(mIncognitoTabModel).addIncognitoObserver(mIncognitoTabModelObserverCaptor.capture());
        IncognitoTabModelObserver incognitoObserver = mIncognitoTabModelObserverCaptor.getValue();

        mTabGridDialogShowingOrAnimationSupplier.set(true);
        incognitoObserver.didBecomeEmpty();
        RobolectricUtil.runAllBackgroundAndUi();

        verify(mPaneHubController).focusPane(PaneId.TAB_SWITCHER);
        assertNull(mIncognitoTabSwitcherPane.getTabSwitcherPaneCoordinator());
    }

    @Test
    public void testTabGridDialogNotVisible() {
        mIncognitoTabSwitcherPane.createTabSwitcherPaneCoordinator();
        assertNotNull(mIncognitoTabSwitcherPane.getTabSwitcherPaneCoordinator());
        mIncognitoTabSwitcherPane.setPaneHubController(mPaneHubController);

        mIncognitoTabSwitcherPane.initWithNative();
        verify(mIncognitoTabModel).addIncognitoObserver(mIncognitoTabModelObserverCaptor.capture());
        IncognitoTabModelObserver incognitoObserver = mIncognitoTabModelObserverCaptor.getValue();

        mTabGridDialogShowingOrAnimationSupplier.set(false);
        incognitoObserver.didBecomeEmpty();
        RobolectricUtil.runAllBackgroundAndUi();

        verify(mPaneHubController, never()).focusPane(PaneId.TAB_SWITCHER);
        assertNotNull(mIncognitoTabSwitcherPane.getTabSwitcherPaneCoordinator());
    }

    /**
     * Verifies that the action button is in one of three states: Enabled (enabled = true) Disabled
     * (enabled = false) Hidden (enabled = null)
     */
    private void checkNewTabButton(@Nullable Boolean enabled) {
        FullButtonData buttonData = mIncognitoTabSwitcherPane.getActionButtonDataSupplier().get();
        if (enabled == null) {
            assertNull(buttonData);
            return;
        } else {
            assertNotNull(buttonData);
        }

        assertEquals(mContext.getString(R.string.button_new_tab), buttonData.resolveText(mContext));
        assertEquals(
                mContext.getString(R.string.button_new_incognito_tab),
                buttonData.resolveContentDescription(mContext));
        assertEquals(
                R.drawable.new_tab_icon,
                shadowOf(buttonData.resolveIcon(mContext)).getCreatedFromResId());
        if (!enabled) {
            assertNull(buttonData.getOnPressRunnable());
        } else {
            assertNotNull(buttonData.getOnPressRunnable());
            reset(mNewTabButtonClickListener);
            buttonData.getOnPressRunnable().run();
            verify(mNewTabButtonClickListener).onClick(isNull());
        }
    }

    private void checkIncognitoTabModelObserverAndButtonData() {
        mIncognitoTabSwitcherPane.createTabSwitcherPaneCoordinator();
        assertNotNull(mIncognitoTabSwitcherPane.getTabSwitcherPaneCoordinator());
        mIncognitoTabSwitcherPane.setPaneHubController(mPaneHubController);

        IncognitoTabModelObserver observer = mIncognitoTabModelObserverCaptor.getValue();

        observer.didBecomeEmpty();
        mIsRecyclerViewAnimatorRunningSupplier.set(true);
        mIsRecyclerViewAnimatorRunningSupplier.set(false);
        RobolectricUtil.runAllBackgroundAndUi();

        assertNull(mIncognitoTabSwitcherPane.getReferenceButtonDataSupplier().get());
        verify(mPaneHubController).focusPane(PaneId.TAB_SWITCHER);
        assertNull(mIncognitoTabSwitcherPane.getTabSwitcherPaneCoordinator());

        // TODO(crbug.com/40946413): These resources need to be updated.
        observer.wasFirstTabCreated();
        DisplayButtonData buttonData =
                mIncognitoTabSwitcherPane.getReferenceButtonDataSupplier().get();
        assertEquals(
                mContext.getString(R.string.accessibility_tab_switcher_incognito_stack),
                buttonData.resolveText(mContext));
        assertEquals(
                mContext.getString(R.string.accessibility_tab_switcher_incognito_stack),
                buttonData.resolveContentDescription(mContext));
        assertNotNull(buttonData.resolveIcon(mContext));

        mIncognitoTabSwitcherPane.createTabSwitcherPaneCoordinator();
        observer.didBecomeEmpty();
        mIsRecyclerViewAnimatorRunningSupplier.set(true);
        mIsRecyclerViewAnimatorRunningSupplier.set(false);
        RobolectricUtil.runAllBackgroundAndUi();

        assertNull(mIncognitoTabSwitcherPane.getReferenceButtonDataSupplier().get());
        verify(mPaneHubController, times(2)).focusPane(PaneId.TAB_SWITCHER);
        assertNull(mIncognitoTabSwitcherPane.getTabSwitcherPaneCoordinator());
    }
}
