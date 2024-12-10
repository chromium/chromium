// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.notNull;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.graphics.drawable.Drawable;

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
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.url.GURL;

import java.util.Arrays;
import java.util.List;

/** Unit tests for {@link DataSharingFaviconProvider}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class DataSharingFaviconProviderUnitTest {
    private static final GURL TAB_URL = new GURL("https://google.com");
    private static final GURL BITMAP_URL = new GURL("https://google.com/xyz");

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private Profile mProfile;
    @Mock private BulkFaviconUtil mBulkFaviconUtil;
    @Mock private Callback<Drawable> mCallback;
    @Mock private Drawable mDrawable;
    private Context mContext;

    private DataSharingFaviconProvider mFaviconProvider;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        mFaviconProvider = new DataSharingFaviconProvider(mContext, mProfile, mBulkFaviconUtil);
    }

    @Test
    public void testBasic() {
        mFaviconProvider.fetchFavicon(TAB_URL, mCallback);
        ArgumentCaptor<Callback<List<Drawable>>> callbackCaptor =
                ArgumentCaptor.forClass(Callback.class);
        verify(mBulkFaviconUtil)
                .fetchAsDrawable(any(), eq(mProfile), any(), anyInt(), callbackCaptor.capture());
        callbackCaptor.getValue().onResult(Arrays.asList(mDrawable));
        verify(mCallback).onResult(notNull());
    }
}
