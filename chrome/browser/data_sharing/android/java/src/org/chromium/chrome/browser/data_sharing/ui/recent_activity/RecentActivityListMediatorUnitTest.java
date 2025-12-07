// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing.ui.recent_activity;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.view.View.OnClickListener;
import android.widget.ImageView;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Assert;
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
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.data_sharing.ui.recent_activity.RecentActivityListCoordinator.AvatarProvider;
import org.chromium.chrome.browser.data_sharing.ui.recent_activity.RecentActivityListCoordinator.FaviconProvider;
import org.chromium.components.collaboration.messaging.ActivityLogItem;
import org.chromium.components.collaboration.messaging.CollaborationEvent;
import org.chromium.components.collaboration.messaging.MessageAttribution;
import org.chromium.components.collaboration.messaging.MessagingBackendService;
import org.chromium.components.collaboration.messaging.RecentActivityAction;
import org.chromium.components.collaboration.messaging.TabMessageMetadata;
import org.chromium.components.data_sharing.GroupMember;
import org.chromium.components.data_sharing.member_role.MemberRole;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_group_sync.TriggerSource;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

@RunWith(BaseRobolectricTestRunner.class)
public class RecentActivityListMediatorUnitTest {
    private static final String TEST_COLLABORATION_ID1 = "collaboration1";
    private static final String USER_DISPLAY_NAME1 = "User 1";
    private static final String USER_DISPLAY_NAME2 = "User 2";
    private static final String TEST_DESCRIPTION_1 = "www.foo.com";
    private static final String TEST_TIMESTAMP_1 = "1 day ago";
    private static final String TEST_DESCRIPTION_2 = "user2@gmail.com";
    private static final String TEST_TIMESTAMP_2 = "1 day ago";
    private static final String TAB_URL1 = "https://google.com";
    private static final int TAB_ID1 = 5;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    private Context mContext;
    @Mock private MessagingBackendService mMessagingBackendService;
    @Mock private TabGroupSyncService mTabGroupSyncService;
    @Mock private FaviconProvider mFaviconProvider;
    @Mock private AvatarProvider mAvatarProvider;
    @Captor private ArgumentCaptor<Callback<Drawable>> mFaviconResponseCallbackCaptor;
    @Captor private ArgumentCaptor<Callback<Drawable>> mAvatarResponseCallbackCaptor;
    @Captor private ArgumentCaptor<TabGroupSyncService.Observer> mTabGroupSyncObserverCaptor;
    @Mock private RecentActivityActionHandler mRecentActivityActionHandler;
    @Mock private Drawable mDrawable;
    @Mock private Runnable mCloseBottomSheetRunnable;
    @Mock private Runnable mCallback1;
    @Mock private ImageView mAvatarView;
    @Mock private ImageView mFaviconView;
    private PropertyModel mPropertyModel;
    private ModelList mModelList;
    private RecentActivityListMediator mMediator;
    private final List<ActivityLogItem> mTestItems = new ArrayList<>();

    @Before
    public void setup() {
        mContext = ApplicationProvider.getApplicationContext();
        when(mMessagingBackendService.getActivityLog(any())).thenReturn(mTestItems);
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {});
        doNothing()
                .when(mFaviconProvider)
                .fetchFavicon(any(), mFaviconResponseCallbackCaptor.capture());
        doNothing()
                .when(mAvatarProvider)
                .getAvatarBitmap(any(), mAvatarResponseCallbackCaptor.capture());
        doNothing().when(mTabGroupSyncService).addObserver(mTabGroupSyncObserverCaptor.capture());

        mPropertyModel =
                new PropertyModel.Builder(RecentActivityContainerProperties.ALL_KEYS).build();

