// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing;

import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.notNull;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.graphics.Bitmap;
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
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper.FaviconImageCallback;
import org.chromium.url.GURL;

/** Unit tests for {@link DataSharingFaviconProvider}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class DataSharingFaviconProviderUnitTest {
    private static final GURL TAB_URL = new GURL("https://google.com");
    private static final GURL BITMAP_URL = new GURL("https://google.com/xyz");

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private Profile mProfile;
    @Mock private FaviconHelper mFaviconHelper;
    @Mock private Callback<Drawable> mCallback;
    private final Bitmap mBitmap = Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888);
    private Context mContext;

    private DataSharingFaviconProvider mFaviconProvider;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        mFaviconProvider = new DataSharingFaviconProvider(mContext, mProfile, mFaviconHelper);
    }

    @Test
    public void testBasic() {
        mFaviconProvider.fetchFavicon(TAB_URL, mCallback);
        ArgumentCaptor<FaviconImageCallback> callbackCaptor =
                ArgumentCaptor.forClass(FaviconImageCallback.class);
        verify(mFaviconHelper)
                .getForeignFaviconImageForURL(
                        eq(mProfile), eq(TAB_URL), anyInt(), callbackCaptor.capture());
        callbackCaptor.getValue().onFaviconAvailable(mBitmap, BITMAP_URL);
        verify(mCallback).onResult(notNull());
    }

    @Test
    public void testDestroy() {
        mFaviconProvider.destroy();
        verify(mFaviconHelper).destroy();
    }
}
