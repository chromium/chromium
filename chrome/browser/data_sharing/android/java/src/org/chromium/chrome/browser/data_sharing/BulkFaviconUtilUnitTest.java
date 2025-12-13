// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.Drawable;

import androidx.test.core.app.ApplicationProvider;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.components.browser_ui.widget.RoundedIconGenerator;
import org.chromium.url.GURL;

import java.util.Arrays;
import java.util.Collections;
import java.util.List;

@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BulkFaviconUtilUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private FaviconHelper mFaviconHelper;
    @Mock private RoundedIconGenerator mRoundedIconGenerator;
    @Captor private ArgumentCaptor<FaviconHelper.FaviconImageCallback> mCallbackCaptor;
    @Captor private ArgumentCaptor<List<Bitmap>> mResultCaptor;

    private Context mContext;
    private BulkFaviconUtil mBulkFaviconUtil;
    private final GURL mGurl1 = new GURL("https://www.google.com");
    private final GURL mGurl2 = new GURL("https://www.chromium.org");
    private final List<GURL> mGurlList = Arrays.asList(mGurl1, mGurl2);

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        mBulkFaviconUtil = new BulkFaviconUtil();
        mBulkFaviconUtil.setFaviconHelperForTesting(mFaviconHelper);
        mBulkFaviconUtil.setRoundedIconGeneratorForTesting(mRoundedIconGenerator);
        // Necessary to avoid an NPE in |FaviconUtils.getIconDrawableWithFilter|.
    }

    @After
    public void tearDown() {
        mBulkFaviconUtil.destroy();
    }

    @Test
    public void testFetchAsBitmap_emptyList() {
        Callback<List<Bitmap>> callback = mock(Callback.class);
        mBulkFaviconUtil.fetchAsBitmap(mContext, mProfile, Collections.emptyList(), 16, callback);
        verify(callback).onResult(Collections.emptyList());
        verify(mFaviconHelper, never()).getForeignFaviconImageForURL(any(), any(), anyInt(), any());
    }

    @Test
    public void testFetchAsBitmap_singleUrl() {
        Bitmap mockBitmap = mock(Bitmap.class);
        doAnswer(
                        invocation -> {
                            mCallbackCaptor.getValue().onFaviconAvailable(mockBitmap, mGurl1);
                            return null;
                        })
                .when(mFaviconHelper)
                .getForeignFaviconImageForURL(
                        eq(mProfile), eq(mGurl1), anyInt(), mCallbackCaptor.capture());

        Callback<List<Bitmap>> callback = mock(Callback.class);
        mBulkFaviconUtil.fetchAsBitmap(mContext, mProfile, Arrays.asList(mGurl1), 16, callback);
        verify(callback).onResult(Arrays.asList(mockBitmap));
    }

    @Test
    public void testFetchAsBitmap_twoUrls() {
        Bitmap mockBitmap1 = mock(Bitmap.class);
        doAnswer(
                        invocation -> {
                            mCallbackCaptor.getValue().onFaviconAvailable(mockBitmap1, mGurl1);
                            return null;
                        })
                .when(mFaviconHelper)
                .getForeignFaviconImageForURL(
                        eq(mProfile), eq(mGurl1), anyInt(), mCallbackCaptor.capture());

        Bitmap mockBitmap2 = mock(Bitmap.class);
        doAnswer(
                        invocation -> {
                            mCallbackCaptor.getValue().onFaviconAvailable(mockBitmap2, mGurl2);
                            return null;
                        })
                .when(mFaviconHelper)
                .getForeignFaviconImageForURL(
                        eq(mProfile), eq(mGurl2), anyInt(), mCallbackCaptor.capture());

        Callback<List<Bitmap>> callback = mock(Callback.class);
        mBulkFaviconUtil.fetchAsBitmap(mContext, mProfile, mGurlList, 16, callback);
        verify(callback).onResult(Arrays.asList(mockBitmap1, mockBitmap2));
    }

    @Test
    public void testFetchAsBitmap_nullFavicon() {
        doAnswer(
                        invocation -> {
                            mCallbackCaptor.getValue().onFaviconAvailable(null, mGurl1);
                            return null;
                        })
                .when(mFaviconHelper)
                .getForeignFaviconImageForURL(
                        eq(mProfile), eq(mGurl1), anyInt(), mCallbackCaptor.capture());

        Callback<List<Bitmap>> callback = mock(Callback.class);
        mBulkFaviconUtil.fetchAsBitmap(mContext, mProfile, Arrays.asList(mGurl1), 16, callback);

        // Verify that the default favicon is used.
        verify(callback).onResult(mResultCaptor.capture());
        assertEquals(1, mResultCaptor.getValue().size());
    }

    @Test
    public void testFetchAsDrawable_emptyList() {
        Callback<List<Drawable>> callback = mock(Callback.class);
        mBulkFaviconUtil.fetchAsDrawable(mContext, mProfile, Collections.emptyList(), 16, callback);
        verify(callback).onResult(Collections.emptyList());
        verify(mFaviconHelper, never()).getForeignFaviconImageForURL(any(), any(), anyInt(), any());
    }

    @Test
    public void testFetchAsDrawable_singleUrl() {
        Bitmap realBitmap = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);
        doAnswer(
                        invocation -> {
                            mCallbackCaptor.getValue().onFaviconAvailable(realBitmap, mGurl1);
                            return null;
                        })
                .when(mFaviconHelper)
                .getForeignFaviconImageForURL(
                        eq(mProfile), eq(mGurl1), anyInt(), mCallbackCaptor.capture());

        Callback<List<Drawable>> callback = mock(Callback.class);
        mBulkFaviconUtil.fetchAsDrawable(mContext, mProfile, Arrays.asList(mGurl1), 16, callback);

        ArgumentCaptor<List<Drawable>> resultCaptor = ArgumentCaptor.forClass(List.class);
        verify(callback).onResult(resultCaptor.capture());
        assertEquals(1, resultCaptor.getValue().size());
        assertTrue(resultCaptor.getValue().get(0) != null);
    }

    @Test
    public void testFetchAsDrawable_twoUrls() {
        Bitmap realBitmap1 = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);
        doAnswer(
                        invocation -> {
                            mCallbackCaptor.getValue().onFaviconAvailable(realBitmap1, mGurl1);
                            return null;
                        })
                .when(mFaviconHelper)
                .getForeignFaviconImageForURL(
                        eq(mProfile), eq(mGurl1), anyInt(), mCallbackCaptor.capture());

        Bitmap realBitmap2 = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);
        doAnswer(
                        invocation -> {
                            mCallbackCaptor.getValue().onFaviconAvailable(realBitmap2, mGurl2);
                            return null;
                        })
                .when(mFaviconHelper)
                .getForeignFaviconImageForURL(
                        eq(mProfile), eq(mGurl2), anyInt(), mCallbackCaptor.capture());

        Callback<List<Drawable>> callback = mock(Callback.class);
        mBulkFaviconUtil.fetchAsDrawable(mContext, mProfile, mGurlList, 16, callback);

        ArgumentCaptor<List<Drawable>> resultCaptor = ArgumentCaptor.forClass(List.class);
        verify(callback).onResult(resultCaptor.capture());
        assertEquals(2, resultCaptor.getValue().size());
        assertTrue(resultCaptor.getValue().get(0) != null);
        assertTrue(resultCaptor.getValue().get(1) != null);
    }

    @Test
    public void testFetchAsDrawable_nullFavicon() {
        doAnswer(
                        invocation -> {
                            mCallbackCaptor.getValue().onFaviconAvailable(null, mGurl1);
                            return null;
                        })
                .when(mFaviconHelper)
                .getForeignFaviconImageForURL(
                        eq(mProfile), eq(mGurl1), anyInt(), mCallbackCaptor.capture());

        Bitmap realBitmap = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);
        doAnswer(
                        invocation -> {
                            return realBitmap;
                        })
                .when(mRoundedIconGenerator)
                .generateIconForUrl(eq(mGurl1));

        Callback<List<Drawable>> callback = mock(Callback.class);
        mBulkFaviconUtil.fetchAsDrawable(mContext, mProfile, Arrays.asList(mGurl1), 16, callback);

        // Verify that the default favicon is used.
        ArgumentCaptor<List<Drawable>> resultCaptor = ArgumentCaptor.forClass(List.class);
        verify(callback).onResult(resultCaptor.capture());
        assertEquals(1, resultCaptor.getValue().size());
        assertTrue(resultCaptor.getValue().get(0) != null);
    }
}
