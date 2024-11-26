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
import android.widget.ImageView;

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
import org.chromium.components.collaboration.messaging.MessageAttribution;
import org.chromium.components.collaboration.messaging.MessagingBackendService;
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
    private static final String TEST_TITLE1 = "title1";
    private static final String TEST_TITLE2 = "title2";
    private static final String TEST_DESCRIPTION1 = "description1";
    private static final String TEST_DESCRIPTION2 = "description2";
    private static final String TAB_URL1 = "https://google.com";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private Context mContext;
    @Mock private MessagingBackendService mMessagingBackendService;
    @Mock private FaviconProvider mFaviconProvider;
    @Mock private AvatarProvider mAvatarProvider;
    @Captor private ArgumentCaptor<Callback<Drawable>> mFaviconResponseCallbackCaptor;
    @Captor private ArgumentCaptor<Callback<Drawable>> mAvatarResponseCallbackCaptor;
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
                        mCloseBottomSheetRunnable);
    }

    private ActivityLogItem createLog1() {
        ActivityLogItem logItem = new ActivityLogItem();
        logItem.titleText = TEST_TITLE1;
        logItem.descriptionText = TEST_DESCRIPTION1;
        GroupMember triggeringUser = new GroupMember(null, null, null, MemberRole.OWNER, null, "");
        logItem.activityMetadata = new MessageAttribution();
        logItem.activityMetadata.triggeringUser = triggeringUser;
        logItem.activityMetadata.tabMetadata = new TabMessageMetadata();
        logItem.activityMetadata.tabMetadata.lastKnownUrl = TAB_URL1;
        return logItem;
    }

    private ActivityLogItem createLog2() {
        ActivityLogItem logItem = new ActivityLogItem();
        logItem.titleText = TEST_TITLE2;
        logItem.descriptionText = TEST_DESCRIPTION2;
        GroupMember triggeringUser = new GroupMember(null, null, null, MemberRole.OWNER, null, "");
        logItem.activityMetadata = new MessageAttribution();
        logItem.activityMetadata.triggeringUser = triggeringUser;
        logItem.activityMetadata.tabMetadata = new TabMessageMetadata();
        logItem.activityMetadata.tabMetadata.lastKnownUrl = TAB_URL1;
        return logItem;
    }

    @Test
    public void testTwoItems() {
        mTestItems.add(createLog1());
        mTestItems.add(createLog2());
        mMediator.requestShowUI(TEST_COLLABORATION_ID1, mCallback1);
        Assert.assertEquals(2, mModelList.size());
        Assert.assertEquals(
                TEST_TITLE1, mModelList.get(0).model.get(RecentActivityListProperties.TITLE_TEXT));
        Assert.assertEquals(
                TEST_DESCRIPTION1,
                mModelList.get(0).model.get(RecentActivityListProperties.DESCRIPTION_TEXT));
        Assert.assertEquals(
                TEST_TITLE2, mModelList.get(1).model.get(RecentActivityListProperties.TITLE_TEXT));
        Assert.assertEquals(
                TEST_DESCRIPTION2,
                mModelList.get(1).model.get(RecentActivityListProperties.DESCRIPTION_TEXT));
    }

    @Test
    public void testAvatar() {
        ActivityLogItem logItem = createLog1();
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
    public void testFavicon() {
        ActivityLogItem logItem = createLog1();
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
    public void testCallbackIsRunAtTheEnd() {
        mMediator.requestShowUI(TEST_COLLABORATION_ID1, mCallback1);
        verify(mCallback1).run();
    }

    @Test
    public void testOnBottomSheetClosed() {
        mMediator.onBottomSheetClosed();
        Assert.assertEquals(0, mModelList.size());
    }
}
