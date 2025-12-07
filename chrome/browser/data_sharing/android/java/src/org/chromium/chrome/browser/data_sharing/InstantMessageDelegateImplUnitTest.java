// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
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
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Callback;
import org.chromium.base.Token;
import org.chromium.base.UnownedUserDataHost;
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
import org.chromium.components.messages.DismissReason;
import org.chromium.components.messages.ManagedMessageDispatcher;
import org.chromium.components.messages.MessageIdentifier;
import org.chromium.components.messages.MessagesFactory;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
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
import java.util.HashSet;
import java.util.Set;
import java.util.UUID;
import java.util.function.Supplier;

/** Unit tests for {@link InstantMessageDelegateImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
public class InstantMessageDelegateImplUnitTest {
    private static final Token TAB_GROUP_ID = new Token(1L, 2L);
    private static final int TAB_ID = 1;
    private static final String MESSAGE_CONTENT_1 = "Message Content 1";

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
    @Mock private Supplier<Boolean> mIsActiveWindowSupplier;
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
        when(mTabGroupModelFilter.tabGroupExists(TAB_GROUP_ID)).thenReturn(true);
        when(mTabGroupModelFilter.getGroupLastShownTabId(TAB_GROUP_ID)).thenReturn(TAB_ID);
        when(mTabGroupModelFilter.getTabModel()).thenReturn(mTabModel);
        when(mTabModel.getTabCreator()).thenReturn(mTabCreator);
        when(mIsActiveWindowSupplier.get()).thenReturn(false);

        mSyncedGroupTestHelper = new SyncedGroupTestHelper(mTabGroupSyncService);
        SavedTabGroup group = mSyncedGroupTestHelper.newTabGroup(SYNC_GROUP_ID1, TAB_GROUP_ID);
        group.savedTabs = SyncedGroupTestHelper.tabsFromCount(10);
        group.collaborationId = COLLABORATION_ID1;

        mDelegate =
                new InstantMessageDelegateImpl(
                        mMessagingBackendService, mDataSharingService, mTabGroupSyncService);
        mDelegate.attachWindow(
                mWindowAndroid,
                mTabGroupModelFilter,
                mDataSharingNotificationManager,
                mDataSharingTabManager,
                mIsActiveWindowSupplier);
    }

    private InstantMessage newInstantMessage(@CollaborationEvent int collaborationEvent) {
        MessageAttribution attribution = new MessageAttribution();
        attribution.tabMetadata = new TabMessageMetadata();
        attribution.tabMetadata.lastKnownUrl = JUnitTestGURLs.URL_1.getSpec();
        attribution.tabGroupMetadata = new TabGroupMessageMetadata();
        attribution.tabGroupMetadata.localTabGroupId = new LocalTabGroupId(TAB_GROUP_ID);
        attribution.tabGroupMetadata.syncTabGroupId = SYNC_GROUP_ID1;
        attribution.triggeringUser = GROUP_MEMBER1;
        InstantMessage instantMessage = new InstantMessage();
        instantMessage.attributions.add(attribution);
        instantMessage.collaborationEvent = collaborationEvent;
        instantMessage.level = InstantNotificationLevel.BROWSER;
        instantMessage.localizedMessage = MESSAGE_CONTENT_1;
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
        when(mTabGroupModelFilter.tabGroupExists(any())).thenReturn(false);
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
        assertEquals(MESSAGE_CONTENT_1, title);

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
        message.attributions.get(0).tabMetadata.lastKnownUrl = null;
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
        assertEquals(MESSAGE_CONTENT_1, title);

        verify(mSuccessCallback, never()).onResult(anyBoolean());

        propertyModel.get(ON_FULLY_VISIBLE).onResult(true);
        verify(mSuccessCallback).onResult(true);

        // When it stops being visible, success shouldn't change.
        propertyModel.get(ON_FULLY_VISIBLE).onResult(false);
        verify(mSuccessCallback, times(1)).onResult(anyBoolean());
    }

    @Test
    public void testTabNavigated_doubleShow() {
        mDelegate.displayInstantaneousMessage(
                newInstantMessage(CollaborationEvent.TAB_UPDATED), mSuccessCallback);

        verify(mManagedMessageDispatcher)
                .enqueueWindowScopedMessage(mPropertyModelCaptor.capture(), anyBoolean());
        PropertyModel propertyModel = mPropertyModelCaptor.getValue();

        verify(mSuccessCallback, never()).onResult(anyBoolean());

        // Initial show, should trigger the success callback.
        propertyModel.get(ON_FULLY_VISIBLE).onResult(true);
        verify(mSuccessCallback).onResult(true);

        // See crbug.com/393023075, it seems message dispatching will re-trigger visibly.
        // Chrome is backgrounded. Although, it's not technically to reshow the message since after
        // http://crrev.com/c/6388437 the message is dismissed after being hidden.
        propertyModel.get(ON_FULLY_VISIBLE).onResult(false);
        ShadowLooper.runUiThreadTasks();
        verify(mManagedMessageDispatcher)
                .dismissMessage(any(), eq(DismissReason.DISMISSED_BY_FEATURE));

        // Chrome is foregrounded.
        propertyModel.get(ON_FULLY_VISIBLE).onResult(true);

        // Message stops showing naturally
        propertyModel.get(ON_FULLY_VISIBLE).onResult(false);

        // Callback should still only have been invoked once.
        verify(mSuccessCallback, times(1)).onResult(anyBoolean());
    }

    @Test
    public void testCollaborationMemberAdded() {
        InstantMessage message = newInstantMessage(CollaborationEvent.COLLABORATION_MEMBER_ADDED);
        message.attributions.get(0).collaborationId = COLLABORATION_ID1;
        mDelegate.displayInstantaneousMessage(message, mSuccessCallback);

        verify(mManagedMessageDispatcher)
                .enqueueWindowScopedMessage(mPropertyModelCaptor.capture(), anyBoolean());
        PropertyModel propertyModel = mPropertyModelCaptor.getValue();
        @MessageIdentifier int messageIdentifier = propertyModel.get(MESSAGE_IDENTIFIER);
        assertEquals(MessageIdentifier.COLLABORATION_MEMBER_ADDED, messageIdentifier);
        assertEquals(MESSAGE_CONTENT_1, propertyModel.get(TITLE));

        propertyModel.get(ON_FULLY_VISIBLE).onResult(true);
        verify(mSuccessCallback).onResult(true);

        Supplier<Integer> action = propertyModel.get(ON_PRIMARY_ACTION);
        assertNotNull(action);
        assertEquals(DISMISS_IMMEDIATELY, action.get().intValue());
        verify(mDataSharingTabManager).createOrManageFlow(any(), anyInt(), any());
    }

    @Test
    public void testCollaborationMemberAdded_NullCollaborationId() {
        InstantMessage message = newInstantMessage(CollaborationEvent.COLLABORATION_MEMBER_ADDED);
        message.attributions.get(0).collaborationId = null;
        mDelegate.displayInstantaneousMessage(message, mSuccessCallback);

        verify(mManagedMessageDispatcher)
                .enqueueWindowScopedMessage(mPropertyModelCaptor.capture(), anyBoolean());
        PropertyModel propertyModel = mPropertyModelCaptor.getValue();
        Supplier<Integer> action = propertyModel.get(ON_PRIMARY_ACTION);
        assertNotNull(action);
        assertEquals(DISMISS_IMMEDIATELY, action.get().intValue());
        verify(mDataSharingTabManager, never()).createOrManageFlow(any(), anyInt(), any());
    }

    @Test
    public void testCollaborationRemoved() {
        mDelegate.displayInstantaneousMessage(
                newInstantMessage(CollaborationEvent.TAB_GROUP_REMOVED), mSuccessCallback);

        verify(mManagedMessageDispatcher)
                .enqueueWindowScopedMessage(mPropertyModelCaptor.capture(), anyBoolean());
        PropertyModel propertyModel = mPropertyModelCaptor.getValue();
        @MessageIdentifier int messageIdentifier = propertyModel.get(MESSAGE_IDENTIFIER);
        assertEquals(MessageIdentifier.COLLABORATION_REMOVED, messageIdentifier);
        assertEquals(MESSAGE_CONTENT_1, propertyModel.get(TITLE));

        propertyModel.get(ON_FULLY_VISIBLE).onResult(true);
        verify(mSuccessCallback).onResult(true);
    }

    @Test
    public void testCollaborationRemoved_NoLastFocusedWindow() {
        when(mTabGroupModelFilter.tabGroupExists(TAB_GROUP_ID)).thenReturn(false);
        mDelegate.displayInstantaneousMessage(
                newInstantMessage(CollaborationEvent.TAB_GROUP_REMOVED), mSuccessCallback);

        verify(mManagedMessageDispatcher, never()).enqueueWindowScopedMessage(any(), anyBoolean());
    }

    @Test
    public void testCollaborationRemoved_LastFocusedWindow() {
        when(mTabGroupModelFilter.tabGroupExists(TAB_GROUP_ID)).thenReturn(false);
        when(mIsActiveWindowSupplier.get()).thenReturn(true);
        mDelegate.displayInstantaneousMessage(
                newInstantMessage(CollaborationEvent.TAB_GROUP_REMOVED), mSuccessCallback);

        verify(mManagedMessageDispatcher)
                .enqueueWindowScopedMessage(mPropertyModelCaptor.capture(), anyBoolean());
        PropertyModel propertyModel = mPropertyModelCaptor.getValue();
        @MessageIdentifier int messageIdentifier = propertyModel.get(MESSAGE_IDENTIFIER);
        assertEquals(MessageIdentifier.COLLABORATION_REMOVED, messageIdentifier);
        assertEquals(MESSAGE_CONTENT_1, propertyModel.get(TITLE));

        propertyModel.get(ON_FULLY_VISIBLE).onResult(true);
        verify(mSuccessCallback).onResult(true);
    }

    @Test
    public void testSystemNotification() {
        InstantMessage message = newInstantMessage(CollaborationEvent.COLLABORATION_MEMBER_ADDED);
        message.level = InstantNotificationLevel.SYSTEM;
        message.attributions.get(0).id =
                UUID.fromString("00000000-0000-0000-0000-000000000009").toString();

        mDelegate.displayInstantaneousMessage(message, mSuccessCallback);

        verify(mDataSharingNotificationManager)
                .showOtherJoinedNotification(any(), eq(SYNC_GROUP_ID1), eq(9));
        verify(mSuccessCallback).onResult(true);
    }

    @Test
    public void testHideInstantMessage_MessageIsShowing() {
        String messageIdToHide = "1";
        InstantMessage message = newInstantMessage(CollaborationEvent.TAB_REMOVED);
        message.attributions.get(0).id = messageIdToHide;

        mDelegate.displayInstantaneousMessage(message, mSuccessCallback);
        verify(mManagedMessageDispatcher)
                .enqueueWindowScopedMessage(mPropertyModelCaptor.capture(), anyBoolean());
        PropertyModel displayedModel = mPropertyModelCaptor.getValue();

        Set<String> idsToHide = new HashSet<>();
        idsToHide.add(messageIdToHide);
        mDelegate.hideInstantaneousMessage(idsToHide);
        ShadowLooper.runUiThreadTasks();

        verify(mManagedMessageDispatcher)
                .dismissMessage(displayedModel, DismissReason.DISMISSED_BY_FEATURE);
    }

    @Test
    public void testHideInstantMessage_MessageNotShowing() {
        InstantMessage messageToShow = newInstantMessage(CollaborationEvent.TAB_REMOVED);
        messageToShow.attributions.get(0).id = "1";
        mDelegate.displayInstantaneousMessage(messageToShow, mSuccessCallback);
        verify(mManagedMessageDispatcher).enqueueWindowScopedMessage(any(), anyBoolean());

        // Attempt to hide a message ID that was never displayed.
        Set<String> idsToHide = new HashSet<>();
        idsToHide.add("2");
        mDelegate.hideInstantaneousMessage(idsToHide);
        ShadowLooper.runUiThreadTasks();

        // This should have noop-ed.
        verify(mManagedMessageDispatcher, never()).dismissMessage(any(), anyInt());
    }
}
