// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.tasks.tab_management.TabGroupListCoordinator.RowType.TAB_GROUP_REMOVED;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.ModelType.MESSAGE;
import static org.chromium.chrome.browser.tasks.tab_management.TabSwitcherMessageManager.MessageType.ALL;

import android.content.Context;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tasks.tab_management.MessageCardView.ActionProvider;
import org.chromium.chrome.browser.tasks.tab_management.MessageCardView.ServiceDismissActionProvider;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcherMessageManager.MessageType;
import org.chromium.components.collaboration.messaging.CollaborationEvent;
import org.chromium.components.collaboration.messaging.MessageAttribution;
import org.chromium.components.collaboration.messaging.MessagingBackendService;
import org.chromium.components.collaboration.messaging.PersistentMessage;
import org.chromium.components.collaboration.messaging.PersistentNotificationType;
import org.chromium.components.collaboration.messaging.TabGroupMessageMetadata;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.Collections;
import java.util.List;

/** Unit tests for {@link TabGroupRemovedMessageMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabGroupRemovedMessageMediatorUnitTest {
    private static final @MessageType int DISMISS_MSG_TYPE = ALL;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private MessagingBackendService mMessagingBackendService;

    private Context mContext;
    private ModelList mModelList;
    private TabGroupRemovedMessageMediator mMediator;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        mModelList = new ModelList();
        mMediator =
                new TabGroupRemovedMessageMediator(mContext, mMessagingBackendService, mModelList);
    }

    @Test
    public void testQueueMessageIfNeeded_noMessages() {
        when(mMessagingBackendService.getMessages(PersistentNotificationType.TOMBSTONED))
                .thenReturn(Collections.emptyList());

        mMediator.queueMessageIfNeeded();

        assertTrue(mModelList.isEmpty());
    }

    @Test
    public void testQueueMessageIfNeeded_irrelevantMessages() {
        PersistentMessage wrongTypeMessage =
                createMessage("id1", "title1", CollaborationEvent.TAB_ADDED);
        PersistentMessage invalidIdMessage =
                createMessage(null, "title2", CollaborationEvent.TAB_GROUP_REMOVED);

        when(mMessagingBackendService.getMessages(PersistentNotificationType.TOMBSTONED))
                .thenReturn(List.of(wrongTypeMessage, invalidIdMessage));

        mMediator.queueMessageIfNeeded();

        assertTrue(mModelList.isEmpty());
    }

    @Test
    public void testQueueMessageIfNeeded_oneMessage() {
        String title = "My Awesome Group";
        PersistentMessage message1 =
                createMessage("id1", title, CollaborationEvent.TAB_GROUP_REMOVED);
        when(mMessagingBackendService.getMessages(PersistentNotificationType.TOMBSTONED))
                .thenReturn(List.of(message1));

        mMediator.queueMessageIfNeeded();

        assertEquals(1, mModelList.size());
        ListItem listItem = mModelList.get(0);
        PropertyModel model = listItem.model;
        assertEquals(MESSAGE, model.get(TabListModel.CardProperties.CARD_TYPE));
        assertEquals(TAB_GROUP_REMOVED, model.get(MessageCardViewProperties.MESSAGE_TYPE));
        String expectedText =
                mContext.getString(R.string.one_tab_group_removed_message_card_description, title);
        assertEquals(expectedText, model.get(MessageCardViewProperties.DESCRIPTION_TEXT));
        assertFalse(model.get(MessageCardViewProperties.ACTION_BUTTON_VISIBLE));
    }

    @Test
    public void testQueueMessageIfNeeded_twoMessages() {
        String title1 = "Work";
        String title2 = "Travel";
        PersistentMessage message1 =
                createMessage("id1", title1, CollaborationEvent.TAB_GROUP_REMOVED);
        PersistentMessage message2 =
                createMessage("id2", title2, CollaborationEvent.TAB_GROUP_REMOVED);
        when(mMessagingBackendService.getMessages(PersistentNotificationType.TOMBSTONED))
                .thenReturn(List.of(message1, message2));

        mMediator.queueMessageIfNeeded();

        assertEquals(1, mModelList.size());
        String expectedText =
                mContext.getString(
                        R.string.two_tab_groups_removed_message_card_description, title1, title2);
        assertEquals(
                expectedText,
                mModelList.get(0).model.get(MessageCardViewProperties.DESCRIPTION_TEXT));
    }

    @Test
    public void testQueueMessageIfNeeded_threeMessages() {
        PersistentMessage message1 =
                createMessage("id1", "t1", CollaborationEvent.TAB_GROUP_REMOVED);
        PersistentMessage message2 =
                createMessage("id2", "t2", CollaborationEvent.TAB_GROUP_REMOVED);
        PersistentMessage message3 =
                createMessage("id3", "t3", CollaborationEvent.TAB_GROUP_REMOVED);

        when(mMessagingBackendService.getMessages(PersistentNotificationType.TOMBSTONED))
                .thenReturn(List.of(message1, message2, message3));

        mMediator.queueMessageIfNeeded();

        assertEquals(1, mModelList.size());
        String expectedText =
                mContext.getResources()
                        .getQuantityString(
                                R.plurals.generic_tab_groups_removed_message_card_description,
                                3,
                                3);
        assertEquals(
                expectedText,
                mModelList.get(0).model.get(MessageCardViewProperties.DESCRIPTION_TEXT));
    }

    @Test
    public void testQueueMessageIfNeeded_missingTitles() {
        String title1 = "title";
        PersistentMessage message1 =
                createMessage("id1", title1, CollaborationEvent.TAB_GROUP_REMOVED);
        PersistentMessage message2 = createMessage("id2", "", CollaborationEvent.TAB_GROUP_REMOVED);
        when(mMessagingBackendService.getMessages(PersistentNotificationType.TOMBSTONED))
                .thenReturn(List.of(message1, message2));

        mMediator.queueMessageIfNeeded();
        assertEquals(1, mModelList.size());

        String expectedText =
                mContext.getResources()
                        .getQuantityString(
                                R.plurals.generic_tab_groups_removed_message_card_description,
                                2,
                                2);
        assertEquals(
                expectedText,
                mModelList.get(0).model.get(MessageCardViewProperties.DESCRIPTION_TEXT));
    }

    @Test
    public void testReviewActionProvider() {
        String id1 = "id1";
        String id2 = "id2";
        PersistentMessage message1 =
                createMessage(id1, "title1", CollaborationEvent.TAB_GROUP_REMOVED);
        PersistentMessage message2 =
                createMessage(id2, "title2", CollaborationEvent.TAB_GROUP_REMOVED);
        when(mMessagingBackendService.getMessages(PersistentNotificationType.TOMBSTONED))
                .thenReturn(List.of(message1, message2));

        mMediator.queueMessageIfNeeded();
        assertEquals(1, mModelList.size());

        PropertyModel model = mModelList.get(0).model;
        ActionProvider reviewProvider =
                model.get(MessageCardViewProperties.UI_DISMISS_ACTION_PROVIDER);
        reviewProvider.action();

        assertTrue(mModelList.isEmpty());

        verify(mMessagingBackendService, times(1))
                .clearPersistentMessage(eq(id1), eq(PersistentNotificationType.TOMBSTONED));
        verify(mMessagingBackendService, times(1))
                .clearPersistentMessage(eq(id2), eq(PersistentNotificationType.TOMBSTONED));
    }

    @Test
    public void testDismissActionProvider_onEmptyList() {
        assertTrue(mModelList.isEmpty());

        ServiceDismissActionProvider dismissProvider = ignored -> mMediator.removeMessageCard();
        dismissProvider.dismiss(DISMISS_MSG_TYPE);
    }

    @Test
    public void testDismissActionProvider_wrongCardType() {
        PropertyModel wrongTypeModel =
                new PropertyModel.Builder(MessageCardViewProperties.ALL_KEYS)
                        .with(TabListModel.CardProperties.CARD_TYPE, MESSAGE)
                        .with(MessageCardViewProperties.MESSAGE_TYPE, 99) // Not TAB_GROUP_REMOVED
                        .build();
        mModelList.add(new ListItem(TAB_GROUP_REMOVED, wrongTypeModel));

        ServiceDismissActionProvider dismissProvider = ignored -> mMediator.removeMessageCard();
        dismissProvider.dismiss(DISMISS_MSG_TYPE);

        assertEquals(1, mModelList.size());
    }

    private PersistentMessage createMessage(
            String id, String title, @CollaborationEvent int eventType) {
        TabGroupMessageMetadata metadata = new TabGroupMessageMetadata();
        metadata.lastKnownTitle = title;

        MessageAttribution attribution = new MessageAttribution();
        attribution.id = id;
        attribution.tabGroupMetadata = metadata;

        PersistentMessage message = new PersistentMessage();
        message.collaborationEvent = eventType;
        message.attribution = attribution;
        return message;
    }
}
