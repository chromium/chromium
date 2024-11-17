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
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.components.collaboration.messaging.MessageAttribution;
import org.chromium.components.collaboration.messaging.MessagingBackendService;
import org.chromium.components.collaboration.messaging.MessagingBackendService.PersistentMessageObserver;
import org.chromium.components.collaboration.messaging.PersistentMessage;
import org.chromium.components.collaboration.messaging.PersistentNotificationType;
import org.chromium.components.collaboration.messaging.TabGroupMessageMetadata;
import org.chromium.components.tab_group_sync.LocalTabGroupId;

import java.util.List;
import java.util.Map;

/** Unit tests for {@link TabGroupLabeller}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabGroupLabellerUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    private static final Token GROUP_ID1 = new Token(1L, 11L);
    private static final int TAB_ID1 = 1;

    @Mock private Profile mProfile;
    @Mock private TabListNotificationHandler mTabListNotificationHandler;
    @Mock private MessagingBackendService mMessagingBackendService;
    @Mock private TabGroupModelFilter mTabGroupModelFilter;

    @Captor private ArgumentCaptor<PersistentMessageObserver> mPersistentMessageObserverCaptor;
    @Captor private ArgumentCaptor<Map<Integer, TabCardLabelData>> mLabelDataCaptor;

    private final ObservableSupplierImpl<TabGroupModelFilter> mTabGroupModelFilterSupplier =
            new ObservableSupplierImpl<>();

    private Context mContext;
    private TabGroupLabeller mTabGroupLabeller;

    @Before
    public void setUp() {
        MessagingBackendServiceFactory.setForTesting(mMessagingBackendService);
        mContext = ApplicationProvider.getApplicationContext();
        mTabGroupModelFilterSupplier.set(mTabGroupModelFilter);
        when(mTabGroupModelFilter.getRootIdFromStableId(GROUP_ID1)).thenReturn(TAB_ID1);
        mTabGroupLabeller =
                new TabGroupLabeller(
                        mProfile, mTabListNotificationHandler, mTabGroupModelFilterSupplier);
    }

    private PersistentMessage makeStandardMessage() {
        PersistentMessage message = new PersistentMessage();
        message.type = PersistentNotificationType.DIRTY_TAB_GROUP;
        message.attribution = new MessageAttribution();
        message.attribution.tabGroupMetadata = new TabGroupMessageMetadata();
        message.attribution.tabGroupMetadata.localTabGroupId = new LocalTabGroupId(GROUP_ID1);
        return message;
    }

    private void assertContainsLabel(Map<Integer, TabCardLabelData> labelDataMap) {
        assertTrue(labelDataMap.containsKey(TAB_ID1));
        TabCardLabelData labelData = labelDataMap.get(TAB_ID1);
        assertEquals(TabCardLabelType.ACTIVITY_UPDATE, labelData.labelType);
        labelData.contentDescriptionResolver.resolve(mContext);
        assertEquals("New activity", labelData.textResolver.resolve(mContext));
    }

    private void assertContainsNullLabel(Map<Integer, TabCardLabelData> labelDataMap) {
        assertTrue(labelDataMap.containsKey(TAB_ID1));
        TabCardLabelData labelData = labelDataMap.get(TAB_ID1);
        assertNull(labelData);
    }

    @Test
    public void testDestroy() {
        verify(mMessagingBackendService)
                .addPersistentMessageObserver(mPersistentMessageObserverCaptor.capture());
        mTabGroupLabeller.destroy();
        verify(mMessagingBackendService)
                .removePersistentMessageObserver(mPersistentMessageObserverCaptor.getValue());
    }

    @Test
    public void testShowAll_Added() {
        List<PersistentMessage> messageList = List.of(makeStandardMessage());
        when(mMessagingBackendService.getMessages(any())).thenReturn(messageList);

        mTabGroupLabeller.showAll();

        verify(mTabListNotificationHandler).updateTabCardLabels(mLabelDataCaptor.capture());
        assertContainsLabel(mLabelDataCaptor.getValue());
    }

    @Test
    public void testShowAll_WrongTabModel() {
        when(mTabGroupModelFilter.getRootIdFromStableId(any())).thenReturn(Tab.INVALID_TAB_ID);
        List<PersistentMessage> messageList = List.of(makeStandardMessage());
        when(mMessagingBackendService.getMessages(any())).thenReturn(messageList);

        mTabGroupLabeller.showAll();
        verify(mTabListNotificationHandler, never()).updateTabCardLabels(any());
    }

    @Test
    public void testShowAll_NullAttributionTabGroupId() {
        PersistentMessage message = makeStandardMessage();
        message.attribution.tabGroupMetadata.localTabGroupId = null;
        List<PersistentMessage> messageList = List.of(message);
        when(mMessagingBackendService.getMessages(any())).thenReturn(messageList);

        mTabGroupLabeller.showAll();
        verify(mTabListNotificationHandler, never()).updateTabCardLabels(any());
    }

    @Test
    public void testShowAll_WrongType() {
        PersistentMessage message = makeStandardMessage();
        message.type = PersistentNotificationType.DIRTY_TAB;
        List<PersistentMessage> messageList = List.of(message);
        when(mMessagingBackendService.getMessages(any())).thenReturn(messageList);

        mTabGroupLabeller.showAll();
        verify(mTabListNotificationHandler, never()).updateTabCardLabels(any());
    }

    @Test
    public void testShowAll_OffTheRecord() {
        when(mTabGroupModelFilter.isOffTheRecord()).thenReturn(true);
        List<PersistentMessage> messageList = List.of(makeStandardMessage());
        when(mMessagingBackendService.getMessages(any())).thenReturn(messageList);

        mTabGroupLabeller.showAll();
        verify(mTabListNotificationHandler, never()).updateTabCardLabels(any());
    }

    @Test
    public void testShowAll_NullFilter() {
        mTabGroupModelFilterSupplier.set(null);
        List<PersistentMessage> messageList = List.of(makeStandardMessage());
        when(mMessagingBackendService.getMessages(any())).thenReturn(messageList);

        mTabGroupLabeller.showAll();
        verify(mTabListNotificationHandler, never()).updateTabCardLabels(any());
    }

    @Test
    public void testDisplayPersistentMessage() {
        verify(mMessagingBackendService)
                .addPersistentMessageObserver(mPersistentMessageObserverCaptor.capture());
        mPersistentMessageObserverCaptor.getValue().displayPersistentMessage(makeStandardMessage());

        verify(mTabListNotificationHandler).updateTabCardLabels(mLabelDataCaptor.capture());
        assertContainsLabel(mLabelDataCaptor.getValue());
    }

    @Test
    public void testHidePersistentMessage() {
        verify(mMessagingBackendService)
                .addPersistentMessageObserver(mPersistentMessageObserverCaptor.capture());
        mPersistentMessageObserverCaptor.getValue().hidePersistentMessage(makeStandardMessage());

        verify(mTabListNotificationHandler).updateTabCardLabels(mLabelDataCaptor.capture());
        assertContainsNullLabel(mLabelDataCaptor.getValue());
    }

    @Test
    public void testOnMessagingBackendServiceInitialized() {
        List<PersistentMessage> messageList = List.of(makeStandardMessage());
        when(mMessagingBackendService.getMessages(any())).thenReturn(messageList);

        verify(mMessagingBackendService)
                .addPersistentMessageObserver(mPersistentMessageObserverCaptor.capture());
        mPersistentMessageObserverCaptor.getValue().onMessagingBackendServiceInitialized();

        verify(mTabListNotificationHandler).updateTabCardLabels(mLabelDataCaptor.capture());
        assertContainsLabel(mLabelDataCaptor.getValue());
    }
}
