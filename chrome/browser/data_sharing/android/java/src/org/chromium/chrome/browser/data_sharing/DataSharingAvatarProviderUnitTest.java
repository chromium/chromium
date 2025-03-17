// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing;

import static org.mockito.ArgumentMatchers.notNull;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.Drawable;

import androidx.appcompat.view.ContextThemeWrapper;
import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.data_sharing.DataSharingUIDelegate;
import org.chromium.components.data_sharing.GroupMember;
import org.chromium.components.data_sharing.SharedGroupTestHelper;
import org.chromium.components.data_sharing.configs.DataSharingAvatarBitmapConfig;

/** Unit tests for {@link DataSharingFaviconProvider}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class DataSharingAvatarProviderUnitTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private DataSharingUIDelegate mDataSharingUIDelegate;
    @Mock private Callback<Drawable> mAvatarCallback;
    private final Bitmap mBitmap = Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888);
    private Context mContext;
    private DataSharingAvatarProvider mAvatarProvider;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
        mAvatarProvider = new DataSharingAvatarProvider(mContext, mDataSharingUIDelegate);
    }

    @Test
    public void testFetchAvatar() {
        ArgumentCaptor<DataSharingAvatarBitmapConfig> callbackCaptor =
                ArgumentCaptor.forClass(DataSharingAvatarBitmapConfig.class);

        GroupMember groupMember = SharedGroupTestHelper.GROUP_MEMBER1;
        mAvatarProvider.getAvatarBitmap(groupMember, mAvatarCallback);
        verify(mDataSharingUIDelegate).getAvatarBitmap(callbackCaptor.capture());
        DataSharingAvatarBitmapConfig config = callbackCaptor.getValue();
        config.getDataSharingAvatarCallback().onAvatarLoaded(mBitmap);
        verify(mAvatarCallback).onResult(notNull());
    }
}
