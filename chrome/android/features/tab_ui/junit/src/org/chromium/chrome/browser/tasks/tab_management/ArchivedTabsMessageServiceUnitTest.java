// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.tasks.tab_management.ArchivedTabsCardViewProperties.ARCHIVE_TIME_DELTA_DAYS;
import static org.chromium.chrome.browser.tasks.tab_management.ArchivedTabsCardViewProperties.CLICK_HANDLER;
import static org.chromium.chrome.browser.tasks.tab_management.ArchivedTabsCardViewProperties.NUMBER_OF_ARCHIVED_TABS;

import android.app.Activity;
import android.view.ViewGroup;
import android.widget.FrameLayout;

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
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.app.tabmodel.ArchivedTabModelOrchestrator;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.tab.TabArchiveSettings;
import org.chromium.chrome.browser.tab_ui.OnTabSelectingListener;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.tab_management.MessageService.MessageType;
import org.chromium.chrome.browser.tasks.tab_management.TabListCoordinator.TabListMode;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;

/** Tests for ArchivedTabsMessageService. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ArchivedTabsMessageServiceUnitTest {
    private static final int TIME_DELTA_DAYS = 10;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.LENIENT);

    @Mock private ArchivedTabModelOrchestrator mArchivedTabModelOrchestrator;
    @Mock private TabArchiveSettings mTabArchiveSettings;
    @Mock private TabModel mArchivedTabModel;
    @Mock private MessageService.MessageObserver mMessageObserver;
    @Mock private ArchivedTabsDialogCoordinator mArchivedTabsDialogCoordinator;
    @Mock private BrowserControlsStateProvider mBrowserControlsStateProvider;
    @Mock private TabContentManager mTabContentManager;
    @Mock private SnackbarManager mSnackbarManager;
    @Mock private TabCreator mRegularTabCreator;
    @Mock private BackPressManager mBackPressManager;
    @Mock private OnTabSelectingListener mOnTabSelectingListener;
    @Mock private ModalDialogManager mModalDialogManager;
    @Captor private ArgumentCaptor<TabArchiveSettings.Observer> mTabArchiveSettingsObserver;

    private Activity mActivity;
    private ViewGroup mRootView;
    private ArchivedTabsMessageService mArchivedTabsMessageService;
    private ObservableSupplierImpl<Integer> mTabCountSupplier = new ObservableSupplierImpl<>();

    @Before
    public void setUp() throws Exception {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mRootView = new FrameLayout(mActivity);

        doReturn(TIME_DELTA_DAYS).when(mTabArchiveSettings).getArchiveTimeDeltaDays();
        doReturn(mTabCountSupplier).when(mArchivedTabModel).getTabCountSupplier();

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
                        mModalDialogManager);
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
        verify(mTabArchiveSettings).addObserver(mTabArchiveSettingsObserver.capture());
    }

    @Test
    public void testTabAddedThenRemoved() {
        PropertyModel customCardPropertyModel =
                mArchivedTabsMessageService.getCustomCardModelForTesting();

        doReturn(1).when(mArchivedTabModel).getCount();
        mTabCountSupplier.set(1);
        assertEquals(1, customCardPropertyModel.get(NUMBER_OF_ARCHIVED_TABS));
        assertEquals(10, customCardPropertyModel.get(ARCHIVE_TIME_DELTA_DAYS));
        verify(mMessageObserver, times(1))
                .messageReady(eq(MessageType.ARCHIVED_TABS_MESSAGE), any());

        doReturn(0).when(mArchivedTabModel).getCount();
        mTabCountSupplier.set(0);
        assertEquals(0, customCardPropertyModel.get(NUMBER_OF_ARCHIVED_TABS));
        assertEquals(10, customCardPropertyModel.get(ARCHIVE_TIME_DELTA_DAYS));
        verify(mMessageObserver, times(1)).messageInvalidate(MessageType.ARCHIVED_TABS_MESSAGE);
    }

    @Test
    public void testSendDuplicateMessage() {
        PropertyModel customCardPropertyModel =
                mArchivedTabsMessageService.getCustomCardModelForTesting();

        doReturn(12).when(mArchivedTabModel).getCount();
        mTabCountSupplier.set(12);
        assertEquals(12, customCardPropertyModel.get(NUMBER_OF_ARCHIVED_TABS));
        assertEquals(10, customCardPropertyModel.get(ARCHIVE_TIME_DELTA_DAYS));

        doReturn(8).when(mArchivedTabModel).getCount();
        verify(mMessageObserver, times(1))
                .messageReady(eq(MessageType.ARCHIVED_TABS_MESSAGE), any());
        mTabCountSupplier.set(8);
        assertEquals(8, customCardPropertyModel.get(NUMBER_OF_ARCHIVED_TABS));
        assertEquals(10, customCardPropertyModel.get(ARCHIVE_TIME_DELTA_DAYS));
        // Sending another message to the queue should exit early without sending a message.
        verify(mMessageObserver, times(1))
                .messageReady(eq(MessageType.ARCHIVED_TABS_MESSAGE), any());

        // After invalidating the previous message, a new message should be sent.
        mArchivedTabsMessageService.maybeInvalidatePreviouslySentMessage();
        mArchivedTabsMessageService.maybeSendMessageToQueue();
        verify(mMessageObserver, times(2))
                .messageReady(eq(MessageType.ARCHIVED_TABS_MESSAGE), any());
        verify(mMessageObserver, times(1)).messageInvalidate(MessageType.ARCHIVED_TABS_MESSAGE);
    }

    @Test
    public void testClickCard() {
        PropertyModel customCardPropertyModel =
                mArchivedTabsMessageService.getCustomCardModelForTesting();
        customCardPropertyModel.get(CLICK_HANDLER).run();
        verify(mArchivedTabsDialogCoordinator).show(mOnTabSelectingListener);
    }

    @Test
    public void testCustomViewDetached() {
        mRootView.addView(mArchivedTabsMessageService.getCustomView());
        assertNotNull(mArchivedTabsMessageService.getCustomView().getParent());

        mArchivedTabsMessageService.onRemoveAllAppendedMessage();
        assertNull(mArchivedTabsMessageService.getCustomView().getParent());
    }

    @Test
    public void testSettingsChangesUpdatesMessage() {
        PropertyModel customCardPropertyModel =
                mArchivedTabsMessageService.getCustomCardModelForTesting();

        doReturn(15).when(mTabArchiveSettings).getArchiveTimeDeltaDays();
        mTabArchiveSettingsObserver.getValue().onSettingChanged();
        assertEquals(15, customCardPropertyModel.get(ARCHIVE_TIME_DELTA_DAYS));
    }

    @Test
    public void testDestroy() {
        mArchivedTabsMessageService.destroy();
        verify(mTabArchiveSettings).removeObserver(mTabArchiveSettingsObserver.getValue());
    }
}
