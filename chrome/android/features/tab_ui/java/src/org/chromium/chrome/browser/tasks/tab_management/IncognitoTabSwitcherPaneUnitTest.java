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
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

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
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.hub.DisplayButtonData;
import org.chromium.chrome.browser.hub.FullButtonData;
import org.chromium.chrome.browser.hub.HubFieldTrial;
import org.chromium.chrome.browser.hub.LoadHint;
import org.chromium.chrome.browser.hub.PaneHubController;
import org.chromium.chrome.browser.hub.PaneId;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthController;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthManager.IncognitoReauthCallback;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tabmodel.IncognitoTabModel;
import org.chromium.chrome.browser.tabmodel.IncognitoTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.feature_engagement.Tracker;

import java.util.function.DoubleConsumer;

/**
 * Unit tests for {@link IncognitoTabSwitcherPane}. Refer to {@link TabSwitcherPaneUnitTest} for
 * tests for shared functionality with {@link TabSwitcherPaneBase}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@DisableFeatures(ChromeFeatureList.ANDROID_HUB_FLOATING_ACTION_BUTTON)
public class IncognitoTabSwitcherPaneUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private ProfileProvider mProfileProvider;
    @Mock private Tracker mTracker;
    @Mock private IncognitoReauthController mIncognitoReauthController;
    @Mock private TabSwitcherPaneCoordinatorFactory mTabSwitcherPaneCoordinatorFactory;
    @Mock private TabSwitcherPaneCoordinator mTabSwitcherPaneCoordinator;
    @Mock private View.OnClickListener mNewTabButtonClickListener;
    @Mock private TabModelFilter mTabModelFilter;
    @Mock private IncognitoTabModel mIncognitoTabModel;
    @Mock private PaneHubController mPaneHubController;
    @Mock private DoubleConsumer mOnAlphaChange;
    @Mock private UserEducationHelper mUserEducationHelper;

    @Captor private ArgumentCaptor<IncognitoTabModelObserver> mIncognitoTabModelObserverCaptor;
    @Captor private ArgumentCaptor<IncognitoReauthCallback> mIncognitoReauthCallbackCaptor;
    @Captor private ArgumentCaptor<Callback<Integer>> mOnTabClickedCallbackCaptor;

    private final OneshotSupplierImpl<ProfileProvider> mProfileProviderSupplier =
            new OneshotSupplierImpl<>();
    private final OneshotSupplierImpl<IncognitoReauthController>
            mIncognitoReauthControllerSupplier = new OneshotSupplierImpl<>();
    private final ObservableSupplierImpl<EdgeToEdgeController> mEdgeToEdgeSupplier =
            new ObservableSupplierImpl<>();

    private Context mContext;
    private IncognitoTabSwitcherPane mIncognitoTabSwitcherPane;
    private int mTimesCreated;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();

        TrackerFactory.setTrackerForTests(mTracker);
        when(mProfileProvider.getOriginalProfile()).thenReturn(mProfile);
        mProfileProviderSupplier.set(mProfileProvider);

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
                        any(),
                        anyBoolean(),
                        any(),
                        any());

        when(mTabModelFilter.getTabModel()).thenReturn(mIncognitoTabModel);
        when(mTabModelFilter.isTabModelRestored()).thenReturn(true);

        mIncognitoTabSwitcherPane =
                new IncognitoTabSwitcherPane(
                        mContext,
                        mProfileProviderSupplier,
                        mTabSwitcherPaneCoordinatorFactory,
                        () -> mTabModelFilter,
                        mNewTabButtonClickListener,
                        mIncognitoReauthControllerSupplier,
                        mOnAlphaChange,
                        mUserEducationHelper,
                        mEdgeToEdgeSupplier);
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
    @SmallTest
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
    @SmallTest
    public void testInitWithNativeHasNoIncognitoTabs() {
        when(mIncognitoTabModel.getCount()).thenReturn(0);
        mIncognitoTabSwitcherPane.initWithNative();
        verify(mIncognitoTabModel).addIncognitoObserver(mIncognitoTabModelObserverCaptor.capture());
        assertNull(mIncognitoTabSwitcherPane.getReferenceButtonDataSupplier().get());

        checkIncognitoTabModelObserverAndButtonData();
    }

    @Test
    @SmallTest
    public void testPaneId() {
        assertEquals(PaneId.INCOGNITO_TAB_SWITCHER, mIncognitoTabSwitcherPane.getPaneId());
    }

    @Test
    @SmallTest
    public void testNewTabButtonData() {
        checkNewTabButton(/* enabled= */ false);

        mIncognitoReauthControllerSupplier.set(mIncognitoReauthController);
        ShadowLooper.runUiThreadTasks();
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
    @SmallTest
    @EnableFeatures(ChromeFeatureList.ANDROID_HUB_FLOATING_ACTION_BUTTON)
    public void testNewTabButtonWithFab() {
        mIncognitoTabSwitcherPane.destroy();
        mIncognitoTabSwitcherPane =
                new IncognitoTabSwitcherPane(
                        mContext,
                        mProfileProviderSupplier,
                        mTabSwitcherPaneCoordinatorFactory,
                        () -> mTabModelFilter,
                        mNewTabButtonClickListener,
                        mIncognitoReauthControllerSupplier,
                        mOnAlphaChange,
                        mUserEducationHelper,
                        mEdgeToEdgeSupplier);

        checkNewTabButton(/* enabled= */ null);

        mIncognitoReauthControllerSupplier.set(mIncognitoReauthController);
        ShadowLooper.runUiThreadTasks();
        verify(mIncognitoReauthController)
                .addIncognitoReauthCallback(mIncognitoReauthCallbackCaptor.capture());
        when(mIncognitoReauthController.isIncognitoReauthPending()).thenReturn(false);
        when(mIncognitoReauthController.isReauthPageShowing()).thenReturn(false);
        mIncognitoTabSwitcherPane.notifyLoadHint(LoadHint.HOT);

        checkNewTabButton(/* enabled= */ true);

        when(mIncognitoReauthController.isIncognitoReauthPending()).thenReturn(true);
        when(mIncognitoReauthController.isReauthPageShowing()).thenReturn(true);
        mIncognitoTabSwitcherPane.showAllTabs();

        checkNewTabButton(/* enabled= */ null);
    }

    @Test
    @SmallTest
    public void testIncognitoReauthCallback() {
        checkNewTabButton(/* enabled= */ false);

        mIncognitoReauthControllerSupplier.set(mIncognitoReauthController);
        ShadowLooper.runUiThreadTasks();
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
        when(mTabModelFilter.isCurrentlySelectedFilter()).thenReturn(true);
        mIncognitoTabSwitcherPane.notifyLoadHint(LoadHint.HOT);
        verify(coordinator).resetWithTabList(null);
        checkNewTabButton(/* enabled= */ false);

        when(mIncognitoReauthController.isIncognitoReauthPending()).thenReturn(false);
        when(mIncognitoReauthController.isReauthPageShowing()).thenReturn(false);
        callback.onIncognitoReauthSuccess();
        verify(coordinator).resetWithTabList(mTabModelFilter);
        verify(coordinator, times(2)).setInitialScrollIndexOffset();
        verify(coordinator).requestAccessibilityFocusOnCurrentTab();
        checkNewTabButton(/* enabled= */ true);

        // Check not called again
        mIncognitoTabSwitcherPane.notifyLoadHint(LoadHint.WARM);
        callback.onIncognitoReauthSuccess();
        verifyNoMoreInteractions(coordinator);
        mIncognitoTabSwitcherPane.notifyLoadHint(LoadHint.HOT);
        verify(coordinator, times(2)).resetWithTabList(mTabModelFilter);
        verify(coordinator, times(3)).setInitialScrollIndexOffset();
        verify(coordinator, times(2)).requestAccessibilityFocusOnCurrentTab();
        checkNewTabButton(/* enabled= */ true);

        when(mTabModelFilter.isCurrentlySelectedFilter()).thenReturn(false);
        callback.onIncognitoReauthSuccess();
        verifyNoMoreInteractions(coordinator);
    }

    @Test
    @SmallTest
    public void testResetWithTabList() {
        assertFalse(mIncognitoTabSwitcherPane.resetWithTabList(null, false));

        mIncognitoTabSwitcherPane.createTabSwitcherPaneCoordinator();
        TabSwitcherPaneCoordinator coordinator =
                mIncognitoTabSwitcherPane.getTabSwitcherPaneCoordinator();

        assertTrue(mIncognitoTabSwitcherPane.resetWithTabList(null, false));
        verify(coordinator).resetWithTabList(null);

        when(mTabModelFilter.isCurrentlySelectedFilter()).thenReturn(true);
        mIncognitoTabSwitcherPane.showAllTabs();
        verify(coordinator, times(2)).resetWithTabList(null);
        when(mTabModelFilter.isCurrentlySelectedFilter()).thenReturn(false);

        mIncognitoTabSwitcherPane.notifyLoadHint(LoadHint.HOT);
        verify(coordinator, times(3)).resetWithTabList(null);

        when(mTabModelFilter.isCurrentlySelectedFilter()).thenReturn(true);
        mIncognitoTabSwitcherPane.showAllTabs();
        verify(coordinator).resetWithTabList(mTabModelFilter);
    }

    @Test
    @SmallTest
    public void testLoadHintColdWarmHotCold() {
        mIncognitoTabSwitcherPane.notifyLoadHint(LoadHint.COLD);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        assertNull(mIncognitoTabSwitcherPane.getTabSwitcherPaneCoordinator());

        mIncognitoTabSwitcherPane.notifyLoadHint(LoadHint.WARM);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        assertNull(mIncognitoTabSwitcherPane.getTabSwitcherPaneCoordinator());

        mIncognitoTabSwitcherPane.notifyLoadHint(LoadHint.HOT);
        TabSwitcherPaneCoordinator coordinator =
                mIncognitoTabSwitcherPane.getTabSwitcherPaneCoordinator();
        assertNotNull(coordinator);
        verify(coordinator, never()).softCleanup();
        verify(coordinator, never()).hardCleanup();

        mIncognitoTabSwitcherPane.notifyLoadHint(LoadHint.COLD);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        assertNull(mIncognitoTabSwitcherPane.getTabSwitcherPaneCoordinator());
        verify(coordinator).softCleanup();
        verify(coordinator).hardCleanup();
    }

    @Test
    @SmallTest
    public void testLoadHintColdHot_TabStateNotInitialized() {
        when(mTabModelFilter.isCurrentlySelectedFilter()).thenReturn(true);
        when(mTabModelFilter.isTabModelRestored()).thenReturn(false);

        mIncognitoTabSwitcherPane.notifyLoadHint(LoadHint.COLD);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        assertNull(mIncognitoTabSwitcherPane.getTabSwitcherPaneCoordinator());

        mIncognitoTabSwitcherPane.notifyLoadHint(LoadHint.HOT);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        TabSwitcherPaneCoordinator coordinator =
                mIncognitoTabSwitcherPane.getTabSwitcherPaneCoordinator();
        assertNotNull(coordinator);
        verify(coordinator, never()).resetWithTabList(mTabModelFilter);
        verify(coordinator).setInitialScrollIndexOffset();
        verify(coordinator).requestAccessibilityFocusOnCurrentTab();

        when(mTabModelFilter.isTabModelRestored()).thenReturn(true);
        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.GridTabSwitcher.TimeToTabStateInitializedFromShown");
        mIncognitoTabSwitcherPane.showAllTabs();
        verify(coordinator).resetWithTabList(mTabModelFilter);
        watcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testResetWithTabListReauthRequired() {
        mIncognitoReauthControllerSupplier.set(mIncognitoReauthController);
        ShadowLooper.runUiThreadTasks();
        mIncognitoTabSwitcherPane.createTabSwitcherPaneCoordinator();
        TabSwitcherPaneCoordinator coordinator =
                mIncognitoTabSwitcherPane.getTabSwitcherPaneCoordinator();

        when(mTabModelFilter.isCurrentlySelectedFilter()).thenReturn(true);
        mIncognitoTabSwitcherPane.notifyLoadHint(LoadHint.HOT);
        verify(coordinator).resetWithTabList(mTabModelFilter);

        when(mIncognitoReauthController.isIncognitoReauthPending()).thenReturn(true);
        mIncognitoTabSwitcherPane.showAllTabs();
        verify(coordinator).resetWithTabList(null);
    }

    @Test
    @SmallTest
    public void testRequestAccessibilityFocusOnCurrentTab() {
        mIncognitoReauthControllerSupplier.set(mIncognitoReauthController);
        ShadowLooper.runUiThreadTasks();
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
        assertTrue(
                AppCompatResources.getDrawable(mContext, R.drawable.new_tab_icon)
                        .getConstantState()
                        .equals(buttonData.resolveIcon(mContext).getConstantState()));
        if (!enabled) {
            assertNull(buttonData.getOnPressRunnable());
        } else {
            assertNotNull(buttonData.getOnPressRunnable());
            reset(mNewTabButtonClickListener);
            reset(mTracker);
            buttonData.getOnPressRunnable().run();
            verify(mNewTabButtonClickListener).onClick(isNull());
            if (HubFieldTrial.usesFloatActionButton()) {
                verify(mTracker).notifyEvent("tab_switcher_floating_action_button_clicked");
            } else {
                verify(mTracker, never()).notifyEvent(any());
            }
        }
    }

    private void checkIncognitoTabModelObserverAndButtonData() {
        mIncognitoTabSwitcherPane.createTabSwitcherPaneCoordinator();
        assertNotNull(mIncognitoTabSwitcherPane.getTabSwitcherPaneCoordinator());
        mIncognitoTabSwitcherPane.setPaneHubController(mPaneHubController);

        IncognitoTabModelObserver observer = mIncognitoTabModelObserverCaptor.getValue();

        observer.didBecomeEmpty();
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

        observer.didBecomeEmpty();
        assertNull(mIncognitoTabSwitcherPane.getReferenceButtonDataSupplier().get());
        verify(mPaneHubController, times(2)).focusPane(PaneId.TAB_SWITCHER);
        assertNull(mIncognitoTabSwitcherPane.getTabSwitcherPaneCoordinator());
    }
}
