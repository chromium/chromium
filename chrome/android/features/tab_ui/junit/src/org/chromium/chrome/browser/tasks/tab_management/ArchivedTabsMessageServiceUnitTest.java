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
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.tasks.tab_management.ArchivedTabsCardViewProperties.CLICK_HANDLER;
import static org.chromium.chrome.browser.tasks.tab_management.ArchivedTabsCardViewProperties.NUMBER_OF_ARCHIVED_TABS;

import android.app.Activity;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.app.tabmodel.ArchivedTabModelOrchestrator;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.hub.PaneManager;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabArchiveSettings;
import org.chromium.chrome.browser.tab.TabArchiver;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tab_ui.OnTabSelectingListener;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.tab_management.TabGridItemTouchHelperCallback.OnDropOnArchivalMessageCardEventListener;
import org.chromium.chrome.browser.tasks.tab_management.TabListCoordinator.TabListMode;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcherMessageManager.MessageType;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_group_sync.TabGroupUiActionHandler;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;
import java.util.function.Supplier;

/** Tests for ArchivedTabsMessageService. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@DisableFeatures(ChromeFeatureList.TAB_ARCHIVAL_DRAG_DROP_ANDROID)
public class ArchivedTabsMessageServiceUnitTest {
    private static final int TIME_DELTA_DAYS = 10;
    private static final int INITIAL_TAB_COUNT = 0;
    private static final @TabId int TAB_ID = 1;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.LENIENT);

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private ArchivedTabModelOrchestrator mArchivedTabModelOrchestrator;
    @Mock private TabArchiveSettings mTabArchiveSettings;
    @Mock private TabArchiver mTabArchiver;
    @Mock private TabModel mArchivedTabModel;
    @Mock private TabModel mTabModel;
    @Mock private Tab mTab;
    @Mock private TabGroupModelFilter mTabGroupModelFilter;
    @Mock private MessageService.MessageObserver<@MessageType Integer> mMessageObserver;
    @Mock private ArchivedTabsDialogCoordinator mArchivedTabsDialogCoordinator;
    @Mock private BrowserControlsStateProvider mBrowserControlsStateProvider;
    @Mock private TabContentManager mTabContentManager;
    @Mock private SnackbarManager mSnackbarManager;
    @Mock private TabCreator mRegularTabCreator;
    @Mock private BackPressManager mBackPressManager;
    @Mock private OnTabSelectingListener mOnTabSelectingListener;
    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private Tracker mTracker;
    @Mock private Runnable mAppendMessageRunnable;
    @Mock private TabListCoordinator mTabListCoordinator;
    @Mock private TabGroupSyncService mTabGroupSyncService;
    @Mock private Supplier<PaneManager> mPaneManagerSupplier;
    @Mock private Supplier<TabGroupUiActionHandler> mTabGroupUiActionHandlerSupplier;
    @Mock private ObservableSupplier<TabGroupModelFilter> mCurrentTabGroupModelFilterSupplier;
    @Mock private LayoutStateProvider mLayoutStateProvider;
    @Captor private ArgumentCaptor<TabArchiveSettings.Observer> mTabArchiveSettingsObserverCaptor;
    @Captor private ArgumentCaptor<OnDropOnArchivalMessageCardEventListener> mOnDropObserverCaptor;

    @Captor
    private ArgumentCaptor<LayoutStateProvider.LayoutStateObserver> mLayoutStateObserverCaptor;

    private Activity mActivity;
    private ViewGroup mRootView;
    private ArchivedTabsMessageService mArchivedTabsMessageService;
    private final ObservableSupplierImpl<Integer> mTabCountSupplier =
            new ObservableSupplierImpl<>(INITIAL_TAB_COUNT);
    private final ObservableSupplierImpl<TabListCoordinator> mTabListCoordinatorSupplier =
            new ObservableSupplierImpl<>();
    private final ObservableSupplierImpl<EdgeToEdgeController> mEdgeToEdgeSupplier =
            new ObservableSupplierImpl<>();
    private final ObservableSupplierImpl<LayoutStateProvider> mLayoutStateProviderSupplier =
            new ObservableSupplierImpl<>();

    @Before
    public void setUp() throws Exception {
        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);
        mRootView = new FrameLayout(mActivity);

        doReturn(TIME_DELTA_DAYS).when(mTabArchiveSettings).getArchiveTimeDeltaDays();
        doReturn(mTabCountSupplier).when(mArchivedTabModelOrchestrator).getTabCountSupplier();
        mTabListCoordinatorSupplier.set(mTabListCoordinator);

        when(mTabModel.getTabById(anyInt())).thenReturn(mTab);
        when(mTabGroupModelFilter.getTabModel()).thenReturn(mTabModel);
        when(mCurrentTabGroupModelFilterSupplier.get()).thenReturn(mTabGroupModelFilter);
        when(mArchivedTabModelOrchestrator.getTabArchiver()).thenReturn(mTabArchiver);
    }

    private void createArchivedTabsMessageService() {
        mLayoutStateProviderSupplier.set(mLayoutStateProvider);
        mArchivedTabsMessageService =
                new ArchivedTabsMessageService(
                        mActivity,
                        mArchivedTabModelOrchestrator,
                        mBrowserControlsStateProvider,
                        mTabContentManager,
                        TabListMode.GRID,
                        mRootView,
                        mSnackbarManager,
                        mRegularTabCreator,
                        mBackPressManager,
                        mModalDialogManager,
                        mTracker,
                        mAppendMessageRunnable,
                        mTabListCoordinatorSupplier,
                        /* desktopWindowStateManager= */ null,
                        mEdgeToEdgeSupplier,
                        mTabGroupSyncService,
                        mPaneManagerSupplier,
                        mTabGroupUiActionHandlerSupplier,
                        mCurrentTabGroupModelFilterSupplier,
                        mLayoutStateProviderSupplier);
        mArchivedTabsMessageService.setArchivedTabsDialogCoordiantorForTesting(
                mArchivedTabsDialogCoordinator);
        mArchivedTabsMessageService.addObserver(mMessageObserver);
        mArchivedTabsMessageService.setOnTabSelectingListener(mOnTabSelectingListener);

        // When the service is created, this getter will return null. Only set up the mock right
        // before onTabModelCreated is called when initialization is nearly over.
        doReturn(mTabArchiveSettings).when(mArchivedTabModelOrchestrator).getTabArchiveSettings();

        mArchivedTabsMessageService
                .getArchivedTabModelOrchestratorObserverForTesting()
                .onTabModelCreated(mArchivedTabModel);
        verify(mTabArchiveSettings).addObserver(mTabArchiveSettingsObserverCaptor.capture());
        verify(mLayoutStateProvider).addObserver(mLayoutStateObserverCaptor.capture());
    }

    @Test
    public void testTabAddedThenRemoved() {
        createArchivedTabsMessageService();
        PropertyModel customCardPropertyModel =
                mArchivedTabsMessageService.getCustomCardModelForTesting();

        mTabCountSupplier.set(1);
        assertEquals(1, customCardPropertyModel.get(NUMBER_OF_ARCHIVED_TABS));
        verify(mMessageObserver, times(1))
                .messageReady(eq(MessageType.ARCHIVED_TABS_MESSAGE), any());

        mTabCountSupplier.set(0);
        assertEquals(0, customCardPropertyModel.get(NUMBER_OF_ARCHIVED_TABS));
        verify(mMessageObserver, times(1)).messageInvalidate(MessageType.ARCHIVED_TABS_MESSAGE);
        verify(mAppendMessageRunnable).run();
    }

    @Test
    public void testSendDuplicateMessage() {
        createArchivedTabsMessageService();
        PropertyModel customCardPropertyModel =
                mArchivedTabsMessageService.getCustomCardModelForTesting();

        mTabCountSupplier.set(12);
        assertEquals(12, customCardPropertyModel.get(NUMBER_OF_ARCHIVED_TABS));

        verify(mMessageObserver, times(1))
                .messageReady(eq(MessageType.ARCHIVED_TABS_MESSAGE), any());
        mTabCountSupplier.set(8);
        assertEquals(8, customCardPropertyModel.get(NUMBER_OF_ARCHIVED_TABS));
        // Sending another message to the queue should exit early without sending a message.
        verify(mMessageObserver, times(1))
                .messageReady(eq(MessageType.ARCHIVED_TABS_MESSAGE), any());
        verify(mAppendMessageRunnable, times(1)).run();

        // After invalidating the previous message, a new message should be sent.
        mArchivedTabsMessageService.maybeInvalidatePreviouslySentMessage();
        mArchivedTabsMessageService.maybeSendMessageToQueue(8);
        verify(mMessageObserver, times(2))
                .messageReady(eq(MessageType.ARCHIVED_TABS_MESSAGE), any());
        verify(mMessageObserver, times(1)).messageInvalidate(MessageType.ARCHIVED_TABS_MESSAGE);
        verify(mAppendMessageRunnable, times(2)).run();
    }

    @Test
    public void testClickCard() {
        createArchivedTabsMessageService();
        PropertyModel customCardPropertyModel =
                mArchivedTabsMessageService.getCustomCardModelForTesting();
        customCardPropertyModel.get(CLICK_HANDLER).run();
        verify(mArchivedTabsDialogCoordinator).show(mOnTabSelectingListener);
        verify(mTracker).notifyEvent("android_tab_declutter_button_clicked");
    }

    @Test
    public void testSettingsChangesUpdatesMessage() {
        createArchivedTabsMessageService();
        PropertyModel customCardPropertyModel =
                mArchivedTabsMessageService.getCustomCardModelForTesting();

        doReturn(15).when(mTabArchiveSettings).getArchiveTimeDeltaDays();
        mTabArchiveSettingsObserverCaptor.getValue().onSettingChanged();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_ARCHIVAL_DRAG_DROP_ANDROID)
    public void testObserverInitialized() {
        createArchivedTabsMessageService();
        mTabListCoordinatorSupplier.set(null);

        reset(mTabListCoordinator);
        mTabListCoordinatorSupplier.set(mTabListCoordinator);
        verify(mTabListCoordinator).setOnDropOnArchivalMessageCardEventListener(any());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_ARCHIVAL_DRAG_DROP_ANDROID)
    public void testOnDropListener() {
        createArchivedTabsMessageService();
        mTabListCoordinatorSupplier.set(null);

        reset(mTabListCoordinator);
        mTabListCoordinatorSupplier.set(mTabListCoordinator);

        verify(mTabListCoordinator)
                .setOnDropOnArchivalMessageCardEventListener(mOnDropObserverCaptor.capture());

        OnDropOnArchivalMessageCardEventListener onDropObserver = mOnDropObserverCaptor.getValue();
        onDropObserver.onDropTab(TAB_ID);

        verify(mTabModel).getTabById(TAB_ID);
        verify(mTabArchiver).archiveAndRemoveTabs(any(), eq(List.of(mTab)));
    }

    @Test
    public void testDestroy() {
        createArchivedTabsMessageService();
        assertTrue(mTabCountSupplier.hasObservers());

        mArchivedTabsMessageService.destroy();
        verify(mTabArchiveSettings).removeObserver(mTabArchiveSettingsObserverCaptor.getValue());
        verify(mArchivedTabsDialogCoordinator).destroy();
        verify(mTabListCoordinator).removeTabListItemSizeChangedObserver(any());
        assertFalse(mTabCountSupplier.hasObservers());
    }

    @Test
    public void testDestroyDialogWhenLeavingTabSwitcher() {
        createArchivedTabsMessageService();
        mLayoutStateObserverCaptor.getValue().onStartedHiding(LayoutType.BROWSING);
        verify(mArchivedTabsDialogCoordinator, times(0)).destroy();
        assertNotNull(mArchivedTabsMessageService.getArchivedTabsDialogCoordinatorForTesting());

        mLayoutStateObserverCaptor.getValue().onStartedHiding(LayoutType.TAB_SWITCHER);
        verify(mArchivedTabsDialogCoordinator).destroy();
        assertNull(mArchivedTabsMessageService.getArchivedTabsDialogCoordinatorForTesting());
    }

    @Test
    public void testIphShownThisSession() {
        TabArchiveSettings.setIphShownThisSession(true);
        createArchivedTabsMessageService();

        PropertyModel customCardPropertyModel =
                mArchivedTabsMessageService.getCustomCardModelForTesting();

        mTabCountSupplier.set(12);
        assertEquals(12, customCardPropertyModel.get(NUMBER_OF_ARCHIVED_TABS));

        verify(mMessageObserver).messageReady(eq(MessageType.ARCHIVED_TABS_MESSAGE), any());
        mTabCountSupplier.set(8);
        doReturn(true)
                .when(mTabListCoordinator)
                .specialItemExists(MessageType.ARCHIVED_TABS_MESSAGE);
        mArchivedTabsMessageService.onAppendedMessage();
        ShadowLooper.runUiThreadTasks();

        // The bit should be reset.
        assertFalse(TabArchiveSettings.getIphShownThisSession());
        verify(mTabListCoordinator).setRecyclerViewPosition(any());
    }
}
