// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.test.ext.junit.rules.ActivityScenarioRule;
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

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.multiwindow.MultiWindowModeStateDispatcher;
import org.chromium.chrome.browser.price_tracking.PriceTrackingFeatures;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab_ui.OnTabSelectingListener;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tasks.tab_management.PriceMessageService.PriceWelcomeMessageReviewActionProvider;
import org.chromium.chrome.browser.tasks.tab_management.TabListCoordinator.TabListMode;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorCoordinator.TabListEditorController;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcherMessageManager.MessageUpdateObserver;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modaldialog.ModalDialogManager;

/** Unit tests for the TabSwitcherMessageManager. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabSwitcherMessageManagerUnitTest {
    private static final int TAB1_ID = 456;
    private static final int TAB2_ID = 789;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private Tracker mTracker;
    @Mock private Profile mProfile;
    @Mock private TabModel mTabModel;
    @Mock private TabModelFilter mTabModelFilter;
    @Mock private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    @Mock private MultiWindowModeStateDispatcher mMultiWindowModeStateDispatcher;
    @Mock private SnackbarManager mSnackbarManager;
    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private TabListCoordinator mTabListCoordinator;
    @Mock private TabListEditorController mTabListEditorController;
    @Mock private PriceWelcomeMessageReviewActionProvider mPriceWelcomeMessageReviewActionProvider;
    @Mock private PriceMessageService mPriceMessageService;
    @Mock private MessageUpdateObserver mMessageUpdateObserver;
    @Mock private BrowserControlsStateProvider mBrowserControlsStateProvider;
    @Mock private TabContentManager mTabContentManager;
    @Mock private ViewGroup mRootView;
    @Mock private TabCreator mRegularTabCreator;
    @Mock private BackPressManager mBackPressManager;
    @Mock private OnTabSelectingListener mOnTabSelectingListener;

    @Captor private ArgumentCaptor<TabModelObserver> mTabModelObserverCaptor;

    @Captor
    private ArgumentCaptor<MultiWindowModeStateDispatcher.MultiWindowModeObserver>
            mMultiWindowModeObserverCaptor;

    private final ObservableSupplierImpl<TabModelFilter> mCurrentTabModelFilterSupplier =
            new ObservableSupplierImpl<>();

    private TabSwitcherMessageManager mMessageManager;
    private MockTab mTab1;
    private MockTab mTab2;

    @Before
    public void setUp() {
        PriceTrackingFeatures.setPriceTrackingEnabledForTesting(true);
        PriceTrackingFeatures.setIsSignedInAndSyncEnabledForTesting(true);

        TrackerFactory.setTrackerForTests(mTracker);

        mTab1 = MockTab.createAndInitialize(TAB1_ID, mProfile);
        mTab2 = MockTab.createAndInitialize(TAB2_ID, mProfile);

        doReturn(true)
                .when(mMultiWindowModeStateDispatcher)
                .addObserver(mMultiWindowModeObserverCaptor.capture());
        doNothing().when(mTabModelFilter).addObserver(mTabModelObserverCaptor.capture());
        doReturn(mTabModel).when(mTabModelFilter).getTabModel();
        doReturn(mProfile).when(mTabModel).getProfile();
        mCurrentTabModelFilterSupplier.set(mTabModelFilter);

        mActivityScenarioRule.getScenario().onActivity(this::onActivityReady);
    }

    private void onActivityReady(Activity activity) {
        FrameLayout container = new FrameLayout(activity);
        activity.setContentView(container);

        mMessageManager =
                new TabSwitcherMessageManager(
                        activity,
                        mActivityLifecycleDispatcher,
                        mCurrentTabModelFilterSupplier,
                        mMultiWindowModeStateDispatcher,
                        mSnackbarManager,
                        mModalDialogManager,
                        mBrowserControlsStateProvider,
                        mTabContentManager,
                        TabListMode.GRID,
                        mRootView,
                        mRegularTabCreator,
                        mBackPressManager,
                        /* desktopWindowStateProvider= */ null);
        mMessageManager.registerMessages(mTabListCoordinator);
        mMessageManager.bind(
                mTabListCoordinator,
                container,
                mPriceWelcomeMessageReviewActionProvider,
                mOnTabSelectingListener);
        mMessageManager.addObserver(mMessageUpdateObserver);

        mMessageManager.setPriceMessageServiceForTesting(mPriceMessageService);
        mMessageManager.initWithNative(mProfile, TabListMode.GRID);

        assertTrue(mCurrentTabModelFilterSupplier.hasObservers());
    }

    @After
    public void tearDown() {
        mMessageManager.removeObserver(mMessageUpdateObserver);
        mMessageManager.destroy();
        assertFalse(mCurrentTabModelFilterSupplier.hasObservers());
    }

    @Test
    @SmallTest
    public void testBeforeReset() {
        mMessageManager.beforeReset();
        verify(mPriceMessageService).invalidateMessage();
        verify(mTabModelFilter).removeObserver(any());
    }

    @Test
    @SmallTest
    public void testAfterReset() {
        verify(mTabModelFilter).addObserver(any());

        mMessageManager.afterReset(0);
        verify(mMessageUpdateObserver).onRemoveAllAppendedMessage();
        verify(mMessageUpdateObserver, never()).onAppendedMessage();
        verify(mTabModelFilter, times(2)).addObserver(any());

        mMessageManager.afterReset(1);
        verify(mMessageUpdateObserver, times(2)).onRemoveAllAppendedMessage();
        verify(mMessageUpdateObserver).onAppendedMessage();
        verify(mTabModelFilter, times(3)).addObserver(any());
    }

    @Test
    @SmallTest
    public void removeMessageItemsWhenCloseLastTab() {
        // Mock that mTab1 is not the only tab in the current tab model and it will be closed.
        doReturn(2).when(mTabModel).getCount();
        mTabModelObserverCaptor.getValue().willCloseTab(mTab1, true);
        verify(mTabListCoordinator, never()).removeSpecialListItem(anyInt(), anyInt());

        // Mock that mTab1 is the only tab in the current tab model and it will be closed.
        doReturn(1).when(mTabModel).getCount();
        mTabModelObserverCaptor.getValue().willCloseTab(mTab1, true);

        verify(mTabListCoordinator)
                .removeSpecialListItem(
                        TabProperties.UiType.MESSAGE, MessageService.MessageType.ALL);
        verify(mTabListCoordinator)
                .removeSpecialListItem(
                        TabProperties.UiType.LARGE_MESSAGE,
                        MessageService.MessageType.INCOGNITO_REAUTH_PROMO_MESSAGE);
        verify(mMessageUpdateObserver).onRemoveAllAppendedMessage();
    }

    @Test
    @SmallTest
    public void restoreMessageItemsWhenUndoLastTabClosure() {
        // Mock that mTab1 was not the only tab in the current tab model and its closure will be
        // undone.
        doReturn(2).when(mTabModel).getCount();
        mTabModelObserverCaptor.getValue().tabClosureUndone(mTab1);
        verify(mMessageUpdateObserver, never()).onRestoreAllAppendedMessage();

        // Mock that mTab1 was the only tab in the current tab model and its closure will be undone.
        doReturn(1).when(mTabModel).getCount();
        mTabModelObserverCaptor.getValue().tabClosureUndone(mTab1);
        verify(mMessageUpdateObserver).onRestoreAllAppendedMessage();
    }

    @Test
    @SmallTest
    public void enterMultiWindowMode() {
        mMultiWindowModeObserverCaptor.getValue().onMultiWindowModeChanged(true);

        verify(mTabListCoordinator)
                .removeSpecialListItem(
                        TabProperties.UiType.MESSAGE, MessageService.MessageType.ALL);
        verify(mTabListCoordinator)
                .removeSpecialListItem(
                        TabProperties.UiType.LARGE_MESSAGE,
                        MessageService.MessageType.INCOGNITO_REAUTH_PROMO_MESSAGE);
        verify(mMessageUpdateObserver).onRemoveAllAppendedMessage();
    }

    @Test
    @SmallTest
    public void exitMultiWindowMode() {
        mMultiWindowModeObserverCaptor.getValue().onMultiWindowModeChanged(false);

        verify(mMessageUpdateObserver).onRestoreAllAppendedMessage();
    }

    @Test
    @SmallTest
    public void removePriceWelcomeMessageWhenCloseBindingTab() {
        doReturn(1).when(mTabModel).getCount();
        doReturn(TAB1_ID).when(mPriceMessageService).getBindingTabId();
        mTabModelObserverCaptor.getValue().willCloseTab(mTab1, true);
        verify(mMessageUpdateObserver, never()).onRemovePriceWelcomeMessage();

        doReturn(2).when(mTabModel).getCount();
        doReturn(TAB2_ID).when(mPriceMessageService).getBindingTabId();
        mTabModelObserverCaptor.getValue().willCloseTab(mTab1, true);
        verify(mMessageUpdateObserver, never()).onRemovePriceWelcomeMessage();

        doReturn(2).when(mTabModel).getCount();
        doReturn(TAB1_ID).when(mPriceMessageService).getBindingTabId();
        mTabModelObserverCaptor.getValue().willCloseTab(mTab1, true);
        verify(mMessageUpdateObserver).onRemovePriceWelcomeMessage();
    }

    @Test
    @SmallTest
    public void restorePriceWelcomeMessageWhenUndoBindingTabClosure() {
        doReturn(1).when(mTabModel).getCount();
        doReturn(TAB1_ID).when(mPriceMessageService).getBindingTabId();
        mTabModelObserverCaptor.getValue().tabClosureUndone(mTab1);
        verify(mMessageUpdateObserver).onRestorePriceWelcomeMessage();

        doReturn(2).when(mTabModel).getCount();
        doReturn(TAB2_ID).when(mPriceMessageService).getBindingTabId();
        mTabModelObserverCaptor.getValue().tabClosureUndone(mTab1);
        // Not called a second time.
        verify(mMessageUpdateObserver).onRestorePriceWelcomeMessage();
    }

    @Test
    @SmallTest
    public void invalidatePriceWelcomeMessageWhenBindingTabClosureCommitted() {
        doReturn(TAB2_ID).when(mPriceMessageService).getBindingTabId();
        mTabModelObserverCaptor.getValue().tabClosureCommitted(mTab1);
        verify(mPriceMessageService, never()).invalidateMessage();

        doReturn(TAB1_ID).when(mPriceMessageService).getBindingTabId();
        mTabModelObserverCaptor.getValue().tabClosureCommitted(mTab1);
        verify(mPriceMessageService).invalidateMessage();
    }

    @Test
    @SmallTest
    public void dismissHandlerSkipWhenUnbound() {
        @MessageService.MessageType
        int messageType = MessageService.MessageType.INCOGNITO_REAUTH_PROMO_MESSAGE;
        mMessageManager.dismissHandler(messageType);
        verify(mTabListCoordinator)
                .removeSpecialListItem(TabProperties.UiType.LARGE_MESSAGE, messageType);
        verify(mMessageUpdateObserver).onRemovedMessage();

        mMessageManager.unbind(mTabListCoordinator);
        verify(mTabListCoordinator, times(2))
                .removeSpecialListItem(TabProperties.UiType.LARGE_MESSAGE, messageType);
        verify(mMessageUpdateObserver).onRemovedMessage();
        verify(mMessageUpdateObserver).onRemoveAllAppendedMessage();

        mMessageManager.dismissHandler(messageType);
        // Not called again and doesn't crash.
        verify(mTabListCoordinator, times(2))
                .removeSpecialListItem(TabProperties.UiType.LARGE_MESSAGE, messageType);
        verify(mMessageUpdateObserver).onRemovedMessage();
    }
}
