// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_image_service;

import static junit.framework.TestCase.assertTrue;

import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import androidx.test.filters.SmallTest;

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
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.page_image_service.mojom.ClientId;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link ImageServiceBridge}. */
@Batch(Batch.UNIT_TESTS)
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public final class ImageServiceBridgeUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public JniMocker mJniMocker = new JniMocker();

    private static final GURL PAGE_URL = JUnitTestGURLs.URL_1;
    private static final GURL SALIENT_IMAGE_URL = JUnitTestGURLs.URL_2;
    private static final int CLIENT_ID = ClientId.BOOKMARKS;

    @Mock private ImageServiceBridge.Natives mImageServiceBridgeJni;
    @Mock private Profile mProfile;
    @Mock private ImageFetcher mImageFetcher;
    @Mock private Callback<GURL> mUrlCallback;
    @Captor private ArgumentCaptor<Callback<GURL>> mUrlCallbackCaptor;

    private ImageServiceBridge mImageServiceBridge;

    @Before
    public void setUp() {
        mJniMocker.mock(ImageServiceBridgeJni.TEST_HOOKS, mImageServiceBridgeJni);

        mImageServiceBridge =
                new ImageServiceBridge(
                        CLIENT_ID,
                        ImageFetcher.POWER_BOOKMARKS_CLIENT_NAME,
                        mProfile,
                        mImageFetcher);
        verify(mImageServiceBridgeJni).init(eq(mProfile));
    }

    @Test
    @SmallTest
    public void testFetchImageUrlFor() {
        mImageServiceBridge.fetchImageUrlFor(/* isAccountData= */ true, PAGE_URL, mUrlCallback);

        verify(mImageServiceBridgeJni)
                .fetchImageUrlFor(
                        anyLong(),
                        eq(true),
                        eq(CLIENT_ID),
                        eq(PAGE_URL),
                        mUrlCallbackCaptor.capture());

        // Verifies the case that no salient image URL is found.
        mUrlCallbackCaptor.getValue().onResult(null);
        verify(mUrlCallback).onResult(isNull());

        // Verifies the case that a salient image URL if found.
        mUrlCallbackCaptor.getValue().onResult(SALIENT_IMAGE_URL);
        verify(mUrlCallback).onResult(eq(SALIENT_IMAGE_URL));
        assertTrue(mImageServiceBridge.isUrlCachedForTesting(PAGE_URL, SALIENT_IMAGE_URL));

        // Verifies that the cached salient image URL will be used immediately if exists.
        mImageServiceBridge.fetchImageUrlFor(/* isAccountData= */ true, PAGE_URL, mUrlCallback);
        verify(mUrlCallback, times(2)).onResult(eq(SALIENT_IMAGE_URL));
    }

    @Test
    @SmallTest
    public void testDestroy() {
        mImageServiceBridge.destroy();
        verify(mImageServiceBridgeJni).destroy(anyLong());
        verify(mImageFetcher).destroy();
    }

    @Test
    @SmallTest
    public void testClear() {
        mImageServiceBridge.clear();
        verify(mImageFetcher).clear();
    }
}
