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
import org.chromium.base.TimeUtils;
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
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

@RunWith(BaseRobolectricTestRunner.class)
public class RecentActivityListMediatorUnitTest {
    private static final String TEST_COLLABORATION_ID1 = "collaboration1";
    private static final String USER_DISPLAY_NAME1 = "User 1";
    private static final String USER_DISPLAY_NAME2 = "User 2";
    private static final String TEST_DESCRIPTION_1 = "www.foo.com";
    private static final String TEST_DESCRIPTION_2 = "user2@gmail.com";
    private static final String TAB_URL1 = "https://google.com";
    private static final int TAB_ID1 = 5;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    private Context mContext;
    @Mock private MessagingBackendService mMessagingBackendService;
    @Mock private FaviconProvider mFaviconProvider;
    @Mock private AvatarProvider mAvatarProvider;
    @Captor private ArgumentCaptor<Callback<Drawable>> mFaviconResponseCallbackCaptor;
    @Captor private ArgumentCaptor<Callback<Drawable>> mAvatarResponseCallbackCaptor;
    @Mock private RecentActivityActionHandler mRecentActivityActionHandler;
    @Mock private Drawable mDrawable;
    @Mock private Runnable mCloseBottomSheetRunnable;
    @Mock private Runnable mCallback1;
    @Mock private ImageView mAvatarView;
    @Mock private ImageView mFaviconView;
    private ModelList mModelList;
    private RecentActivityListMediator mMediator;
    private List<ActivityLogItem> mTestItems = new ArrayList<>();

    @Before
    public void setup() {
        mContext = ApplicationProvider.getApplicationContext();
        when(mMessagingBackendService.getActivityLog(any())).thenReturn(mTestItems);
        doNothing()
                .when(mFaviconProvider)
                .fetchFavicon(any(), mFaviconResponseCallbackCaptor.capture());
        doNothing()
                .when(mAvatarProvider)
                .getAvatarBitmap(any(), mAvatarResponseCallbackCaptor.capture());

        mModelList = new ModelList();
        mMediator =
                new RecentActivityListMediator(
                        mContext,
                        mModelList,
                        mMessagingBackendService,
                        mFaviconProvider,
                        mAvatarProvider,
                        mRecentActivityActionHandler,
                        mCloseBottomSheetRunnable);
    }

    private ActivityLogItem createLog1(@CollaborationEvent int collaborationEvent) {
        ActivityLogItem logItem = new ActivityLogItem();
        logItem.collaborationEvent = collaborationEvent;
        logItem.userDisplayName = USER_DISPLAY_NAME1;
        logItem.description = TEST_DESCRIPTION_1;
        logItem.timeDeltaMs = TimeUtils.MILLISECONDS_PER_DAY;
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
        logItem.userDisplayName = USER_DISPLAY_NAME2;
        logItem.description = TEST_DESCRIPTION_2;
        logItem.timeDeltaMs = TimeUtils.MILLISECONDS_PER_DAY;
        GroupMember triggeringUser = new GroupMember(null, null, null, MemberRole.OWNER, null, "");
        logItem.activityMetadata = new MessageAttribution();
        logItem.activityMetadata.triggeringUser = triggeringUser;
        logItem.activityMetadata.tabMetadata = new TabMessageMetadata();
        logItem.activityMetadata.tabMetadata.lastKnownUrl = TAB_URL1;
        logItem.activityMetadata.tabMetadata.localTabId = TAB_ID1;
        return logItem;
    }

    @Test
    public void testTwoItemsTitleAndDescription() {
        // Expected title: "You changed a tab".
        ActivityLogItem logItem1 = createLog1(CollaborationEvent.TAB_UPDATED);
        logItem1.userIsSelf = true;
        mTestItems.add(logItem1);
        // Expected title: "User 2 removed a tab".
        mTestItems.add(createLog2(CollaborationEvent.TAB_REMOVED));

        // Show UI and verify.
        mMediator.requestShowUI(TEST_COLLABORATION_ID1, mCallback1);
        Assert.assertEquals(2, mModelList.size());

        String titleText1 = mModelList.get(0).model.get(RecentActivityListProperties.TITLE_TEXT);
        String descriptionText1 =
                mModelList.get(0).model.get(RecentActivityListProperties.DESCRIPTION_TEXT);
        String titleText2 = mModelList.get(1).model.get(RecentActivityListProperties.TITLE_TEXT);
        String descriptionText2 =
                mModelList.get(1).model.get(RecentActivityListProperties.DESCRIPTION_TEXT);

        Assert.assertEquals("You changed a tab", titleText1);
        Assert.assertEquals("www.foo.com • 1 day ago", descriptionText1);

        Assert.assertEquals("User 2 removed a tab", titleText2);
        Assert.assertEquals("user2@gmail.com • 1 day ago", descriptionText2);
    }

