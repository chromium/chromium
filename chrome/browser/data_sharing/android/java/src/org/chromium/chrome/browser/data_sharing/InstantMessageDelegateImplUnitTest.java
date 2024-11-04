// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.components.messages.MessageBannerProperties.MESSAGE_IDENTIFIER;
import static org.chromium.components.messages.MessageBannerProperties.ON_FULLY_VISIBLE;
import static org.chromium.components.messages.MessageBannerProperties.TITLE;

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

import org.chromium.base.Callback;
import org.chromium.base.Token;
import org.chromium.base.UnownedUserDataHost;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.collaboration.messaging.MessagingBackendServiceFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.components.collaboration.messaging.CollaborationEvent;
import org.chromium.components.collaboration.messaging.InstantMessage;
import org.chromium.components.collaboration.messaging.InstantNotificationLevel;
import org.chromium.components.collaboration.messaging.MessageAttribution;
import org.chromium.components.collaboration.messaging.MessagingBackendService;
import org.chromium.components.collaboration.messaging.TabGroupMessageMetadata;
import org.chromium.components.collaboration.messaging.TabMessageMetadata;
import org.chromium.components.data_sharing.SharedGroupTestHelper;
import org.chromium.components.messages.ManagedMessageDispatcher;
import org.chromium.components.messages.MessageIdentifier;
import org.chromium.components.messages.MessagesFactory;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.MockitoHelper;

import java.lang.ref.WeakReference;

