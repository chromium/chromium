// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.theme_collections;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

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
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link NtpThemeCollectionBridge}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class NtpThemeCollectionBridgeUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private NtpThemeCollectionBridge.Natives mNatives;
    @Mock private Profile mProfile;
    @Mock private Callback<List<BackgroundCollection>> mBackgroundCollectionsCallback;
    @Mock private Callback<List<CollectionImage>> mCollectionImagesCallback;
    @Mock private Callback<CustomBackgroundInfo> mOnThemeUpdatedCallback;

    @Captor private ArgumentCaptor<Callback<Object[]>> mObjectArrayCallbackCaptor;

    private NtpThemeCollectionBridge mNtpThemeCollectionBridge;

    @Before
    public void setUp() {
        NtpThemeCollectionBridgeJni.setInstanceForTesting(mNatives);
        when(mNatives.init(any(), any())).thenReturn(1L);
        mNtpThemeCollectionBridge = new NtpThemeCollectionBridge(mProfile, mOnThemeUpdatedCallback);
    }

    @Test
    public void testInitAndDestroy() {
        verify(mNatives).init(eq(mProfile), any(NtpThemeCollectionBridge.class));
        mNtpThemeCollectionBridge.destroy();
        verify(mNatives).destroy(1L);
    }

    @Test
    public void testGetBackgroundCollections() {
        mNtpThemeCollectionBridge.getBackgroundCollections(mBackgroundCollectionsCallback);
        verify(mNatives).getBackgroundCollections(eq(1L), mObjectArrayCallbackCaptor.capture());

        // Test with null response.
        mObjectArrayCallbackCaptor.getValue().onResult(null);
        verify(mBackgroundCollectionsCallback).onResult(null);

        // Test with empty array.
        Object[] collections = new Object[0];
        mObjectArrayCallbackCaptor.getValue().onResult(collections);
        verify(mBackgroundCollectionsCallback).onResult(eq(new ArrayList<>()));

        // Test with some items.
        BackgroundCollection collection1 =
                new BackgroundCollection("id1", "label1", JUnitTestGURLs.URL_1, 123);
        BackgroundCollection collection2 =
                new BackgroundCollection("id2", "label2", JUnitTestGURLs.URL_2, 456);
        Object[] collectionsWithData = new Object[] {collection1, collection2};
        mObjectArrayCallbackCaptor.getValue().onResult(collectionsWithData);
        verify(mBackgroundCollectionsCallback).onResult(eq(List.of(collection1, collection2)));
    }

    @Test
    public void testGetBackgroundImages() {
        String collectionId = "test_id";
        mNtpThemeCollectionBridge.getBackgroundImages(collectionId, mCollectionImagesCallback);
        verify(mNatives)
                .getBackgroundImages(
                        eq(1L), eq(collectionId), mObjectArrayCallbackCaptor.capture());

        // Test with null response.
        mObjectArrayCallbackCaptor.getValue().onResult(null);
        verify(mCollectionImagesCallback).onResult(null);

        // Test with empty array.
        Object[] images = new Object[0];
        mObjectArrayCallbackCaptor.getValue().onResult(images);
        verify(mCollectionImagesCallback).onResult(eq(new ArrayList<>()));

        // Test with some items.
        CollectionImage image1 =
                new CollectionImage(
                        "id1",
                        JUnitTestGURLs.URL_1,
                        JUnitTestGURLs.URL_1,
                        new ArrayList<>(),
                        JUnitTestGURLs.URL_1);
        CollectionImage image2 =
                new CollectionImage(
                        "id2",
                        JUnitTestGURLs.URL_2,
                        JUnitTestGURLs.URL_2,
                        new ArrayList<>(),
                        JUnitTestGURLs.URL_2);
        Object[] imagesWithData = new Object[] {image1, image2};
        mObjectArrayCallbackCaptor.getValue().onResult(imagesWithData);
        verify(mCollectionImagesCallback).onResult(eq(List.of(image1, image2)));
    }

    @Test
    public void testCreateCollection() {
        String id = "id";
        String label = "label";
        GURL url = JUnitTestGURLs.EXAMPLE_URL;
        int hash = 123; // Mock hash value for testing
        BackgroundCollection collection =
                NtpThemeCollectionBridge.createCollection(id, label, url, hash);
        assertEquals(id, collection.id);
        assertEquals(label, collection.label);
        assertEquals(url, collection.previewImageUrl);
    }

    @Test
    public void testCreateImage() {
        String collectionId = "collectionId";
        GURL imageUrl = JUnitTestGURLs.URL_1;
        GURL previewImageUrl = JUnitTestGURLs.URL_2;
        String[] attribution = new String[] {"foo", "bar"};
        GURL attributionUrl = JUnitTestGURLs.URL_3;
        CollectionImage image =
                NtpThemeCollectionBridge.createImage(
                        collectionId, imageUrl, previewImageUrl, attribution, attributionUrl);
        assertEquals(collectionId, image.collectionId);
        assertEquals(imageUrl, image.imageUrl);
        assertEquals(previewImageUrl, image.previewImageUrl);
        assertEquals(List.of("foo", "bar"), image.attribution);
        assertEquals(attributionUrl, image.attributionUrl);
    }

    @Test
    public void testSetThemeCollectionImage() {
        CollectionImage image =
                new CollectionImage(
                        "collectionId",
                        JUnitTestGURLs.URL_1,
                        JUnitTestGURLs.URL_2,
                        List.of("attr1", "attr2"),
                        JUnitTestGURLs.URL_3);
        mNtpThemeCollectionBridge.setThemeCollectionImage(image);
        verify(mNatives)
                .setThemeCollectionImage(
                        1L,
                        "collectionId",
                        JUnitTestGURLs.URL_1,
                        JUnitTestGURLs.URL_2,
                        "attr1",
                        "attr2",
                        JUnitTestGURLs.URL_3);
    }

    @Test
    public void testSelectLocalBackgroundImage() {
        mNtpThemeCollectionBridge.selectLocalBackgroundImage();
        verify(mNatives).selectLocalBackgroundImage(1L);
    }

    @Test
    public void testResetCustomBackground() {
        mNtpThemeCollectionBridge.resetCustomBackground();
        verify(mNatives).resetCustomBackground(1L);
    }

    @Test
    public void onCustomBackgroundImageUpdated() {
        CustomBackgroundInfo info =
                new CustomBackgroundInfo(JUnitTestGURLs.URL_1, "collection_id", false, true);
        when(mNatives.getCustomBackgroundInfo(1L)).thenReturn(info);

        mNtpThemeCollectionBridge.onCustomBackgroundImageUpdated();

        verify(mOnThemeUpdatedCallback).onResult(info);
    }

    @Test
    public void testOnCustomBackgroundImageUpdated_nullInfo() {
        when(mNatives.getCustomBackgroundInfo(1L)).thenReturn(null);
        mNtpThemeCollectionBridge.onCustomBackgroundImageUpdated();
        verify(mOnThemeUpdatedCallback).onResult(null);
    }

    @Test
    public void testSetThemeCollectionDailyRefreshed() {
        String collectionId = "test_id";
        mNtpThemeCollectionBridge.setThemeCollectionDailyRefreshed(collectionId);
        verify(mNatives).setThemeCollectionDailyRefreshed(eq(1L), eq(collectionId));
    }
}