    @Test
    public void testDescriptionContainsTimestampOnly() {
        // Expected description: "8 minutes ago".
        ActivityLogItem logItem1 = createLog1(CollaborationEvent.TAB_UPDATED);
        logItem1.userIsSelf = true;
        logItem1.description = null;
        logItem1.timeDeltaMs = TimeUtils.MILLISECONDS_PER_MINUTE * 8;
        mTestItems.add(logItem1);

        // Show UI and verify.
        mMediator.requestShowUI(TEST_COLLABORATION_ID1, mCallback1);
        Assert.assertEquals(1, mModelList.size());

        String descriptionText1 =
                mModelList.get(0).model.get(RecentActivityListProperties.DESCRIPTION_TEXT);
        Assert.assertEquals("8 min ago", descriptionText1);
    }

    @Test
    public void testAvatar() {
        ActivityLogItem logItem = createLog1(CollaborationEvent.TAB_UPDATED);
        mTestItems.add(logItem);

        mMediator.requestShowUI(TEST_COLLABORATION_ID1, mCallback1);
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

        mMediator.requestShowUI(TEST_COLLABORATION_ID1, mCallback1);
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

        mMediator.requestShowUI(TEST_COLLABORATION_ID1, mCallback1);
        Assert.assertNull(
                mModelList.get(0).model.get(RecentActivityListProperties.FAVICON_PROVIDER));
    }

    @Test
    public void testActionFocusTab() {
        ActivityLogItem logItem = createLog1(CollaborationEvent.TAB_UPDATED);
        logItem.action = RecentActivityAction.FOCUS_TAB;
        mTestItems.add(logItem);
        mMediator.requestShowUI(TEST_COLLABORATION_ID1, mCallback1);
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
        mMediator.requestShowUI(TEST_COLLABORATION_ID1, mCallback1);
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
        mMediator.requestShowUI(TEST_COLLABORATION_ID1, mCallback1);
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
        mMediator.requestShowUI(TEST_COLLABORATION_ID1, mCallback1);
        OnClickListener onClickListener =
                mModelList.get(0).model.get(RecentActivityListProperties.ON_CLICK_LISTENER);
        onClickListener.onClick(null);
        verify(mRecentActivityActionHandler).manageSharing();
    }

    @Test
    public void testCallbackIsRunAtTheEnd() {
        mMediator.requestShowUI(TEST_COLLABORATION_ID1, mCallback1);
        verify(mCallback1).run();
    }

    @Test
    public void testOnBottomSheetClosed() {
        mMediator.onBottomSheetClosed();
        Assert.assertEquals(0, mModelList.size());
    }

    @Test
    public void testGetTitleResStringForAllEventTypes() {
        // Create one item for each type of collaboration event and add to the backend.
        int[] events = {
            CollaborationEvent.TAB_ADDED,
            CollaborationEvent.TAB_REMOVED,
            CollaborationEvent.TAB_UPDATED,
            CollaborationEvent.TAB_GROUP_NAME_UPDATED,
            CollaborationEvent.TAB_GROUP_COLOR_UPDATED,
            CollaborationEvent.COLLABORATION_MEMBER_ADDED,
            CollaborationEvent.COLLABORATION_MEMBER_REMOVED
        };
        for (int i = 0; i < events.length; i++) {
            ActivityLogItem logItem = createLog1(events[i]);
            mTestItems.add(logItem);
        }

        // Show UI and verify each item's title text.
        mMediator.requestShowUI(TEST_COLLABORATION_ID1, mCallback1);
        Assert.assertEquals(events.length, mModelList.size());

        Assert.assertEquals(
                "User 1 added a tab",
                mModelList.get(0).model.get(RecentActivityListProperties.TITLE_TEXT));
        Assert.assertEquals(
                "User 1 removed a tab",
                mModelList.get(1).model.get(RecentActivityListProperties.TITLE_TEXT));
        Assert.assertEquals(
                "User 1 changed a tab",
                mModelList.get(2).model.get(RecentActivityListProperties.TITLE_TEXT));
        Assert.assertEquals(
                "User 1 changed the group name",
                mModelList.get(3).model.get(RecentActivityListProperties.TITLE_TEXT));
        Assert.assertEquals(
                "User 1 changed the group color",
                mModelList.get(4).model.get(RecentActivityListProperties.TITLE_TEXT));
        Assert.assertEquals(
                "User 1 joined the group",
                mModelList.get(5).model.get(RecentActivityListProperties.TITLE_TEXT));
        Assert.assertEquals(
                "User 1 left the group",
                mModelList.get(6).model.get(RecentActivityListProperties.TITLE_TEXT));
    }
}