/** Unit tests for {@link InstantMessageDelegateImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
public class InstantMessageDelegateImplUnitTest {
    private static final Token TAB_GROUP_ID = new Token(1L, 2L);
    private static final int TAB_ID = 1;
    private static final String TAB_TITLE = "Tab Title";
    private static final String TAB_GROUP_TITLE = "Group Title";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private MessagingBackendService mMessagingBackendService;
    @Mock private ManagedMessageDispatcher mManagedMessageDispatcher;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private TabGroupModelFilter mTabGroupModelFilter;
    @Mock private Callback<Boolean> mSuccessCallback;
    @Mock private DataSharingNotificationManager mDataSharingNotificationManager;

    @Captor private ArgumentCaptor<PropertyModel> mPropertyModelCaptor;

    private InstantMessageDelegateImpl mDelegate;

    @Before
    public void setUp() {
        MockitoHelper.forwardBind(mSuccessCallback);
        MessagingBackendServiceFactory.setForTesting(mMessagingBackendService);
        when(mWindowAndroid.getUnownedUserDataHost()).thenReturn(new UnownedUserDataHost());
        MessagesFactory.attachMessageDispatcher(mWindowAndroid, mManagedMessageDispatcher);
        when(mWindowAndroid.getContext())
                .thenReturn(new WeakReference<>(ApplicationProvider.getApplicationContext()));
        when(mTabGroupModelFilter.getRootIdFromStableId(TAB_GROUP_ID)).thenReturn(TAB_ID);

        mDelegate = new InstantMessageDelegateImpl(mProfile);
        mDelegate.attachWindow(
                mWindowAndroid, mTabGroupModelFilter, mDataSharingNotificationManager);
    }

    private InstantMessage newInstantMessage(@CollaborationEvent int collaborationEvent) {
        MessageAttribution attribution = new MessageAttribution();
        attribution.tabMetadata = new TabMessageMetadata();
        attribution.tabMetadata.lastKnownTitle = TAB_TITLE;
        attribution.tabGroupMetadata = new TabGroupMessageMetadata();
        attribution.tabGroupMetadata.lastKnownTitle = TAB_GROUP_TITLE;
        attribution.tabGroupMetadata.localTabGroupId = new LocalTabGroupId(TAB_GROUP_ID);
        attribution.triggeringUser = SharedGroupTestHelper.GROUP_MEMBER1;
        InstantMessage instantMessage = new InstantMessage();
        instantMessage.attribution = attribution;
        instantMessage.collaborationEvent = collaborationEvent;
        instantMessage.level = InstantNotificationLevel.BROWSER;
        return instantMessage;
    }

    @Test
    public void testDisplayInstantaneousMessage_NotAttached() {
        mDelegate.detachWindow(mWindowAndroid);
        mDelegate.displayInstantaneousMessage(
                newInstantMessage(CollaborationEvent.TAB_REMOVED), mSuccessCallback);
        verify(mManagedMessageDispatcher, never()).enqueueWindowScopedMessage(any(), anyBoolean());
        verify(mSuccessCallback).onResult(false);
    }

    @Test
    public void testDisplayInstantaneousMessage_NotInTabModel() {
        when(mTabGroupModelFilter.getRootIdFromStableId(TAB_GROUP_ID))
                .thenReturn(Tab.INVALID_TAB_ID);
        mDelegate.displayInstantaneousMessage(
                newInstantMessage(CollaborationEvent.TAB_REMOVED), mSuccessCallback);
        verify(mManagedMessageDispatcher, never()).enqueueWindowScopedMessage(any(), anyBoolean());
        verify(mSuccessCallback).onResult(false);
    }

    @Test
    public void testTabRemoved() {
        mDelegate.displayInstantaneousMessage(
                newInstantMessage(CollaborationEvent.TAB_REMOVED), mSuccessCallback);

        verify(mManagedMessageDispatcher)
                .enqueueWindowScopedMessage(mPropertyModelCaptor.capture(), anyBoolean());
        PropertyModel propertyModel = mPropertyModelCaptor.getValue();
        @MessageIdentifier int messageIdentifier = propertyModel.get(MESSAGE_IDENTIFIER);
        assertEquals(MessageIdentifier.TAB_REMOVED_THROUGH_COLLABORATION, messageIdentifier);
        String title = propertyModel.get(TITLE);
        assertTrue(title.contains(SharedGroupTestHelper.GIVEN_NAME1));
        assertTrue(title.contains(TAB_TITLE));

        propertyModel.get(ON_FULLY_VISIBLE).onResult(true);
        verify(mSuccessCallback).onResult(true);
    }

    @Test
    public void testTabNavigated() {
        mDelegate.displayInstantaneousMessage(
                newInstantMessage(CollaborationEvent.TAB_UPDATED), mSuccessCallback);

        verify(mManagedMessageDispatcher)
                .enqueueWindowScopedMessage(mPropertyModelCaptor.capture(), anyBoolean());
        PropertyModel propertyModel = mPropertyModelCaptor.getValue();
        @MessageIdentifier int messageIdentifier = propertyModel.get(MESSAGE_IDENTIFIER);
        assertEquals(MessageIdentifier.TAB_NAVIGATED_THROUGH_COLLABORATION, messageIdentifier);
        String title = propertyModel.get(TITLE);
        assertTrue(title.contains(SharedGroupTestHelper.GIVEN_NAME1));
        assertTrue(title.contains(TAB_TITLE));

        verify(mSuccessCallback, never()).onResult(anyBoolean());

        propertyModel.get(ON_FULLY_VISIBLE).onResult(true);
        verify(mSuccessCallback).onResult(true);

        // When it stops being visible, success shouldn't change.
        propertyModel.get(ON_FULLY_VISIBLE).onResult(false);
        verify(mSuccessCallback, times(1)).onResult(anyBoolean());
    }

    @Test
    public void testCollaborationMemberAdded() {
        mDelegate.displayInstantaneousMessage(
                newInstantMessage(CollaborationEvent.COLLABORATION_MEMBER_ADDED), mSuccessCallback);

        verify(mManagedMessageDispatcher)
                .enqueueWindowScopedMessage(mPropertyModelCaptor.capture(), anyBoolean());
        PropertyModel propertyModel = mPropertyModelCaptor.getValue();
        @MessageIdentifier int messageIdentifier = propertyModel.get(MESSAGE_IDENTIFIER);
        assertEquals(MessageIdentifier.COLLABORATION_MEMBER_ADDED, messageIdentifier);
        String title = propertyModel.get(TITLE);
        assertTrue(title.contains(SharedGroupTestHelper.GIVEN_NAME1));
        assertTrue(title.contains(TAB_GROUP_TITLE));

        propertyModel.get(ON_FULLY_VISIBLE).onResult(true);
        verify(mSuccessCallback).onResult(true);
    }

    @Test
    public void testCollaborationMemberAdded_FallbackTitle() {
        when(mTabGroupModelFilter.getRootIdFromStableId(any())).thenReturn(TAB_ID);
        when(mTabGroupModelFilter.getRelatedTabCountForRootId(anyInt())).thenReturn(13);
        InstantMessage message = newInstantMessage(CollaborationEvent.COLLABORATION_MEMBER_ADDED);
        message.attribution.tabGroupMetadata.lastKnownTitle = "";
        mDelegate.displayInstantaneousMessage(message, mSuccessCallback);

        verify(mManagedMessageDispatcher)
                .enqueueWindowScopedMessage(mPropertyModelCaptor.capture(), anyBoolean());
        PropertyModel propertyModel = mPropertyModelCaptor.getValue();
        @MessageIdentifier int messageIdentifier = propertyModel.get(MESSAGE_IDENTIFIER);
        assertEquals(MessageIdentifier.COLLABORATION_MEMBER_ADDED, messageIdentifier);
        String title = propertyModel.get(TITLE);
        assertTrue(title.contains(SharedGroupTestHelper.GIVEN_NAME1));
        assertTrue(title.contains(Integer.toString(13)));

        propertyModel.get(ON_FULLY_VISIBLE).onResult(true);
        verify(mSuccessCallback).onResult(true);
    }

    @Test
    public void testCollaborationRemoved() {
        mDelegate.displayInstantaneousMessage(
                newInstantMessage(CollaborationEvent.COLLABORATION_REMOVED), mSuccessCallback);

        verify(mManagedMessageDispatcher)
                .enqueueWindowScopedMessage(mPropertyModelCaptor.capture(), anyBoolean());
        PropertyModel propertyModel = mPropertyModelCaptor.getValue();
        @MessageIdentifier int messageIdentifier = propertyModel.get(MESSAGE_IDENTIFIER);
        assertEquals(MessageIdentifier.COLLABORATION_REMOVED, messageIdentifier);
        String title = propertyModel.get(TITLE);
        assertTrue(title.contains(TAB_GROUP_TITLE));

        propertyModel.get(ON_FULLY_VISIBLE).onResult(true);
        verify(mSuccessCallback).onResult(true);
    }

    @Test
    public void testCollaborationRemoved_FallbackTitle() {
        when(mTabGroupModelFilter.getRootIdFromStableId(any())).thenReturn(TAB_ID);
        when(mTabGroupModelFilter.getRelatedTabCountForRootId(anyInt())).thenReturn(13);
        InstantMessage message = newInstantMessage(CollaborationEvent.COLLABORATION_REMOVED);
        message.attribution.tabGroupMetadata.lastKnownTitle = "";
        mDelegate.displayInstantaneousMessage(message, mSuccessCallback);

        verify(mManagedMessageDispatcher)
                .enqueueWindowScopedMessage(mPropertyModelCaptor.capture(), anyBoolean());
        PropertyModel propertyModel = mPropertyModelCaptor.getValue();
        @MessageIdentifier int messageIdentifier = propertyModel.get(MESSAGE_IDENTIFIER);
        assertEquals(MessageIdentifier.COLLABORATION_REMOVED, messageIdentifier);
        String title = propertyModel.get(TITLE);
        assertTrue(title.contains(Integer.toString(13)));

        propertyModel.get(ON_FULLY_VISIBLE).onResult(true);
        verify(mSuccessCallback).onResult(true);
    }

    @Test
    public void testSystemNotification() {
        InstantMessage message = newInstantMessage(CollaborationEvent.COLLABORATION_MEMBER_ADDED);
        message.level = InstantNotificationLevel.SYSTEM;

        mDelegate.displayInstantaneousMessage(message, mSuccessCallback);

        verify(mDataSharingNotificationManager).showOtherJoinedNotification(any(), any());
        verify(mSuccessCallback).onResult(true);
    }
}
