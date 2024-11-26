// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing.ui.recent_activity;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.collaboration.messaging.ActivityLogItem;
import org.chromium.components.collaboration.messaging.MessagingBackendService;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;

import java.util.ArrayList;
import java.util.List;

@RunWith(BaseRobolectricTestRunner.class)
public class RecentActivityListMediatorUnitTest {
    private static final String TEST_COLLABORATION_ID1 = "collaboration1";
    private static final String TEST_TITLE1 = "title1";
    private static final String TEST_TITLE2 = "title2";
    private static final String TEST_DESCRIPTION1 = "description1";
    private static final String TEST_DESCRIPTION2 = "description2";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private Context mContext;
    @Mock private MessagingBackendService mMessagingBackendService;
    @Mock private Runnable mCloseBottomSheetRunnable;
    @Mock private Runnable mCallback1;
    private ModelList mModelList;
    private RecentActivityListMediator mMediator;

    @Before
    public void setup() {
        setupMessages();
        mModelList = new ModelList();
        mMediator =
                new RecentActivityListMediator(
                        mContext, mModelList, mMessagingBackendService, mCloseBottomSheetRunnable);
    }

    private void setupMessages() {
        List<ActivityLogItem> testItems = new ArrayList<>();
        ActivityLogItem logItem1 = new ActivityLogItem();
        logItem1.titleText = TEST_TITLE1;
        logItem1.descriptionText = TEST_DESCRIPTION1;
        testItems.add(logItem1);
        ActivityLogItem logItem2 = new ActivityLogItem();
        logItem2.titleText = TEST_TITLE2;
        logItem2.descriptionText = TEST_DESCRIPTION2;
        testItems.add(logItem2);
        when(mMessagingBackendService.getActivityLog(any())).thenReturn(testItems);
    }

    @Test
    public void testTwoItems() {
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
