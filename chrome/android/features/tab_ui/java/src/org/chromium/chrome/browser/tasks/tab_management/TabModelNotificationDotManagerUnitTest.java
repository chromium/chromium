// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.text.TextUtils;

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

import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.collaboration.CollaborationServiceFactory;
import org.chromium.chrome.browser.collaboration.messaging.MessagingBackendServiceFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab_ui.TabModelDotInfo;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilterObserver;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.collaboration.ServiceStatus;
import org.chromium.components.collaboration.messaging.MessageAttribution;
import org.chromium.components.collaboration.messaging.MessagingBackendService;
import org.chromium.components.collaboration.messaging.MessagingBackendService.PersistentMessageObserver;
import org.chromium.components.collaboration.messaging.PersistentMessage;
import org.chromium.components.collaboration.messaging.PersistentNotificationType;
import org.chromium.components.collaboration.messaging.TabMessageMetadata;

import java.util.ArrayList;
import java.util.List;
import java.util.Optional;

/** Unit tests for {@link TabModelNotificationDotManager}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabModelNotificationDotManagerUnitTest {
    private static final int EXISTING_TAB_ID = 5;
    private static final int ROOT_ID = 6;
    private static final int NON_EXISTANT_TAB_ID = 7;
    private static final int TAB_COUNT = 3;
    private static final Token TAB_GROUP_ID = new Token(378L, 4378L);
    private static final String TITLE = "Vacation";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabGroupModelFilterProvider mTabGroupModelFilterProvider;
    @Mock private TabGroupModelFilter mTabGroupModelFilter;
    @Mock private TabModel mTabModel;
    @Mock private Tab mTab;
    @Mock private MessagingBackendService mMessagingBackendService;
    @Mock private CollaborationService mCollaborationService;
    @Mock private ServiceStatus mServiceStatus;

    @Captor private ArgumentCaptor<PersistentMessageObserver> mPersistentMessageObserverCaptor;
    @Captor private ArgumentCaptor<TabModelSelectorObserver> mTabModelSelectorObserverCaptor;
    @Captor private ArgumentCaptor<TabModelObserver> mTabModelObserverCaptor;
    @Captor private ArgumentCaptor<TabGroupModelFilterObserver> mTabGroupModelFilterObserverCaptor;

    private final PersistentMessage mDirtyTabMessage = new PersistentMessage();
    private final PersistentMessage mNonDirtyTabMessage = new PersistentMessage();

    private TabModelNotificationDotManager mTabModelNotificationDotManager;

    @Before
    public void setUp() {
        mDirtyTabMessage.type = PersistentNotificationType.DIRTY_TAB;
        mNonDirtyTabMessage.type = PersistentNotificationType.CHIP;

        when(mServiceStatus.isAllowedToJoin()).thenReturn(true);
        when(mCollaborationService.getServiceStatus()).thenReturn(mServiceStatus);
        CollaborationServiceFactory.setForTesting(mCollaborationService);
        MessagingBackendServiceFactory.setForTesting(mMessagingBackendService);

        when(mTabModelSelector.isTabStateInitialized()).thenReturn(false);
        when(mTabModelSelector.getTabGroupModelFilterProvider())
                .thenReturn(mTabGroupModelFilterProvider);
        when(mTabGroupModelFilterProvider.getTabGroupModelFilter(false))
                .thenReturn(mTabGroupModelFilter);
        when(mTabGroupModelFilter.getTabModel()).thenReturn(mTabModel);
        when(mTabGroupModelFilter.getTabGroupIdFromRootId(ROOT_ID)).thenReturn(TAB_GROUP_ID);
        when(mTabGroupModelFilter.getRootIdFromTabGroupId(TAB_GROUP_ID)).thenReturn(ROOT_ID);
        when(mTabGroupModelFilter.getTabCountForGroup(TAB_GROUP_ID)).thenReturn(TAB_COUNT);
        when(mTabGroupModelFilter.getTabGroupTitle(ROOT_ID)).thenReturn(TITLE);
        when(mTabModel.getProfile()).thenReturn(mProfile);
        when(mTabModel.getTabById(EXISTING_TAB_ID)).thenReturn(mTab);
        when(mTab.getRootId()).thenReturn(ROOT_ID);

        Context context = ApplicationProvider.getApplicationContext();
        mTabModelNotificationDotManager = new TabModelNotificationDotManager(context);
        mTabModelNotificationDotManager.initWithNative(mTabModelSelector);

        verify(mMessagingBackendService)
                .addPersistentMessageObserver(mPersistentMessageObserverCaptor.capture());
        verify(mTabModelSelector).addObserver(mTabModelSelectorObserverCaptor.capture());
    }

    @After
    public void tearDown() {
        mTabModelNotificationDotManager.destroy();

        verify(mMessagingBackendService).removePersistentMessageObserver(any());
    }

    @Test
    public void testDestroyNoNativeInit() {
        Context context = ApplicationProvider.getApplicationContext();
        TabModelNotificationDotManager notificationDotManager =
                new TabModelNotificationDotManager(context);

        // Verify this doesn't crash if called before native is initialized.
        notificationDotManager.destroy();
    }

    @Test
    public void testUpdateOnMessagingBackendServiceInitializedLast() {
        createDirtyTabMessageForIds(List.of(EXISTING_TAB_ID));

        mTabModelSelectorObserverCaptor.getValue().onTabStateInitialized();
        verify(mTabModel).addObserver(mTabModelObserverCaptor.capture());
        verify(mTabGroupModelFilter)
                .addTabGroupObserver(mTabGroupModelFilterObserverCaptor.capture());
        verifyHidden();

        mPersistentMessageObserverCaptor.getValue().onMessagingBackendServiceInitialized();
        verifyShown();
    }

    @Test
    public void testUpdateOnTabModelSelectorInitializedLast() {
        createDirtyTabMessageForIds(List.of(EXISTING_TAB_ID));

        mPersistentMessageObserverCaptor.getValue().onMessagingBackendServiceInitialized();
        verifyHidden();

        mTabModelSelectorObserverCaptor.getValue().onTabStateInitialized();
        verify(mTabModel).addObserver(mTabModelObserverCaptor.capture());
        verify(mTabGroupModelFilter)
                .addTabGroupObserver(mTabGroupModelFilterObserverCaptor.capture());
        verifyShown();
    }

    @Test
    public void testHidePersistentMessage() {
        initializeBothBackends();
        createDirtyTabMessageForIds(List.of(EXISTING_TAB_ID));
        // Set to visible.
        mPersistentMessageObserverCaptor.getValue().displayPersistentMessage(mDirtyTabMessage);
        verifyShown();

        createDirtyTabMessageForIds(List.of(NON_EXISTANT_TAB_ID));

        // Cannot hide if not dirty message related.
        mPersistentMessageObserverCaptor.getValue().hidePersistentMessage(mNonDirtyTabMessage);
        verifyShown();

        mPersistentMessageObserverCaptor.getValue().hidePersistentMessage(mDirtyTabMessage);
        verifyHidden();

        // One way latching; hide should only hide it cannot show.
        createDirtyTabMessageForIds(List.of(EXISTING_TAB_ID));
        mPersistentMessageObserverCaptor.getValue().hidePersistentMessage(mDirtyTabMessage);
        verifyHidden();
    }

    @Test
    public void testShowPersistentMessage() {
        initializeBothBackends();
        createDirtyTabMessageForIds(List.of(EXISTING_TAB_ID));

        mPersistentMessageObserverCaptor.getValue().displayPersistentMessage(mNonDirtyTabMessage);
        verifyHidden();

        mPersistentMessageObserverCaptor.getValue().displayPersistentMessage(mDirtyTabMessage);
        verifyShown();

        // One way latching; display should only show it cannot hide.
        createDirtyTabMessageForIds(List.of(NON_EXISTANT_TAB_ID));
        mPersistentMessageObserverCaptor.getValue().displayPersistentMessage(mDirtyTabMessage);
        verifyShown();
    }

    @Test
    public void testComputeUpdateTabPresence() {
        initializeBothBackends();

        createDirtyTabMessageForIds(List.of(EXISTING_TAB_ID));
        mPersistentMessageObserverCaptor.getValue().displayPersistentMessage(mDirtyTabMessage);
        verifyShown();

        createDirtyTabMessageForIds(List.of(NON_EXISTANT_TAB_ID));
        mPersistentMessageObserverCaptor.getValue().hidePersistentMessage(mDirtyTabMessage);
        verifyHidden();

        createDirtyTabMessageForIds(List.of(NON_EXISTANT_TAB_ID, EXISTING_TAB_ID));
        mPersistentMessageObserverCaptor.getValue().displayPersistentMessage(mDirtyTabMessage);
        verifyShown();
    }

    @Test
    public void testComputeUpdateTabGroupModelFilterObserver() {
        initializeBothBackends();
        createDirtyTabMessageForIds(List.of(EXISTING_TAB_ID));

        mTabGroupModelFilterObserverCaptor.getValue().didMergeTabToGroup(mTab);
        verifyHidden();
    }

    @Test
    public void testComputeUpdateTabModelObserver() {
        initializeBothBackends();
        createDirtyTabMessageForIds(List.of(EXISTING_TAB_ID));

        mTabModelObserverCaptor
                .getValue()
                .didAddTab(
                        mTab,
                        TabLaunchType.FROM_SYNC_BACKGROUND,
                        TabCreationState.LIVE_IN_BACKGROUND,
                        /* markedForSelection= */ false);
        verifyHidden();

        when(mTab.getTabGroupId()).thenReturn(TAB_GROUP_ID);

        mTabModelObserverCaptor
                .getValue()
                .didAddTab(
                        mTab,
                        TabLaunchType.FROM_SYNC_BACKGROUND,
                        TabCreationState.LIVE_IN_BACKGROUND,
                        /* markedForSelection= */ false);
        verifyShown();

        when(mTabModel.getTabById(EXISTING_TAB_ID)).thenReturn(null);
        mTabModelObserverCaptor.getValue().tabRemoved(mTab);
        verifyHidden();

        when(mTabModel.getTabById(EXISTING_TAB_ID)).thenReturn(mTab);
        mTabModelObserverCaptor.getValue().tabClosureUndone(mTab);
        verifyShown();

        when(mTabModel.getTabById(EXISTING_TAB_ID)).thenReturn(null);
        mTabModelObserverCaptor.getValue().onFinishingTabClosure(mTab);
        verifyHidden();

        when(mTabModel.getTabById(EXISTING_TAB_ID)).thenReturn(mTab);
        mTabModelObserverCaptor.getValue().tabClosureUndone(mTab);
        verifyShown();

        when(mTabModel.getTabById(EXISTING_TAB_ID)).thenReturn(null);
        mTabModelObserverCaptor.getValue().willCloseTab(mTab, true);
        verifyHidden();
    }

    @Test
    public void testFallbackTitle() {
        when(mTabGroupModelFilter.getTabGroupTitle(ROOT_ID)).thenReturn(null);
        initializeBothBackends();
        createDirtyTabMessageForIds(List.of(EXISTING_TAB_ID));

        // Set to visible.
        mPersistentMessageObserverCaptor.getValue().displayPersistentMessage(mDirtyTabMessage);

        verifyShown("3 tabs");
    }

    private void initializeBothBackends() {
        when(mTabModelSelector.isTabStateInitialized()).thenReturn(true);
        mPersistentMessageObserverCaptor.getValue().onMessagingBackendServiceInitialized();
        mTabModelSelectorObserverCaptor.getValue().onTabStateInitialized();
        verify(mTabModel).addObserver(mTabModelObserverCaptor.capture());
        verify(mTabGroupModelFilter)
                .addTabGroupObserver(mTabGroupModelFilterObserverCaptor.capture());
    }

    private void createDirtyTabMessageForIds(List<Integer> ids) {
        List<PersistentMessage> messages = new ArrayList<>();

        for (int id : ids) {
            PersistentMessage message = new PersistentMessage();
            message.type = PersistentNotificationType.DIRTY_TAB;
            MessageAttribution attribution = new MessageAttribution();
            message.attribution = attribution;
            TabMessageMetadata tabMetadata = new TabMessageMetadata();
            tabMetadata.localTabId = id;
            attribution.tabMetadata = tabMetadata;
            messages.add(message);
        }

        when(mMessagingBackendService.getMessages(
                        Optional.of(PersistentNotificationType.DIRTY_TAB)))
                .thenReturn(messages);
    }

    private void verifyHidden() {
        TabModelDotInfo tabModelDotInfo =
                mTabModelNotificationDotManager.getNotificationDotObservableSupplier().get();
        assertFalse(tabModelDotInfo.showDot);
        assertTrue(TextUtils.isEmpty(tabModelDotInfo.tabGroupTitle));
    }

    private void verifyShown() {
        verifyShown(TITLE);
    }

    private void verifyShown(String expectedTitle) {
        TabModelDotInfo tabModelDotInfo =
                mTabModelNotificationDotManager.getNotificationDotObservableSupplier().get();
        assertTrue(tabModelDotInfo.showDot);
        assertEquals(expectedTitle, tabModelDotInfo.tabGroupTitle);
    }
}
