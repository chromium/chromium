// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.components.data_sharing.SharedGroupTestHelper.COLLABORATION_ID1;
import static org.chromium.components.data_sharing.SharedGroupTestHelper.GIVEN_NAME1;
import static org.chromium.components.data_sharing.SharedGroupTestHelper.GROUP_MEMBER1;
import static org.chromium.components.messages.MessageBannerProperties.MESSAGE_IDENTIFIER;
import static org.chromium.components.messages.MessageBannerProperties.ON_FULLY_VISIBLE;
import static org.chromium.components.messages.MessageBannerProperties.ON_PRIMARY_ACTION;
import static org.chromium.components.messages.MessageBannerProperties.TITLE;
import static org.chromium.components.messages.PrimaryActionClickBehavior.DISMISS_IMMEDIATELY;
import static org.chromium.components.tab_group_sync.SyncedGroupTestHelper.SYNC_GROUP_ID1;

import android.app.Activity;
import android.graphics.Bitmap;
import android.text.TextUtils;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.ArgumentMatcher;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.Token;
import org.chromium.base.UnownedUserDataHost;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.components.collaboration.messaging.CollaborationEvent;
import org.chromium.components.collaboration.messaging.InstantMessage;
import org.chromium.components.collaboration.messaging.InstantNotificationLevel;
import org.chromium.components.collaboration.messaging.MessageAttribution;
import org.chromium.components.collaboration.messaging.MessagingBackendService;
import org.chromium.components.collaboration.messaging.TabGroupMessageMetadata;
import org.chromium.components.collaboration.messaging.TabMessageMetadata;
import org.chromium.components.data_sharing.DataSharingService;
import org.chromium.components.data_sharing.DataSharingUIDelegate;
import org.chromium.components.data_sharing.configs.DataSharingAvatarBitmapConfig;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.messages.ManagedMessageDispatcher;
import org.chromium.components.messages.MessageIdentifier;
import org.chromium.components.messages.MessagesFactory;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SyncedGroupTestHelper;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.MockitoHelper;
import org.chromium.url.JUnitTestGURLs;

import java.lang.ref.WeakReference;
import java.util.Arrays;

