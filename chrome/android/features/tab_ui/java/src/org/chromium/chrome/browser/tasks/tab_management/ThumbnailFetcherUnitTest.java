// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.graphics.drawable.Drawable;
import android.util.Size;

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
import org.chromium.chrome.browser.tab_ui.ThumbnailProvider;

/** Unit tests for {@link ThumbnailFetcher}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ThumbnailFetcherUnitTest {
    private static final int TAB_ID = 123;
    private static final Size SIZE = new Size(378, 987);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ThumbnailProvider mThumbnailProvider;
    @Mock private Callback<Drawable> mCallback;
    @Mock private Callback<Drawable> mCallback2;
    @Mock private Drawable mDrawable;

    @Captor private ArgumentCaptor<Callback<Drawable>> mCallbackCaptor;

    @Test
    public void testFetch() {
        ThumbnailFetcher fetcher = new ThumbnailFetcher(mThumbnailProvider, TAB_ID);

        boolean isSelected = true;
        fetcher.fetch(SIZE, isSelected, mCallback);
        verify(mThumbnailProvider)
                .getTabThumbnailWithCallback(
                        eq(TAB_ID), eq(SIZE), eq(isSelected), mCallbackCaptor.capture());

        mCallbackCaptor.getValue().onResult(mDrawable);
        verify(mCallback).onResult(mDrawable);
    }

    @Test
    public void testCancel() {
        ThumbnailFetcher fetcher = new ThumbnailFetcher(mThumbnailProvider, TAB_ID);

        boolean isSelected = true;
        fetcher.fetch(SIZE, isSelected, mCallback);
        verify(mThumbnailProvider)
                .getTabThumbnailWithCallback(
                        eq(TAB_ID), eq(SIZE), eq(isSelected), mCallbackCaptor.capture());

        fetcher.cancel();

        mCallbackCaptor.getValue().onResult(mDrawable);
        verify(mCallback, never()).onResult(mDrawable);
    }

    @Test
    public void testDoubleFetchCancelsFirst() {
        ThumbnailFetcher fetcher = new ThumbnailFetcher(mThumbnailProvider, TAB_ID);

        boolean isSelected = true;
        fetcher.fetch(SIZE, isSelected, mCallback);
        verify(mThumbnailProvider)
                .getTabThumbnailWithCallback(
                        eq(TAB_ID), eq(SIZE), eq(isSelected), mCallbackCaptor.capture());

        isSelected = false;
        fetcher.fetch(SIZE, isSelected, mCallback2);
        verify(mThumbnailProvider)
                .getTabThumbnailWithCallback(
                        eq(TAB_ID), eq(SIZE), eq(isSelected), mCallbackCaptor.capture());

        for (var callback : mCallbackCaptor.getAllValues()) {
            callback.onResult(mDrawable);
        }
        verify(mCallback, never()).onResult(mDrawable);
        verify(mCallback2).onResult(mDrawable);
    }
}
