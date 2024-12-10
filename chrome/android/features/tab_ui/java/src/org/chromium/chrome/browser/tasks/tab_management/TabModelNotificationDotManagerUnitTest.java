// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

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

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.collaboration.messaging.MessagingBackendServiceFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
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
    private static final int NON_EXISTANT_TAB_ID = 7;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModel mTabModel;
    @Mock private Tab mTab;
    @Mock private MessagingBackendService mMessagingBackendService;

    @Captor private ArgumentCaptor<PersistentMessageObserver> mPersistentMessageObserverCaptor;
    @Captor private ArgumentCaptor<TabModelSelectorObserver> mTabModelSelectorObserverCaptor;

    private TabModelNotificationDotManager mTabModelNotificationDotManager;
    private PersistentMessage mDirtyTabMessage = new PersistentMessage();
    private PersistentMessage mNonDirtyTabMessage = new PersistentMessage();

    @Before
    public void setUp() {
        mDirtyTabMessage.type = PersistentNotificationType.DIRTY_TAB;
        mNonDirtyTabMessage.type = PersistentNotificationType.CHIP;

        MessagingBackendServiceFactory.setForTesting(mMessagingBackendService);

        when(mTabModelSelector.isTabStateInitialized()).thenReturn(false);
        when(mTabModelSelector.getModel(false)).thenReturn(mTabModel);
        when(mTabModel.getProfile()).thenReturn(mProfile);
        when(mTabModel.getTabById(EXISTING_TAB_ID)).thenReturn(mTab);

        mTabModelNotificationDotManager = new TabModelNotificationDotManager(mTabModelSelector);

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
    public void testUpdateOnMessagingBackendServiceInitializedLast() {
        createDirtyTabMessageForIds(List.of(EXISTING_TAB_ID));

        mTabModelSelectorObserverCaptor.getValue().onTabStateInitialized();
        assertFalse(mTabModelNotificationDotManager.getNotificationDotObservableSupplier().get());

        mPersistentMessageObserverCaptor.getValue().onMessagingBackendServiceInitialized();
        assertTrue(mTabModelNotificationDotManager.getNotificationDotObservableSupplier().get());
    }

    @Test
    public void testUpdateOnTabModelSelectorInitializedLast() {
        createDirtyTabMessageForIds(List.of(EXISTING_TAB_ID));

        mPersistentMessageObserverCaptor.getValue().onMessagingBackendServiceInitialized();
        assertFalse(mTabModelNotificationDotManager.getNotificationDotObservableSupplier().get());

        mTabModelSelectorObserverCaptor.getValue().onTabStateInitialized();
        assertTrue(mTabModelNotificationDotManager.getNotificationDotObservableSupplier().get());
    }

    @Test
    public void testHidePersistentMessage() {
        initializeBothBackends();
        createDirtyTabMessageForIds(List.of(EXISTING_TAB_ID));
        // Set to visible.
        mPersistentMessageObserverCaptor.getValue().displayPersistentMessage(mDirtyTabMessage);
        assertTrue(mTabModelNotificationDotManager.getNotificationDotObservableSupplier().get());

        createDirtyTabMessageForIds(List.of(NON_EXISTANT_TAB_ID));

        // Cannot hide if not dirty message related.
        mPersistentMessageObserverCaptor.getValue().hidePersistentMessage(mNonDirtyTabMessage);
        assertTrue(mTabModelNotificationDotManager.getNotificationDotObservableSupplier().get());

        mPersistentMessageObserverCaptor.getValue().hidePersistentMessage(mDirtyTabMessage);
        assertFalse(mTabModelNotificationDotManager.getNotificationDotObservableSupplier().get());

        // One way latching; hide should only hide it cannot show.
        createDirtyTabMessageForIds(List.of(EXISTING_TAB_ID));
        mPersistentMessageObserverCaptor.getValue().hidePersistentMessage(mDirtyTabMessage);
        assertFalse(mTabModelNotificationDotManager.getNotificationDotObservableSupplier().get());
    }

    @Test
    public void testShowPersistentMessage() {
        initializeBothBackends();
        createDirtyTabMessageForIds(List.of(EXISTING_TAB_ID));

        mPersistentMessageObserverCaptor.getValue().displayPersistentMessage(mNonDirtyTabMessage);
        assertFalse(mTabModelNotificationDotManager.getNotificationDotObservableSupplier().get());

        mPersistentMessageObserverCaptor.getValue().displayPersistentMessage(mDirtyTabMessage);
        assertTrue(mTabModelNotificationDotManager.getNotificationDotObservableSupplier().get());

        // One way latching; display should only show it cannot hide.
        createDirtyTabMessageForIds(List.of(NON_EXISTANT_TAB_ID));
        mPersistentMessageObserverCaptor.getValue().displayPersistentMessage(mDirtyTabMessage);
        assertTrue(mTabModelNotificationDotManager.getNotificationDotObservableSupplier().get());
    }

    @Test
    public void testComputeUpdateTabPresence() {
        initializeBothBackends();

        createDirtyTabMessageForIds(List.of(EXISTING_TAB_ID));
        mPersistentMessageObserverCaptor.getValue().displayPersistentMessage(mDirtyTabMessage);
        assertTrue(mTabModelNotificationDotManager.getNotificationDotObservableSupplier().get());

        createDirtyTabMessageForIds(List.of(NON_EXISTANT_TAB_ID));
        mPersistentMessageObserverCaptor.getValue().hidePersistentMessage(mDirtyTabMessage);
        assertFalse(mTabModelNotificationDotManager.getNotificationDotObservableSupplier().get());

        createDirtyTabMessageForIds(List.of(NON_EXISTANT_TAB_ID, EXISTING_TAB_ID));
        mPersistentMessageObserverCaptor.getValue().displayPersistentMessage(mDirtyTabMessage);
        assertTrue(mTabModelNotificationDotManager.getNotificationDotObservableSupplier().get());
    }

    private void initializeBothBackends() {
        when(mTabModelSelector.isTabStateInitialized()).thenReturn(true);
        mPersistentMessageObserverCaptor.getValue().onMessagingBackendServiceInitialized();
        mTabModelSelectorObserverCaptor.getValue().onTabStateInitialized();
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
}