        mModelList = new ModelList();
        mMediator =
                new RecentActivityListMediator(
                        TEST_COLLABORATION_ID1,
                        mContext,
                        mPropertyModel,
                        mModelList,
                        mMessagingBackendService,
                        mTabGroupSyncService,
                        mFaviconProvider,
                        mAvatarProvider,
                        mRecentActivityActionHandler,
                        mCloseBottomSheetRunnable);
    }

    private ActivityLogItem createLog1(@CollaborationEvent int collaborationEvent) {
        ActivityLogItem logItem = new ActivityLogItem();
        logItem.collaborationEvent = collaborationEvent;
        logItem.titleText = USER_DISPLAY_NAME1 + " changed a tab";
        logItem.descriptionText = TEST_DESCRIPTION_1;
        logItem.timeDeltaText = TEST_TIMESTAMP_1;
        GroupMember triggeringUser = new GroupMember(null, null, null, MemberRole.OWNER, null, "");
        logItem.activityMetadata = new MessageAttribution();
        logItem.activityMetadata.triggeringUser = triggeringUser;
        logItem.activityMetadata.tabMetadata = new TabMessageMetadata();
        logItem.activityMetadata.tabMetadata.lastKnownUrl = TAB_URL1;
        logItem.activityMetadata.tabMetadata.localTabId = TAB_ID1;
        return logItem;
    }

    private ActivityLogItem createLog2(@CollaborationEvent int collaborationEvent) {
        ActivityLogItem logItem = new ActivityLogItem();
        logItem.collaborationEvent = collaborationEvent;
        logItem.titleText = USER_DISPLAY_NAME2 + " removed a tab";
        logItem.descriptionText = TEST_DESCRIPTION_2;
        logItem.timeDeltaText = TEST_TIMESTAMP_1;
        GroupMember affectedUser = new GroupMember(null, null, null, MemberRole.OWNER, null, "");
        logItem.activityMetadata = new MessageAttribution();
        logItem.activityMetadata.affectedUser = affectedUser;
        logItem.activityMetadata.tabMetadata = new TabMessageMetadata();
        logItem.activityMetadata.tabMetadata.lastKnownUrl = TAB_URL1;
        logItem.activityMetadata.tabMetadata.localTabId = TAB_ID1;
        return logItem;
    }

    @Test
    public void testTwoItemsTitleAndDescription() {
        // Expected title: "User 1 changed a tab".
        ActivityLogItem logItem1 = createLog1(CollaborationEvent.TAB_UPDATED);
        mTestItems.add(logItem1);
        // Expected title: "User 2 removed a tab".
        mTestItems.add(createLog2(CollaborationEvent.TAB_REMOVED));

        // Show UI and verify.
        mMediator.requestShowUI(mCallback1);
        Assert.assertEquals(2, mModelList.size());

        String titleText1 = mModelList.get(0).model.get(RecentActivityListProperties.TITLE_TEXT);
        DescriptionAndTimestamp descriptionAndTimestamp1 =
                mModelList
                        .get(0)
                        .model
                        .get(RecentActivityListProperties.DESCRIPTION_AND_TIMESTAMP_TEXT);
        String titleText2 = mModelList.get(1).model.get(RecentActivityListProperties.TITLE_TEXT);
        DescriptionAndTimestamp descriptionAndTimestamp2 =
                mModelList
                        .get(1)
                        .model
                        .get(RecentActivityListProperties.DESCRIPTION_AND_TIMESTAMP_TEXT);

        Assert.assertEquals("User 1 changed a tab", titleText1);
        Assert.assertEquals("www.foo.com", descriptionAndTimestamp1.description);
        Assert.assertEquals("1 day ago", descriptionAndTimestamp1.timestamp);

        Assert.assertEquals("User 2 removed a tab", titleText2);
        Assert.assertEquals("user2@gmail.com", descriptionAndTimestamp2.description);
        Assert.assertEquals("1 day ago", descriptionAndTimestamp2.timestamp);
    }

    @Test
    public void testAvatar() {
        ActivityLogItem logItem = createLog1(CollaborationEvent.TAB_UPDATED);
        mTestItems.add(logItem);

        mMediator.requestShowUI(mCallback1);
        mModelList
                .get(0)
                .model
                .get(RecentActivityListProperties.AVATAR_PROVIDER)
                .onResult(mAvatarView);
        verify(mAvatarProvider, times(1))
                .getAvatarBitmap(eq(logItem.activityMetadata.triggeringUser), any());
        mAvatarResponseCallbackCaptor.getValue().onResult(mDrawable);
        verify(mAvatarView, times(1)).setImageDrawable(mDrawable);
    }

    @Test
    public void testFaviconIsShown() {
        ActivityLogItem logItem = createLog1(CollaborationEvent.TAB_UPDATED);
        logItem.showFavicon = true;
        mTestItems.add(logItem);

        mMediator.requestShowUI(mCallback1);
        mModelList
                .get(0)
                .model
                .get(RecentActivityListProperties.FAVICON_PROVIDER)
                .onResult(mFaviconView);
        verify(mFaviconProvider, times(1)).fetchFavicon(eq(new GURL(TAB_URL1)), any());
        mFaviconResponseCallbackCaptor.getValue().onResult(mDrawable);
        verify(mFaviconView, times(1)).setImageDrawable(mDrawable);
    }

    @Test
    public void testFaviconNotShown() {
        ActivityLogItem logItem = createLog1(CollaborationEvent.TAB_UPDATED);
        logItem.showFavicon = false;
        mTestItems.add(logItem);

        mMediator.requestShowUI(mCallback1);
        Assert.assertNull(
                mModelList.get(0).model.get(RecentActivityListProperties.FAVICON_PROVIDER));
    }

    @Test
    public void testActionFocusTab() {
        ActivityLogItem logItem = createLog1(CollaborationEvent.TAB_UPDATED);
        logItem.action = RecentActivityAction.FOCUS_TAB;
        mTestItems.add(logItem);
        mMediator.requestShowUI(mCallback1);
        OnClickListener onClickListener =
                mModelList.get(0).model.get(RecentActivityListProperties.ON_CLICK_LISTENER);
        onClickListener.onClick(null);
        verify(mRecentActivityActionHandler).focusTab(eq(TAB_ID1));
    }

    @Test
    public void testActionReopenTab() {
        ActivityLogItem logItem = createLog1(CollaborationEvent.TAB_REMOVED);
        logItem.action = RecentActivityAction.REOPEN_TAB;
        mTestItems.add(logItem);
        mMediator.requestShowUI(mCallback1);
        OnClickListener onClickListener =
                mModelList.get(0).model.get(RecentActivityListProperties.ON_CLICK_LISTENER);
        onClickListener.onClick(null);
        verify(mRecentActivityActionHandler).reopenTab(eq(TAB_URL1));
    }

    @Test
    public void testActionOpenTabGroupEditDialog() {
        ActivityLogItem logItem = createLog1(CollaborationEvent.TAB_GROUP_COLOR_UPDATED);
        logItem.action = RecentActivityAction.OPEN_TAB_GROUP_EDIT_DIALOG;
        mTestItems.add(logItem);
        mMediator.requestShowUI(mCallback1);
        OnClickListener onClickListener =
                mModelList.get(0).model.get(RecentActivityListProperties.ON_CLICK_LISTENER);
        onClickListener.onClick(null);
        verify(mRecentActivityActionHandler).openTabGroupEditDialog();
    }

    @Test
    public void testActionManageSharingDialog() {
        ActivityLogItem logItem = createLog1(CollaborationEvent.COLLABORATION_MEMBER_ADDED);
        logItem.action = RecentActivityAction.MANAGE_SHARING;
        mTestItems.add(logItem);
        mMediator.requestShowUI(mCallback1);
        OnClickListener onClickListener =
                mModelList.get(0).model.get(RecentActivityListProperties.ON_CLICK_LISTENER);
        onClickListener.onClick(null);
        verify(mRecentActivityActionHandler).manageSharing();
    }

    @Test
    public void testCallbackIsRunAtTheEnd() {
        mMediator.requestShowUI(mCallback1);
        verify(mCallback1).run();
    }

    @Test
    public void testOnBottomSheetClosed() {
        mMediator.onBottomSheetClosed();
        Assert.assertEquals(0, mModelList.size());
        verify(mTabGroupSyncService).removeObserver(any());
    }

    @Test
    public void testTabGroupRemovedClosesBottomSheet() {
        mMediator.requestShowUI(mCallback1);
        Assert.assertNotNull(mTabGroupSyncObserverCaptor.getValue());

        mTabGroupSyncObserverCaptor.getValue().onTabGroupRemoved("someId", TriggerSource.REMOTE);
        verify(mCloseBottomSheetRunnable).run();
    }
}
