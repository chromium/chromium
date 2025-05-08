// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing.ui.recent_activity;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.View;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

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

import org.chromium.base.CallbackUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.data_sharing.ui.recent_activity.RecentActivityListCoordinator.AvatarProvider;
import org.chromium.chrome.browser.data_sharing.ui.recent_activity.RecentActivityListCoordinator.FaviconProvider;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.collaboration.messaging.ActivityLogItem;
import org.chromium.components.collaboration.messaging.CollaborationEvent;
import org.chromium.components.collaboration.messaging.MessageAttribution;
import org.chromium.components.collaboration.messaging.MessagingBackendService;
import org.chromium.components.collaboration.messaging.TabMessageMetadata;
import org.chromium.components.data_sharing.GroupMember;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.ui.base.TestActivity;

import java.util.ArrayList;
import java.util.List;

@RunWith(BaseRobolectricTestRunner.class)
public class RecentActivityListCoordinatorUnitTest {
    private static final String TEST_COLLABORATION_ID1 = "collaboration1";
    private static final String USER_DISPLAY_NAME1 = "User 1";
    private static final String TEST_DESCRIPTION_1 = "www.foo.com";
    private static final String TEST_TIMESTAMP_1 = "1 day ago";
    private static final String TAB_URL1 = "https://google.com";
    private static final int TAB_ID1 = 5;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Captor private ArgumentCaptor<EmptyBottomSheetObserver> mBottomSheetObserverCaptor;
    @Captor private ArgumentCaptor<BottomSheetContent> mBottomSheetContentCaptor;

    @Mock private MessagingBackendService mMessagingBackendService;
    @Mock private TabGroupSyncService mTabGroupSyncService;
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private FaviconProvider mFaviconProvider;
    @Mock private AvatarProvider mAvatarProvider;
    @Mock private RecentActivityActionHandler mRecentActivityActionHandler;
    private Activity mActivity;
    private RecentActivityListCoordinator mCoordinator;

    @Before
    public void setup() {
        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);
        doNothing().when(mBottomSheetController).addObserver(mBottomSheetObserverCaptor.capture());
        when(mBottomSheetController.requestShowContent(
                        mBottomSheetContentCaptor.capture(), anyBoolean()))
                .thenAnswer(
                        invocation -> {
                            return true;
                        });

        mCoordinator =
                new RecentActivityListCoordinator(
                        TEST_COLLABORATION_ID1,
                        mActivity,
                        mBottomSheetController,
                        mMessagingBackendService,
                        mTabGroupSyncService,
                        mFaviconProvider,
                        mAvatarProvider,
                        mRecentActivityActionHandler,
                        CallbackUtils.emptyRunnable());
        verify(mBottomSheetController).addObserver(any());
    }

    private ActivityLogItem createLog1() {
        ActivityLogItem logItem = new ActivityLogItem();
        logItem.collaborationEvent = CollaborationEvent.TAB_ADDED;
        logItem.titleText = USER_DISPLAY_NAME1 + " changed a tab";
        logItem.descriptionText = TEST_DESCRIPTION_1;
        logItem.timeDeltaText = TEST_TIMESTAMP_1;
        GroupMember triggeringUser =
                new GroupMember(
                        null,
                        null,
                        null,
                        org.chromium.components.data_sharing.member_role.MemberRole.OWNER,
                        null,
                        "");
        logItem.activityMetadata = new MessageAttribution();
        logItem.activityMetadata.triggeringUser = triggeringUser;
        logItem.activityMetadata.tabMetadata = new TabMessageMetadata();
        logItem.activityMetadata.tabMetadata.lastKnownUrl = TAB_URL1;
        logItem.activityMetadata.tabMetadata.localTabId = TAB_ID1;
        return logItem;
    }

    @Test
    public void testOpenBottomSheet() {
        mCoordinator.requestShowUI();
        verify(mBottomSheetController).requestShowContent(any(), eq(true));
    }

    @Test
    public void testCloseBottomSheetRemovesBottomSheetObserver() {
        mCoordinator.requestShowUI();
        verify(mBottomSheetController).requestShowContent(any(), eq(true));
        mBottomSheetObserverCaptor.getValue().onSheetClosed(0);
        verify(mBottomSheetController).removeObserver(any());
        verify(mTabGroupSyncService).removeObserver(any());
    }

    @Test
    public void testEmptyState_Shown() {
        when(mMessagingBackendService.getActivityLog(any())).thenReturn(new ArrayList<>());
        mCoordinator.requestShowUI();
        View emptyView =
                mBottomSheetContentCaptor
                        .getValue()
                        .getContentView()
                        .findViewById(R.id.recent_activity_empty_view);
        View recyclerView =
                mBottomSheetContentCaptor
                        .getValue()
                        .getContentView()
                        .findViewById(R.id.recent_activity_recycler_view);
        Assert.assertEquals(View.VISIBLE, emptyView.getVisibility());
        Assert.assertEquals(View.GONE, recyclerView.getVisibility());
    }

    @Test
    public void testEmptyState_NotShown() {
        List<ActivityLogItem> logItems = new ArrayList<>();
        logItems.add(createLog1());
        when(mMessagingBackendService.getActivityLog(any())).thenReturn(logItems);
        mCoordinator.requestShowUI();
        View emptyView =
                mBottomSheetContentCaptor
                        .getValue()
                        .getContentView()
                        .findViewById(R.id.recent_activity_empty_view);
        View recyclerView =
                mBottomSheetContentCaptor
                        .getValue()
                        .getContentView()
                        .findViewById(R.id.recent_activity_recycler_view);
        Assert.assertEquals(View.GONE, emptyView.getVisibility());
        Assert.assertEquals(View.VISIBLE, recyclerView.getVisibility());
    }
}
