// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing;

import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.SavedTabGroupTab;
import org.chromium.components.tab_group_sync.TriggerSource;

/** Unit test for {@link DataSharingTabObserver} */
@RunWith(BaseRobolectricTestRunner.class)
public class DataSharingTabObserverUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private static final String COLLABORATION_ID = "data_sharing";
    private static final LocalTabGroupId LOCAL_ID = new LocalTabGroupId(Token.createRandom());
    private static final Integer TAB_ID = 123;

    @Mock private DataSharingTabManager mDataSharingTabManager;

    private DataSharingTabObserver mDataSharingTabObserver;

    @Before
    public void setUp() {
        mDataSharingTabObserver =
                new DataSharingTabObserver(COLLABORATION_ID, mDataSharingTabManager);
    }

    @Test
    public void testInvalidURL() {
        SavedTabGroup savedTabGroup = new SavedTabGroup();
        savedTabGroup.collaborationId = COLLABORATION_ID;
        savedTabGroup.localId = LOCAL_ID;
        SavedTabGroupTab savedTabGroupTab = new SavedTabGroupTab();
        savedTabGroupTab.localId = TAB_ID;
        savedTabGroup.savedTabs.add(savedTabGroupTab);
        mDataSharingTabObserver.onTabGroupAdded(savedTabGroup, TriggerSource.REMOTE);
        verify(mDataSharingTabManager).openTabGroupWithTabId(TAB_ID);
        verify(mDataSharingTabManager).deleteObserver(mDataSharingTabObserver);
    }
}
