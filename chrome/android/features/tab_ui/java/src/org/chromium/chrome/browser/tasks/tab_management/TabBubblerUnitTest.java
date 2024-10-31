// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

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
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.collaboration.messaging.MessagingBackendServiceFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.collaboration.messaging.MessageAttribution;
import org.chromium.components.collaboration.messaging.MessagingBackendService;
import org.chromium.components.collaboration.messaging.MessagingBackendService.PersistentMessageObserver;
import org.chromium.components.collaboration.messaging.PersistentMessage;
import org.chromium.components.collaboration.messaging.PersistentNotificationType;
import org.chromium.components.collaboration.messaging.TabGroupMessageMetadata;
import org.chromium.components.collaboration.messaging.TabMessageMetadata;
import org.chromium.components.tab_group_sync.LocalTabGroupId;

import java.util.List;
import java.util.Set;

/** Unit tests for {@link TabBubbler}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabBubblerUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    private static final Token GROUP_ID1 = new Token(1L, 11L);
    private static final Token GROUP_ID2 = new Token(2L, 22L);
    private static final int TAB_ID1 = 1;

    @Mock private Profile mProfile;
    @Mock private TabListNotificationHandler mTabListNotificationHandler;
    @Mock private MessagingBackendService mMessagingBackendService;

    @Captor private ArgumentCaptor<PersistentMessageObserver> mPersistentMessageObserverCaptor;
    @Captor private ArgumentCaptor<Set<Integer>> mTabIdsCaptor;

    private final ObservableSupplierImpl<Token> mTabGroupIdSupplier =
            new ObservableSupplierImpl<>();

    private TabBubbler mTabBubbler;

    @Before
    public void setUp() {
        MessagingBackendServiceFactory.setForTesting(mMessagingBackendService);
        mTabGroupIdSupplier.set(GROUP_ID1);
        mTabBubbler = new TabBubbler(mProfile, mTabListNotificationHandler, mTabGroupIdSupplier);
    }

    private PersistentMessage makeStandardMessage() {
        PersistentMessage message = new PersistentMessage();
        message.type = PersistentNotificationType.DIRTY_TAB;
        message.attribution = new MessageAttribution();
        message.attribution.tabMetadata = new TabMessageMetadata();
        message.attribution.tabMetadata.localTabId = TAB_ID1;
        message.attribution.tabGroupMetadata = new TabGroupMessageMetadata();
        message.attribution.tabGroupMetadata.localTabGroupId = new LocalTabGroupId(GROUP_ID1);
        return message;
    }

    @Test
    public void testDestroy() {
        verify(mMessagingBackendService)
                .addPersistentMessageObserver(mPersistentMessageObserverCaptor.capture());
        mTabBubbler.destroy();
        verify(mMessagingBackendService)
                .removePersistentMessageObserver(mPersistentMessageObserverCaptor.getValue());
    }

    @Test
    public void testShowAll_Added() {
        List<PersistentMessage> messageList = List.of(makeStandardMessage());
        when(mMessagingBackendService.getMessagesForGroup(any(), any())).thenReturn(messageList);

        mTabBubbler.showAll();

        verify(mTabListNotificationHandler)
                .updateTabStripNotificationBubble(mTabIdsCaptor.capture(), eq(true));
        assertTrue(mTabIdsCaptor.getValue().contains(TAB_ID1));
    }

    @Test
    public void testShowAll_DifferentTabGroup() {
        PersistentMessage message = makeStandardMessage();
        message.attribution.tabGroupMetadata.localTabGroupId = new LocalTabGroupId(GROUP_ID2);
        List<PersistentMessage> messageList = List.of(message);
        when(mMessagingBackendService.getMessagesForGroup(any(), any())).thenReturn(messageList);

        mTabBubbler.showAll();
        verify(mTabListNotificationHandler, never())
                .updateTabStripNotificationBubble(any(), anyBoolean());
    }

    @Test
    public void testShowAll_NullAttributionTabGroup() {
        PersistentMessage message = makeStandardMessage();
        message.attribution.tabGroupMetadata.localTabGroupId = null;
        List<PersistentMessage> messageList = List.of(message);
        when(mMessagingBackendService.getMessagesForGroup(any(), any())).thenReturn(messageList);

        mTabBubbler.showAll();
        verify(mTabListNotificationHandler, never())
                .updateTabStripNotificationBubble(any(), anyBoolean());
    }

    @Test
    public void testShowAll_NullCurrentTabGroup() {
        mTabGroupIdSupplier.set(null);
        List<PersistentMessage> messageList = List.of(makeStandardMessage());
        when(mMessagingBackendService.getMessagesForGroup(any(), any())).thenReturn(messageList);

        mTabBubbler.showAll();
        verify(mTabListNotificationHandler, never())
                .updateTabStripNotificationBubble(any(), anyBoolean());
    }

    @Test
    public void testShowAll_WrongMessageType() {
        PersistentMessage message = makeStandardMessage();
        message.type = PersistentNotificationType.CHIP;
        List<PersistentMessage> messageList = List.of(message);
        when(mMessagingBackendService.getMessagesForGroup(any(), any())).thenReturn(messageList);

        mTabBubbler.showAll();
        verify(mTabListNotificationHandler, never())
                .updateTabStripNotificationBubble(any(), anyBoolean());
    }

    @Test
    public void testShowAll_InvalidTabId() {
        PersistentMessage message = makeStandardMessage();
        message.attribution.tabMetadata.localTabId = Tab.INVALID_TAB_ID;
        List<PersistentMessage> messageList = List.of(message);
        when(mMessagingBackendService.getMessagesForGroup(any(), any())).thenReturn(messageList);

        mTabBubbler.showAll();
        verify(mTabListNotificationHandler, never())
                .updateTabStripNotificationBubble(any(), anyBoolean());
    }

    @Test
    public void testDisplayPersistentMessage() {
        verify(mMessagingBackendService)
                .addPersistentMessageObserver(mPersistentMessageObserverCaptor.capture());
        mPersistentMessageObserverCaptor.getValue().displayPersistentMessage(makeStandardMessage());

        verify(mTabListNotificationHandler)
                .updateTabStripNotificationBubble(mTabIdsCaptor.capture(), eq(true));
        assertTrue(mTabIdsCaptor.getValue().contains(TAB_ID1));
    }

    @Test
    public void testHidePersistentMessage() {
        verify(mMessagingBackendService)
                .addPersistentMessageObserver(mPersistentMessageObserverCaptor.capture());
        mPersistentMessageObserverCaptor.getValue().hidePersistentMessage(makeStandardMessage());

        verify(mTabListNotificationHandler)
                .updateTabStripNotificationBubble(mTabIdsCaptor.capture(), eq(false));
        assertTrue(mTabIdsCaptor.getValue().contains(TAB_ID1));
    }

    @Test
    public void testOnMessagingBackendServiceInitialized() {
        List<PersistentMessage> messageList = List.of(makeStandardMessage());
        when(mMessagingBackendService.getMessagesForGroup(any(), any())).thenReturn(messageList);

        verify(mMessagingBackendService)
                .addPersistentMessageObserver(mPersistentMessageObserverCaptor.capture());
        mPersistentMessageObserverCaptor.getValue().onMessagingBackendServiceInitialized();

        verify(mTabListNotificationHandler)
                .updateTabStripNotificationBubble(mTabIdsCaptor.capture(), eq(true));
        assertTrue(mTabIdsCaptor.getValue().contains(TAB_ID1));
    }
}
