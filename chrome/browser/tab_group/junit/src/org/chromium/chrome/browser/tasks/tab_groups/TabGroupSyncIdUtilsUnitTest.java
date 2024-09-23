// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_groups;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.SharedPreferences;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;

/** Tests for {@link TabGroupSyncIdUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabGroupSyncIdUtilsUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private static final String TAB_GROUP_SYNC_IDS_FILE_NAME = "tab_group_sync_ids";
    private static final int LOCAL_ID = 456;
    private static final String SYNC_ID = "SyncID";

    @Mock Context mContext;
    @Mock SharedPreferences mSharedPreferences;
    @Mock SharedPreferences.Editor mEditor;
    @Mock SharedPreferences.Editor mPutStringEditor;
    @Mock SharedPreferences.Editor mRemoveEditor;

    @Before
    public void setUp() {
        doReturn(mSharedPreferences)
                .when(mContext)
                .getSharedPreferences(TAB_GROUP_SYNC_IDS_FILE_NAME, Context.MODE_PRIVATE);
        doReturn(mEditor).when(mSharedPreferences).edit();
        doReturn(mRemoveEditor).when(mEditor).remove(any(String.class));
        doReturn(mPutStringEditor).when(mEditor).putString(any(String.class), any(String.class));
        ContextUtils.initApplicationContextForTests(mContext);
    }

    @Test
    public void testDeleteTabGroupSyncId() {
        TabGroupSyncIdUtils.deleteTabGroupSyncId(LOCAL_ID);

        verify(mEditor).remove(eq(String.valueOf(LOCAL_ID)));
        verify(mRemoveEditor).apply();
    }

    @Test
    public void testGetTabGroupSyncId() {
        // Mock that we have a stored tab group sync ID with reference to LOCAL_ID.
        when(mSharedPreferences.getString(String.valueOf(LOCAL_ID), null)).thenReturn(SYNC_ID);

        assertThat(TabGroupSyncIdUtils.getTabGroupSyncId(LOCAL_ID), equalTo(SYNC_ID));
    }

    @Test
    public void testStoreTabGroupSyncId() {
        TabGroupSyncIdUtils.putTabGroupSyncId(LOCAL_ID, SYNC_ID);

        verify(mEditor).putString(eq(String.valueOf(LOCAL_ID)), eq(SYNC_ID));
        verify(mPutStringEditor).apply();
    }
}
