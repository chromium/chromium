// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;

import androidx.test.core.app.ApplicationProvider;

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
import org.chromium.components.collaboration.messaging.CollaborationEvent;
import org.chromium.components.collaboration.messaging.MessageAttribution;
import org.chromium.components.collaboration.messaging.MessagingBackendService;
import org.chromium.components.collaboration.messaging.MessagingBackendService.PersistentMessageObserver;
import org.chromium.components.collaboration.messaging.PersistentMessage;
import org.chromium.components.collaboration.messaging.PersistentNotificationType;
import org.chromium.components.collaboration.messaging.TabGroupMessageMetadata;
import org.chromium.components.collaboration.messaging.TabMessageMetadata;
import org.chromium.components.tab_group_sync.LocalTabGroupId;

import java.util.List;
import java.util.Map;

/** Unit tests for {@link TabLabeller}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabLabellerUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    private static final Token GROUP_ID1 = new Token(1L, 11L);
    private static final Token GROUP_ID2 = new Token(2L, 22L);
    private static final int TAB_ID1 = 1;
    private static final int TAB_ID2 = 2;

    @Mock private Profile mProfile;
    @Mock private TabListNotificationHandler mTabListNotificationHandler;
    @Mock private MessagingBackendService mMessagingBackendService;

    @Captor private ArgumentCaptor<PersistentMessageObserver> mPersistentMessageObserverCaptor;
    @Captor private ArgumentCaptor<Map<Integer, TabCardLabelData>> mLabelDataCaptor;

    private final ObservableSupplierImpl<Token> mTabGroupIdSupplier =
            new ObservableSupplierImpl<>();

    private Context mContext;
    private TabLabeller mTabLabeller;

    @Before
    public void setUp() {
        MessagingBackendServiceFactory.setForTesting(mMessagingBackendService);
        mContext = ApplicationProvider.getApplicationContext();
        mTabGroupIdSupplier.set(GROUP_ID1);
        mTabLabeller = new TabLabeller(mProfile, mTabListNotificationHandler, mTabGroupIdSupplier);
    }

    private PersistentMessage makeStandardMessage() {
        PersistentMessage message = new PersistentMessage();
        message.type = PersistentNotificationType.CHIP;
        message.attribution = new MessageAttribution();
        message.attribution.tabMetadata = new TabMessageMetadata();
        message.attribution.tabMetadata.localTabId = 1;
        message.attribution.tabGroupMetadata = new TabGroupMessageMetadata();
        message.attribution.tabGroupMetadata.localTabGroupId = new LocalTabGroupId(GROUP_ID1);
        message.collaborationEvent = CollaborationEvent.TAB_ADDED;
        return message;
    }

    private void assertContainsLabel(
            Map<Integer, TabCardLabelData> labelDataMap, int tabId, String text) {
        assertTrue(labelDataMap.containsKey(tabId));
        TabCardLabelData labelData = labelDataMap.get(tabId);
        assertEquals(TabCardLabelType.ACTIVITY_UPDATE, labelData.labelType);
        labelData.contentDescriptionResolver.resolve(mContext);
        assertEquals(text, labelData.textResolver.resolve(mContext));
    }

    private void assertContainsNullLabel(Map<Integer, TabCardLabelData> labelDataMap, int tabId) {
        assertTrue(labelDataMap.containsKey(tabId));
        TabCardLabelData labelData = labelDataMap.get(tabId);
        assertNull(labelData);
    }

    @Test
    public void testDestroy() {
        verify(mMessagingBackendService)
                .addPersistentMessageObserver(mPersistentMessageObserverCaptor.capture());
        mTabLabeller.destroy();
        verify(mMessagingBackendService)
                .removePersistentMessageObserver(mPersistentMessageObserverCaptor.getValue());
    }

    @Test
    public void testShowAll_Added() {
        List<PersistentMessage> messageList = List.of(makeStandardMessage());
        when(mMessagingBackendService.getMessagesForGroup(any(), any())).thenReturn(messageList);

        mTabLabeller.showAll();

        verify(mTabListNotificationHandler).updateTabCardLabels(mLabelDataCaptor.capture());
        assertContainsLabel(mLabelDataCaptor.getValue(), TAB_ID1, "Added");
    }

    @Test
    public void testShowAll_Multiple() {
        PersistentMessage message1 = makeStandardMessage();
        PersistentMessage message2 = makeStandardMessage();
        message2.attribution.tabMetadata.localTabId = TAB_ID2;
        List<PersistentMessage> messageList = List.of(message1, message2);
        when(mMessagingBackendService.getMessagesForGroup(any(), any())).thenReturn(messageList);

        mTabLabeller.showAll();

        verify(mTabListNotificationHandler).updateTabCardLabels(mLabelDataCaptor.capture());
        assertContainsLabel(mLabelDataCaptor.getValue(), TAB_ID1, "Added");
        assertContainsLabel(mLabelDataCaptor.getValue(), TAB_ID2, "Added");
    }

    @Test
    public void testShowAll_DifferentTabGroup() {
        PersistentMessage message = makeStandardMessage();
        message.attribution.tabGroupMetadata.localTabGroupId = new LocalTabGroupId(GROUP_ID2);
        List<PersistentMessage> messageList = List.of(message);
        when(mMessagingBackendService.getMessagesForGroup(any(), any())).thenReturn(messageList);

        mTabLabeller.showAll();
        verify(mTabListNotificationHandler, never()).updateTabCardLabels(any());
    }

    @Test
    public void testShowAll_NullAttributionTabGroup() {
        PersistentMessage message = makeStandardMessage();
        message.attribution.tabGroupMetadata.localTabGroupId = null;
        List<PersistentMessage> messageList = List.of(message);
        when(mMessagingBackendService.getMessagesForGroup(any(), any())).thenReturn(messageList);

        mTabLabeller.showAll();
        verify(mTabListNotificationHandler, never()).updateTabCardLabels(any());
    }

    @Test
    public void testShowAll_NullCurrentTabGroup() {
        mTabGroupIdSupplier.set(null);
        List<PersistentMessage> messageList = List.of(makeStandardMessage());
        when(mMessagingBackendService.getMessagesForGroup(any(), any())).thenReturn(messageList);

        mTabLabeller.showAll();
        verify(mTabListNotificationHandler, never()).updateTabCardLabels(any());
    }

    @Test
    public void testShowAll_WrongMessageType() {
        PersistentMessage message = makeStandardMessage();
        message.type = PersistentNotificationType.DIRTY_TAB;
        List<PersistentMessage> messageList = List.of(message);
        when(mMessagingBackendService.getMessagesForGroup(any(), any())).thenReturn(messageList);

        mTabLabeller.showAll();
        verify(mTabListNotificationHandler, never()).updateTabCardLabels(any());
    }

    @Test
    public void testShowAll_InvalidTabId() {
        PersistentMessage message = makeStandardMessage();
        message.attribution.tabMetadata.localTabId = Tab.INVALID_TAB_ID;
        List<PersistentMessage> messageList = List.of(message);
        when(mMessagingBackendService.getMessagesForGroup(any(), any())).thenReturn(messageList);

        mTabLabeller.showAll();
        verify(mTabListNotificationHandler, never()).updateTabCardLabels(any());
    }

    @Test
    public void testShowAll_WrongUserAction() {
        PersistentMessage message = makeStandardMessage();
        message.collaborationEvent = CollaborationEvent.COLLABORATION_REMOVED;
        List<PersistentMessage> messageList = List.of(message);
        when(mMessagingBackendService.getMessagesForGroup(any(), any())).thenReturn(messageList);

        mTabLabeller.showAll();
        verify(mTabListNotificationHandler, never()).updateTabCardLabels(any());
    }

    @Test
    public void testShowAll_Changed() {
        PersistentMessage message = makeStandardMessage();
        message.collaborationEvent = CollaborationEvent.TAB_UPDATED;
        List<PersistentMessage> messageList = List.of(message);
        when(mMessagingBackendService.getMessagesForGroup(any(), any())).thenReturn(messageList);

        mTabLabeller.showAll();

        verify(mTabListNotificationHandler).updateTabCardLabels(mLabelDataCaptor.capture());
        assertContainsLabel(mLabelDataCaptor.getValue(), TAB_ID1, "Changed");
    }

    @Test
    public void testDisplayPersistentMessage() {
        verify(mMessagingBackendService)
                .addPersistentMessageObserver(mPersistentMessageObserverCaptor.capture());
        mPersistentMessageObserverCaptor.getValue().displayPersistentMessage(makeStandardMessage());

        verify(mTabListNotificationHandler).updateTabCardLabels(mLabelDataCaptor.capture());
        assertContainsLabel(mLabelDataCaptor.getValue(), TAB_ID1, "Added");
    }

    @Test
    public void testHidePersistentMessage() {
        verify(mMessagingBackendService)
                .addPersistentMessageObserver(mPersistentMessageObserverCaptor.capture());
        mPersistentMessageObserverCaptor.getValue().hidePersistentMessage(makeStandardMessage());

        verify(mTabListNotificationHandler).updateTabCardLabels(mLabelDataCaptor.capture());
        assertContainsNullLabel(mLabelDataCaptor.getValue(), TAB_ID1);
    }

    @Test
    public void testOnMessagingBackendServiceInitialized() {
        List<PersistentMessage> messageList = List.of(makeStandardMessage());
        when(mMessagingBackendService.getMessagesForGroup(any(), any())).thenReturn(messageList);

        verify(mMessagingBackendService)
                .addPersistentMessageObserver(mPersistentMessageObserverCaptor.capture());
        mPersistentMessageObserverCaptor.getValue().onMessagingBackendServiceInitialized();

        verify(mTabListNotificationHandler).updateTabCardLabels(mLabelDataCaptor.capture());
        assertContainsLabel(mLabelDataCaptor.getValue(), TAB_ID1, "Added");
    }
}
