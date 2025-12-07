// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

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
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.app.tabmodel.ArchivedTabModelOrchestrator;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.hub.PaneManager;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.multiwindow.MultiWindowModeStateDispatcher;
import org.chromium.chrome.browser.price_tracking.PriceTrackingFeatures;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tab_ui.OnTabSelectingListener;
import org.chromium.chrome.browser.tab_ui.SuggestionLifecycleObserverHandler;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tasks.tab_management.PriceMessageService.PriceWelcomeMessageReviewActionProvider;
import org.chromium.chrome.browser.tasks.tab_management.TabListCoordinator.TabListMode;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.UiType;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcherMessageManager.MessageType;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcherMessageManager.MessageUpdateObserver;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_group_sync.TabGroupUiActionHandler;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.List;
import java.util.function.Supplier;

/** Unit tests for the TabSwitcherMessageManager. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.TAB_SWITCHER_GROUP_SUGGESTIONS_ANDROID)
public class TabSwitcherMessageManagerUnitTest {
    private static final int INITIAL_TAB_COUNT = 0;
    private static final int TAB1_ID = 456;
    private static final int TAB2_ID = 789;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private Tracker mTracker;
    @Mock private Profile mProfile;
    @Mock private TabModel mTabModel;
    @Mock private TabGroupModelFilter mTabGroupModelFilter;
    @Mock private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    @Mock private MultiWindowModeStateDispatcher mMultiWindowModeStateDispatcher;
    @Mock private SnackbarManager mSnackbarManager;
    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private TabListCoordinator mTabListCoordinator;
    @Mock private TabListHighlighter mTabListHighlighter;
    @Mock private PriceWelcomeMessageReviewActionProvider mPriceWelcomeMessageReviewActionProvider;
    @Mock private MessageUpdateObserver mMessageUpdateObserver;
    @Mock private BrowserControlsStateProvider mBrowserControlsStateProvider;
    @Mock private TabContentManager mTabContentManager;
    @Mock private ViewGroup mRootView;
    @Mock private TabCreator mRegularTabCreator;
    @Mock private BackPressManager mBackPressManager;
    @Mock private OnTabSelectingListener mOnTabSelectingListener;
    @Mock private TabGroupSyncService mTabGroupSyncService;
    @Mock private Supplier<PaneManager> mPaneManagerSupplier;
    @Mock private Supplier<TabGroupUiActionHandler> mTabGroupUiActionHandlerSupplier;
    @Mock private ArchivedTabModelOrchestrator mArchivedTabModelOrchestrator;
    @Mock private Supplier<LayoutStateProvider> mLayoutStateProviderSupplier;
    @Captor private ArgumentCaptor<TabModelObserver> mTabModelObserverCaptor;

    @Captor
    private ArgumentCaptor<MultiWindowModeStateDispatcher.MultiWindowModeObserver>
            mMultiWindowModeObserverCaptor;

    private final ObservableSupplierImpl<TabGroupModelFilter> mCurrentTabGroupModelFilterSupplier =
            new ObservableSupplierImpl<>();
    private final ObservableSupplierImpl<EdgeToEdgeController> mEdgeToEdgeSupplier =
            new ObservableSupplierImpl<>();
    private final ObservableSupplierImpl<Integer> mTabCountSupplier =
            new ObservableSupplierImpl<>(INITIAL_TAB_COUNT);
    private TabSwitcherMessageManager mMessageManager;
    private MockTab mTab1;
    private MockTab mTab2;

    @Before
    public void setUp() {
        PriceTrackingFeatures.setPriceAnnotationsEnabledForTesting(true);
        PriceTrackingFeatures.setIsSignedInAndSyncEnabledForTesting(true);

        TrackerFactory.setTrackerForTests(mTracker);
        TabGroupSyncServiceFactory.setForTesting(mTabGroupSyncService);
        ArchivedTabModelOrchestrator.setInstanceForTesting(mArchivedTabModelOrchestrator);

        mTab1 = MockTab.createAndInitialize(TAB1_ID, mProfile);
        mTab2 = MockTab.createAndInitialize(TAB2_ID, mProfile);

        doReturn(true)
                .when(mMultiWindowModeStateDispatcher)
                .addObserver(mMultiWindowModeObserverCaptor.capture());
        doReturn(mTabListHighlighter).when(mTabListCoordinator).getTabListHighlighter();
        doNothing().when(mTabGroupModelFilter).addObserver(any());
        doReturn(mTabModel).when(mTabGroupModelFilter).getTabModel();
        doReturn(mProfile).when(mTabModel).getProfile();
        doReturn(mProfile).when(mProfile).getOriginalProfile();
        mCurrentTabGroupModelFilterSupplier.set(mTabGroupModelFilter);

        when(mArchivedTabModelOrchestrator.getTabCountSupplier()).thenReturn(mTabCountSupplier);

        mActivityScenarioRule.getScenario().onActivity(this::onActivityReady);
    }

    private void onActivityReady(Activity activity) {
        FrameLayout container = new FrameLayout(activity);
        activity.setContentView(container);

        mMessageManager =
                new TabSwitcherMessageManager(
                        activity,
                        mActivityLifecycleDispatcher,
                        mCurrentTabGroupModelFilterSupplier,
                        mMultiWindowModeStateDispatcher,
                        mSnackbarManager,
                        mModalDialogManager,
                        mBrowserControlsStateProvider,
                        mTabContentManager,
                        TabListMode.GRID,
                        mRootView,
                        mRegularTabCreator,
                        mBackPressManager,
                        /* desktopWindowStateManager= */ null,
                        mEdgeToEdgeSupplier,
                        mPaneManagerSupplier,
                        mTabGroupUiActionHandlerSupplier,
                        mLayoutStateProviderSupplier);
        mMessageManager.registerMessageHostDelegate(
                MessageHostDelegateFactory.build(mTabListCoordinator));
        mMessageManager.bind(
                mTabListCoordinator,
                container,
                mPriceWelcomeMessageReviewActionProvider,
                mOnTabSelectingListener);
        mMessageManager.addObserver(mMessageUpdateObserver);
        mMessageManager.initWithNative(mProfile, TabListMode.GRID);
        verify(mTabGroupModelFilter, times(2)).addObserver(mTabModelObserverCaptor.capture());

        assertTrue(mCurrentTabGroupModelFilterSupplier.hasObservers());
    }

    @After
    public void tearDown() {
        mMessageManager.removeObserver(mMessageUpdateObserver);
        mMessageManager.destroy();
        assertFalse(mCurrentTabGroupModelFilterSupplier.hasObservers());
    }

    @Test
    public void testBeforeReset() {
        mMessageManager.beforeReset();
        verify(mTabGroupModelFilter).removeObserver(any());
    }

    @Test
    public void testAfterReset() {
        verify(mTabGroupModelFilter, times(2)).addObserver(any());

        mMessageManager.afterReset(0);
        verify(mMessageUpdateObserver).onRemoveAllAppendedMessage();
        verify(mMessageUpdateObserver, never()).onAppendedMessage();
        verify(mTabGroupModelFilter, times(3)).addObserver(any());

        mMessageManager.afterReset(1);
        verify(mMessageUpdateObserver, times(2)).onRemoveAllAppendedMessage();
        verify(mMessageUpdateObserver).onAppendedMessage();
        verify(mTabGroupModelFilter, times(4)).addObserver(any());
    }

    @Test
    public void removeMessageItemsWhenCloseLastTab() {
        // Mock that mTab1 is not the only tab in the current tab model and it will be closed.
        doReturn(2).when(mTabModel).getCount();
        getTabModelObserver(0).willCloseTab(mTab1, true);
        verify(mTabListCoordinator, never()).removeSpecialListItem(anyInt(), anyInt());

        // Mock that mTab1 is the only tab in the current tab model and it will be closed.
        doReturn(1).when(mTabModel).getCount();
        getTabModelObserver(0).willCloseTab(mTab1, true);

        verify(mTabListCoordinator).removeSpecialListItem(UiType.IPH_MESSAGE, MessageType.IPH);
        verify(mTabListCoordinator)
                .removeSpecialListItem(UiType.PRICE_MESSAGE, MessageType.PRICE_MESSAGE);
        verify(mTabListCoordinator)
                .removeSpecialListItem(
                        UiType.INCOGNITO_REAUTH_PROMO_MESSAGE,
                        MessageType.INCOGNITO_REAUTH_PROMO_MESSAGE);
        verify(mTabListCoordinator)
                .removeSpecialListItem(
                        UiType.ARCHIVED_TABS_MESSAGE, MessageType.ARCHIVED_TABS_MESSAGE);
        verify(mMessageUpdateObserver).onRemoveAllAppendedMessage();
    }

    @Test
    public void removeMessageItemsWhenCloseMultipleTabs() {
        // Simulate only some tabs being closed.
        doReturn(3).when(mTabModel).getCount();
        getTabModelObserver(0).willCloseMultipleTabs(false, List.of(mTab1, mTab2));
        verify(mTabListCoordinator, never()).removeSpecialListItem(anyInt(), anyInt());

        // Simulate all tabs being closed.
        doReturn(2).when(mTabModel).getCount();
        getTabModelObserver(0).willCloseMultipleTabs(false, List.of(mTab1, mTab2));

        verify(mTabListCoordinator).removeSpecialListItem(UiType.IPH_MESSAGE, MessageType.IPH);
        verify(mTabListCoordinator)
                .removeSpecialListItem(UiType.PRICE_MESSAGE, MessageType.PRICE_MESSAGE);
        verify(mTabListCoordinator)
                .removeSpecialListItem(
                        UiType.INCOGNITO_REAUTH_PROMO_MESSAGE,
                        MessageType.INCOGNITO_REAUTH_PROMO_MESSAGE);
        verify(mTabListCoordinator)
                .removeSpecialListItem(
                        UiType.ARCHIVED_TABS_MESSAGE, MessageType.ARCHIVED_TABS_MESSAGE);
        verify(mMessageUpdateObserver).onRemoveAllAppendedMessage();
    }

    @Test
    public void removeMessageItemsWhenCloseLastTab_withGroupSuggestion() {
        createGroupSuggestion();

        // Mock that mTab1 is not the only tab in the current tab model and it will be closed.
        doReturn(2).when(mTabModel).getCount();
        getTabModelObserver(0).willCloseTab(mTab1, true);
        verify(mTabListCoordinator, never()).removeSpecialListItem(anyInt(), anyInt());

        // Mock that mTab1 is the only tab in the current tab model and it will be closed.
        doReturn(1).when(mTabModel).getCount();
        getTabModelObserver(0).willCloseTab(mTab1, true);

        verify(mTabListCoordinator).removeSpecialListItem(UiType.IPH_MESSAGE, MessageType.IPH);
        verify(mTabListCoordinator)
                .removeSpecialListItem(UiType.PRICE_MESSAGE, MessageType.PRICE_MESSAGE);
        verify(mTabListCoordinator)
                .removeSpecialListItem(
                        UiType.INCOGNITO_REAUTH_PROMO_MESSAGE,
                        MessageType.INCOGNITO_REAUTH_PROMO_MESSAGE);
        verify(mTabListCoordinator)
                .removeSpecialListItem(
                        UiType.ARCHIVED_TABS_MESSAGE, MessageType.ARCHIVED_TABS_MESSAGE);
        verify(mTabListCoordinator)
                .removeSpecialListItem(
                        UiType.TAB_GROUP_SUGGESTION_MESSAGE,
                        MessageType.TAB_GROUP_SUGGESTION_MESSAGE);
        verify(mTabListHighlighter).unhighlightTabs();
        verify(mMessageUpdateObserver).onRemoveAllAppendedMessage();
    }

    @Test
    public void restoreMessageItemsWhenUndoLastTabClosure() {
        // Mock that mTab1 was not the only tab in the current tab model and its closure will be
        // undone.
        doReturn(2).when(mTabModel).getCount();
        getTabModelObserver(0).tabClosureUndone(mTab1);
        verify(mMessageUpdateObserver, never()).onRestoreAllAppendedMessage();

        // Mock that mTab1 was the only tab in the current tab model and its closure will be undone.
        doReturn(1).when(mTabModel).getCount();
        getTabModelObserver(0).tabClosureUndone(mTab1);
        verify(mMessageUpdateObserver).onRestoreAllAppendedMessage();
    }

    @Test
    public void enterMultiWindowMode() {
        mMultiWindowModeObserverCaptor.getValue().onMultiWindowModeChanged(true);

        verify(mTabListCoordinator).removeSpecialListItem(UiType.IPH_MESSAGE, MessageType.IPH);
        verify(mTabListCoordinator)
                .removeSpecialListItem(UiType.PRICE_MESSAGE, MessageType.PRICE_MESSAGE);
        verify(mTabListCoordinator)
                .removeSpecialListItem(
                        UiType.INCOGNITO_REAUTH_PROMO_MESSAGE,
                        MessageType.INCOGNITO_REAUTH_PROMO_MESSAGE);
        verify(mTabListCoordinator)
                .removeSpecialListItem(
                        UiType.ARCHIVED_TABS_MESSAGE, MessageType.ARCHIVED_TABS_MESSAGE);
        verify(mMessageUpdateObserver).onRemoveAllAppendedMessage();
    }

    @Test
    public void enterMultiWindowMode_withGroupSuggestion() {
        createGroupSuggestion();

        mMultiWindowModeObserverCaptor.getValue().onMultiWindowModeChanged(true);

        verify(mTabListCoordinator).removeSpecialListItem(UiType.IPH_MESSAGE, MessageType.IPH);
        verify(mTabListCoordinator)
                .removeSpecialListItem(UiType.PRICE_MESSAGE, MessageType.PRICE_MESSAGE);
        verify(mTabListCoordinator)
                .removeSpecialListItem(
                        UiType.INCOGNITO_REAUTH_PROMO_MESSAGE,
                        MessageType.INCOGNITO_REAUTH_PROMO_MESSAGE);
        verify(mTabListCoordinator)
                .removeSpecialListItem(
                        UiType.ARCHIVED_TABS_MESSAGE, MessageType.ARCHIVED_TABS_MESSAGE);
        verify(mTabListCoordinator)
                .removeSpecialListItem(
                        UiType.TAB_GROUP_SUGGESTION_MESSAGE,
                        MessageType.TAB_GROUP_SUGGESTION_MESSAGE);
        verify(mTabListHighlighter).unhighlightTabs();
        verify(mMessageUpdateObserver).onRemoveAllAppendedMessage();
    }

    @Test
    public void exitMultiWindowMode() {
        mMultiWindowModeObserverCaptor.getValue().onMultiWindowModeChanged(false);

        verify(mMessageUpdateObserver).onRestoreAllAppendedMessage();
    }

    @Test
    public void dismissHandlerSkipWhenUnbound() {
        @MessageType int messageType = MessageType.INCOGNITO_REAUTH_PROMO_MESSAGE;
        mMessageManager.dismissHandler(messageType);
        verify(mTabListCoordinator)
                .removeSpecialListItem(UiType.INCOGNITO_REAUTH_PROMO_MESSAGE, messageType);
        verify(mMessageUpdateObserver).onRemovedMessage();

        mMessageManager.unbind(mTabListCoordinator);
        verify(mTabListCoordinator, times(2))
                .removeSpecialListItem(UiType.INCOGNITO_REAUTH_PROMO_MESSAGE, messageType);
        verify(mMessageUpdateObserver).onRemovedMessage();
        verify(mMessageUpdateObserver).onRemoveAllAppendedMessage();

        mMessageManager.dismissHandler(messageType);
        // Not called again and doesn't crash.
        verify(mTabListCoordinator, times(2))
                .removeSpecialListItem(UiType.INCOGNITO_REAUTH_PROMO_MESSAGE, messageType);
        verify(mMessageUpdateObserver).onRemovedMessage();
    }

    private TabModelObserver getTabModelObserver(int i) {
        return mTabModelObserverCaptor.getAllValues().get(i);
    }

    private void createGroupSuggestion() {
        TabGroupSuggestionMessageService suggestionService =
                mMessageManager.getTabGroupSuggestionMessageService();
        assertNotNull(suggestionService);
        suggestionService.addGroupMessageForTabs(
                List.of(1), new SuggestionLifecycleObserverHandler());
    }
}