/** Unit tests for {@link InstantMessageDelegateImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
public class InstantMessageDelegateImplUnitTest {
    private static final Token TAB_GROUP_ID = new Token(1L, 2L);
    private static final int TAB_ID = 1;
    private static final String TAB_TITLE = "Tab Title";
    private static final String TAB_GROUP_TITLE = "Group Title";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private MessagingBackendService mMessagingBackendService;
    @Mock private DataSharingService mDataSharingService;
    @Mock private DataSharingUIDelegate mDataSharingUiDelegate;
    @Mock private ManagedMessageDispatcher mManagedMessageDispatcher;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private TabGroupModelFilter mTabGroupModelFilter;
    @Mock private TabModel mTabModel;
    @Mock private TabCreator mTabCreator;
    @Mock private Callback<Boolean> mSuccessCallback;
    @Mock private DataSharingNotificationManager mDataSharingNotificationManager;
    @Mock private DataSharingTabManager mDataSharingTabManager;
    @Mock private TabGroupSyncService mTabGroupSyncService;
    @Mock private Bitmap mAvatarBitmap;
    @Mock private Tab mTab1;
    @Mock private Tab mTab2;

    @Captor private ArgumentCaptor<PropertyModel> mPropertyModelCaptor;

    private final UnownedUserDataHost mUnownedUserDataHost = new UnownedUserDataHost();

    private SyncedGroupTestHelper mSyncedGroupTestHelper;
    private InstantMessageDelegateImpl mDelegate;

    @Before
    public void setUp() {
        MockitoHelper.forwardBind(mSuccessCallback);
        mActivityScenarioRule.getScenario().onActivity(this::onActivity);
    }

    private void onActivity(Activity activity) {
        when(mDataSharingService.getUiDelegate()).thenReturn(mDataSharingUiDelegate);
        MockitoHelper.doCallback(
                        (DataSharingAvatarBitmapConfig config) ->
                                config.getDataSharingAvatarCallback().onAvatarLoaded(mAvatarBitmap))
                .when(mDataSharingUiDelegate)
                .getAvatarBitmap(any());

        when(mWindowAndroid.getUnownedUserDataHost()).thenReturn(mUnownedUserDataHost);
        MessagesFactory.attachMessageDispatcher(mWindowAndroid, mManagedMessageDispatcher);

        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<>(activity));
        when(mTabGroupModelFilter.getRootIdFromStableId(TAB_GROUP_ID)).thenReturn(TAB_ID);
        when(mTabGroupModelFilter.getTabModel()).thenReturn(mTabModel);
        when(mTabModel.getTabCreator()).thenReturn(mTabCreator);

        mSyncedGroupTestHelper = new SyncedGroupTestHelper(mTabGroupSyncService);
        mSyncedGroupTestHelper.newTabGroup(SYNC_GROUP_ID1, TAB_GROUP_ID);

        mDelegate =
                new InstantMessageDelegateImpl(
                        mMessagingBackendService, mDataSharingService, mTabGroupSyncService);
        mDelegate.attachWindow(
                mWindowAndroid,
                mTabGroupModelFilter,
                mDataSharingNotificationManager,
                mDataSharingTabManager);
    }

    private InstantMessage newInstantMessage(@CollaborationEvent int collaborationEvent) {
        MessageAttribution attribution = new MessageAttribution();
        attribution.tabMetadata = new TabMessageMetadata();
        attribution.tabMetadata.lastKnownTitle = TAB_TITLE;
        attribution.tabMetadata.lastKnownUrl = JUnitTestGURLs.URL_1.getSpec();
        attribution.tabGroupMetadata = new TabGroupMessageMetadata();
        attribution.tabGroupMetadata.lastKnownTitle = TAB_GROUP_TITLE;
        attribution.tabGroupMetadata.localTabGroupId = new LocalTabGroupId(TAB_GROUP_ID);
        attribution.tabGroupMetadata.syncTabGroupId = SYNC_GROUP_ID1;
        attribution.triggeringUser = GROUP_MEMBER1;
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
        assertTrue(title.contains(GIVEN_NAME1));
        assertTrue(title.contains(TAB_TITLE));

        propertyModel.get(ON_FULLY_VISIBLE).onResult(true);
        verify(mSuccessCallback).onResult(true);

        when(mTabGroupModelFilter.getRelatedTabList(anyInt()))
                .thenReturn(Arrays.asList(mTab1, mTab2));
        assertEquals(DISMISS_IMMEDIATELY, propertyModel.get(ON_PRIMARY_ACTION).get().intValue());
        ArgumentMatcher<LoadUrlParams> matcher =
                (LoadUrlParams params) ->
                        TextUtils.equals(params.getUrl(), JUnitTestGURLs.URL_1.getSpec());
        verify(mTabCreator)
                .createNewTab(argThat(matcher), eq(TabLaunchType.FROM_TAB_GROUP_UI), eq(mTab2));
    }

    @Test
    public void testTabRemoved_NullUrl() {
        InstantMessage message = newInstantMessage(CollaborationEvent.TAB_REMOVED);
        message.attribution.tabMetadata.lastKnownUrl = null;
        mDelegate.displayInstantaneousMessage(message, mSuccessCallback);

        verify(mManagedMessageDispatcher)
                .enqueueWindowScopedMessage(mPropertyModelCaptor.capture(), anyBoolean());
        PropertyModel propertyModel = mPropertyModelCaptor.getValue();

        when(mTabGroupModelFilter.getRelatedTabList(anyInt()))
                .thenReturn(Arrays.asList(mTab1, mTab2));
        assertEquals(DISMISS_IMMEDIATELY, propertyModel.get(ON_PRIMARY_ACTION).get().intValue());
        ArgumentMatcher<LoadUrlParams> matcher =
                (LoadUrlParams params) -> TextUtils.equals(params.getUrl(), UrlConstants.NTP_URL);
        verify(mTabCreator)
                .createNewTab(argThat(matcher), eq(TabLaunchType.FROM_TAB_GROUP_UI), eq(mTab2));
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
        assertTrue(title.contains(GIVEN_NAME1));
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
        InstantMessage message = newInstantMessage(CollaborationEvent.COLLABORATION_MEMBER_ADDED);
        message.attribution.collaborationId = COLLABORATION_ID1;
        mDelegate.displayInstantaneousMessage(message, mSuccessCallback);

        verify(mManagedMessageDispatcher)
                .enqueueWindowScopedMessage(mPropertyModelCaptor.capture(), anyBoolean());
        PropertyModel propertyModel = mPropertyModelCaptor.getValue();
        @MessageIdentifier int messageIdentifier = propertyModel.get(MESSAGE_IDENTIFIER);
        assertEquals(MessageIdentifier.COLLABORATION_MEMBER_ADDED, messageIdentifier);
        String title = propertyModel.get(TITLE);
        assertTrue(title.contains(GIVEN_NAME1));
        assertTrue(title.contains(TAB_GROUP_TITLE));

        propertyModel.get(ON_FULLY_VISIBLE).onResult(true);
        verify(mSuccessCallback).onResult(true);

        Supplier<Integer> action = propertyModel.get(ON_PRIMARY_ACTION);
        assertNotNull(action);
        assertEquals(DISMISS_IMMEDIATELY, action.get().intValue());
        verify(mDataSharingTabManager).showManageSharing(any(), any());
    }

    @Test
    public void testCollaborationMemberAdded_NullCollaborationId() {
        InstantMessage message = newInstantMessage(CollaborationEvent.COLLABORATION_MEMBER_ADDED);
        message.attribution.collaborationId = null;
        mDelegate.displayInstantaneousMessage(message, mSuccessCallback);

        verify(mManagedMessageDispatcher)
                .enqueueWindowScopedMessage(mPropertyModelCaptor.capture(), anyBoolean());
        PropertyModel propertyModel = mPropertyModelCaptor.getValue();
        Supplier<Integer> action = propertyModel.get(ON_PRIMARY_ACTION);
        assertNotNull(action);
        assertEquals(DISMISS_IMMEDIATELY, action.get().intValue());
        verify(mDataSharingTabManager, never()).showManageSharing(any(), any());
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
        assertTrue(title.contains(GIVEN_NAME1));
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

        verify(mDataSharingNotificationManager)
                .showOtherJoinedNotification(any(), eq(SYNC_GROUP_ID1));
        verify(mSuccessCallback).onResult(true);
    }
}
