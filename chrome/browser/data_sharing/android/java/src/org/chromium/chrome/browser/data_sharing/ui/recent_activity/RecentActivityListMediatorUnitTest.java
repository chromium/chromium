// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing.ui.recent_activity;

import static org.mockito.Mockito.verify;

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
import org.chromium.components.collaboration.messaging.MessagingBackendService;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;

@RunWith(BaseRobolectricTestRunner.class)
public class RecentActivityListMediatorUnitTest {
    private static final String TEST_COLLABORATION_ID1 = "collaboration1";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private Context mContext;
    @Mock private MessagingBackendService mMessagingBackendService;
    @Mock private Runnable mCloseBottomSheetRunnable;
    @Mock private Runnable mCallback1;
    private ModelList mModelList;
    private RecentActivityListMediator mMediator;

    @Before
    public void setup() {
        mModelList = new ModelList();
        mMediator =
                new RecentActivityListMediator(
                        mContext, mModelList, mMessagingBackendService, mCloseBottomSheetRunnable);
    }

    @Test
    public void testBasic() {
        mMediator.requestShowUI(TEST_COLLABORATION_ID1, mCallback1);
        verify(mCallback1).run();
    }

    @Test
    public void testOnBottomSheetClosed() {
        mMediator.onBottomSheetClosed();
        Assert.assertEquals(0, mModelList.size());
    }
}
