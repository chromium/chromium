// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
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
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.app.tabmodel.ArchivedTabModelOrchestrator;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.tab.TabArchiveSettings;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorBase;
import org.chromium.chrome.browser.tasks.tab_management.MessageService.MessageType;
import org.chromium.chrome.browser.tasks.tab_management.TabListCoordinator.TabListMode;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.concurrent.TimeUnit;

/** Tests for ArchivedTabsMessageService. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ArchivedTabsMessageServiceUnitTest {
    private static final int ARCHIVED_TABS = 12;
    private static final int TIME_DELTA_HOURS = (int) TimeUnit.DAYS.toHours(10);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.LENIENT);

    @Mock private ArchivedTabModelOrchestrator mArchivedTabModelOrchestrator;
    @Mock private TabArchiveSettings mTabArchiveSettings;
    @Mock private TabModel mArchivedTabModel;
    @Mock private MessageService.MessageObserver mMessageObserver;
    @Mock private ArchivedTabsDialogCoordinator mArchivedTabsDialogCoordinator;
    @Mock private TabModelSelectorBase mArchivedTabModelSelector;
    @Mock private BrowserControlsStateProvider mBrowserControlsStateProvider;
    @Mock private TabContentManager mTabContentManager;
    @Mock private SnackbarManager mSnackbarManager;

    private Activity mActivity;
    private ViewGroup mRootView;
    private ArchivedTabsMessageService mArchivedTabsMessageService;

    @Before
    public void setUp() throws Exception {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mRootView = new FrameLayout(mActivity);

        doReturn(TIME_DELTA_HOURS).when(mTabArchiveSettings).getArchiveTimeDeltaHours();
        doReturn(mTabArchiveSettings).when(mArchivedTabModelOrchestrator).getTabArchiveSettings();

        mArchivedTabsMessageService =
                new ArchivedTabsMessageService(
                        mActivity,
                        mArchivedTabModelOrchestrator,
                        mBrowserControlsStateProvider,
                        mTabContentManager,
                        TabListMode.GRID,
                        mRootView,
                        mSnackbarManager);
        mArchivedTabsMessageService.setArchivedTabsDialogCoordiantorForTesting(
                mArchivedTabsDialogCoordinator);
        mArchivedTabsMessageService.addObserver(mMessageObserver);
        mArchivedTabsMessageService
                .getArchivedTabModelOrchestratorObserverForTesting()
                .onTabModelCreated(mArchivedTabModel);
    }

    @Test
    public void testTabAddedThenRemoved() {
        PropertyModel customCardPropertyModel =
                mArchivedTabsMessageService.getCustomCardModelForTesting();

        doReturn(1).when(mArchivedTabModel).getCount();
        // Note: The params don't matter here.
        mArchivedTabsMessageService
                .getArchivedTabModelObserverForTesting()
                .didAddTab(null, 0, 0, false);
        assertEquals(1, customCardPropertyModel.get(NUMBER_OF_ARCHIVED_TABS));
        assertEquals(10, customCardPropertyModel.get(ARCHIVE_TIME_DELTA_DAYS));
        verify(mMessageObserver, times(1))
                .messageReady(eq(MessageType.ARCHIVED_TABS_MESSAGE), any());

        doReturn(0).when(mArchivedTabModel).getCount();
        // Note: The params don't matter here.
        mArchivedTabsMessageService.getArchivedTabModelObserverForTesting().tabRemoved(null);
        assertEquals(0, customCardPropertyModel.get(NUMBER_OF_ARCHIVED_TABS));
        assertEquals(10, customCardPropertyModel.get(ARCHIVE_TIME_DELTA_DAYS));
        verify(mMessageObserver, times(1)).messageInvalidate(MessageType.ARCHIVED_TABS_MESSAGE);
    }

    @Test
    public void testSendDuplicateMessage() {
        PropertyModel customCardPropertyModel =
                mArchivedTabsMessageService.getCustomCardModelForTesting();

        doReturn(12).when(mArchivedTabModel).getCount();
        // Note: The params don't matter here.
        mArchivedTabsMessageService
                .getArchivedTabModelObserverForTesting()
                .didAddTab(null, 0, 0, false);
        assertEquals(12, customCardPropertyModel.get(NUMBER_OF_ARCHIVED_TABS));
        assertEquals(10, customCardPropertyModel.get(ARCHIVE_TIME_DELTA_DAYS));

        doReturn(8).when(mArchivedTabModel).getCount();
        verify(mMessageObserver, times(1))
                .messageReady(eq(MessageType.ARCHIVED_TABS_MESSAGE), any());
        // Note: The params don't matter here.
        mArchivedTabsMessageService
                .getArchivedTabModelObserverForTesting()
                .didAddTab(null, 0, 0, false);
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
        verify(mArchivedTabsDialogCoordinator).show();
    }
}
