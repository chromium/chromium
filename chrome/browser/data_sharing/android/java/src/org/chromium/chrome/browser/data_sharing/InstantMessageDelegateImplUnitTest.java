// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.components.messages.MessageBannerProperties.MESSAGE_IDENTIFIER;
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
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_group_sync.messaging.MessagingBackendServiceFactory;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.components.data_sharing.SharedGroupTestHelper;
import org.chromium.components.messages.ManagedMessageDispatcher;
import org.chromium.components.messages.MessageIdentifier;
import org.chromium.components.messages.MessagesFactory;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.messaging.InstantMessage;
import org.chromium.components.tab_group_sync.messaging.InstantNotificationLevel;
import org.chromium.components.tab_group_sync.messaging.MessageAttribution;
import org.chromium.components.tab_group_sync.messaging.MessagingBackendService;
import org.chromium.components.tab_group_sync.messaging.UserAction;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.ref.WeakReference;

/** Unit tests for {@link InstantMessageDelegateImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
public class InstantMessageDelegateImplUnitTest {
    private static final Token TAB_GROUP_ID = new Token(1L, 2L);
    private static final int TAB_ID = 1;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private MessagingBackendService mMessagingBackendService;
    @Mock private ManagedMessageDispatcher mManagedMessageDispatcher;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private TabGroupModelFilter mTabGroupModelFilter;
    @Mock private Callback<Boolean> mSuccessCallback;

    @Captor private ArgumentCaptor<PropertyModel> mPropertyModelCaptor;

    private InstantMessageDelegateImpl mDelegate;

    @Before
    public void setUp() {
        MessagingBackendServiceFactory.setForTesting(mMessagingBackendService);
        when(mWindowAndroid.getUnownedUserDataHost()).thenReturn(new UnownedUserDataHost());
        MessagesFactory.attachMessageDispatcher(mWindowAndroid, mManagedMessageDispatcher);
        when(mWindowAndroid.getContext())
                .thenReturn(new WeakReference<>(ApplicationProvider.getApplicationContext()));
        when(mTabGroupModelFilter.getRootIdFromStableId(TAB_GROUP_ID)).thenReturn(TAB_ID);

        mDelegate = new InstantMessageDelegateImpl(mProfile);
        mDelegate.attachWindow(mWindowAndroid, mTabGroupModelFilter);
    }

    private InstantMessage newInstantMessage(@UserAction int action) {
        MessageAttribution attribution = new MessageAttribution();
        attribution.localTabGroupId = new LocalTabGroupId(TAB_GROUP_ID);
        attribution.triggeringUser = SharedGroupTestHelper.GROUP_MEMBER1;
        InstantMessage instantMessage = new InstantMessage();
        instantMessage.attribution = attribution;
        instantMessage.action = action;
        instantMessage.level = InstantNotificationLevel.BROWSER;
        return instantMessage;
    }

    @Test
    public void testDisplayInstantaneousMessage_NotAttached() {
        mDelegate.detachWindow(mWindowAndroid);
        mDelegate.displayInstantaneousMessage(
                newInstantMessage(UserAction.TAB_REMOVED), mSuccessCallback);
        verify(mManagedMessageDispatcher, never()).enqueueWindowScopedMessage(any(), anyBoolean());
        verify(mSuccessCallback).onResult(false);
    }

    @Test
    public void testDisplayInstantaneousMessage_NotInTabModel() {
        when(mTabGroupModelFilter.getRootIdFromStableId(TAB_GROUP_ID))
                .thenReturn(Tab.INVALID_TAB_ID);
        mDelegate.displayInstantaneousMessage(
                newInstantMessage(UserAction.TAB_REMOVED), mSuccessCallback);
        verify(mManagedMessageDispatcher, never()).enqueueWindowScopedMessage(any(), anyBoolean());
        verify(mSuccessCallback).onResult(false);
    }

    @Test
    public void testTabRemoved() {
        mDelegate.displayInstantaneousMessage(
                newInstantMessage(UserAction.TAB_REMOVED), mSuccessCallback);

        verify(mManagedMessageDispatcher)
                .enqueueWindowScopedMessage(mPropertyModelCaptor.capture(), anyBoolean());
        verify(mSuccessCallback).onResult(true);
        PropertyModel propertyModel = mPropertyModelCaptor.getValue();
        @MessageIdentifier int messageIdentifier = propertyModel.get(MESSAGE_IDENTIFIER);
        assertEquals(MessageIdentifier.TAB_REMOVED_THROUGH_COLLABORATION, messageIdentifier);
        String title = propertyModel.get(TITLE);
        assertTrue(title.contains(SharedGroupTestHelper.GIVEN_NAME1));
    }

    @Test
    public void testTabNavigated() {
        mDelegate.displayInstantaneousMessage(
                newInstantMessage(UserAction.TAB_NAVIGATED), mSuccessCallback);

        verify(mManagedMessageDispatcher)
                .enqueueWindowScopedMessage(mPropertyModelCaptor.capture(), anyBoolean());
        verify(mSuccessCallback).onResult(true);
        PropertyModel propertyModel = mPropertyModelCaptor.getValue();
        @MessageIdentifier int messageIdentifier = propertyModel.get(MESSAGE_IDENTIFIER);
        assertEquals(MessageIdentifier.TAB_NAVIGATED_THROUGH_COLLABORATION, messageIdentifier);
        String title = propertyModel.get(TITLE);
        assertTrue(title.contains(SharedGroupTestHelper.GIVEN_NAME1));
    }

    @Test
    public void testCollaborationUserJoined() {
        mDelegate.displayInstantaneousMessage(
                newInstantMessage(UserAction.COLLABORATION_USER_JOINED), mSuccessCallback);

        verify(mManagedMessageDispatcher)
                .enqueueWindowScopedMessage(mPropertyModelCaptor.capture(), anyBoolean());
        verify(mSuccessCallback).onResult(true);
        PropertyModel propertyModel = mPropertyModelCaptor.getValue();
        @MessageIdentifier int messageIdentifier = propertyModel.get(MESSAGE_IDENTIFIER);
        assertEquals(MessageIdentifier.COLLABORATION_USER_JOINED, messageIdentifier);
        String title = propertyModel.get(TITLE);
        assertTrue(title.contains(SharedGroupTestHelper.GIVEN_NAME1));
    }

    @Test
    public void testCollaborationRemoved() {
        mDelegate.displayInstantaneousMessage(
                newInstantMessage(UserAction.COLLABORATION_REMOVED), mSuccessCallback);

        verify(mManagedMessageDispatcher)
                .enqueueWindowScopedMessage(mPropertyModelCaptor.capture(), anyBoolean());
        verify(mSuccessCallback).onResult(true);
        PropertyModel propertyModel = mPropertyModelCaptor.getValue();
        @MessageIdentifier int messageIdentifier = propertyModel.get(MESSAGE_IDENTIFIER);
        assertEquals(MessageIdentifier.COLLABORATION_REMOVED, messageIdentifier);
    }
}
