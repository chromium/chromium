// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ntp_customization.theme.NtpThemeBridge.ThemeCollectionSelectionListener;
import org.chromium.chrome.browser.ntp_customization.theme.theme_collections.BackgroundCollection;
import org.chromium.chrome.browser.ntp_customization.theme.theme_collections.CollectionImage;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link NtpThemeBridge}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class NtpThemeBridgeUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private NtpThemeBridge.Natives mNatives;
    @Mock private Profile mProfile;
    @Mock private Callback<List<BackgroundCollection>> mBackgroundCollectionsCallback;
    @Mock private Callback<List<CollectionImage>> mCollectionImagesCallback;
    @Mock private ThemeCollectionSelectionListener mListener;
    @Captor private ArgumentCaptor<Callback<Object[]>> mObjectArrayCallbackCaptor;

    private NtpThemeBridge mNtpThemeBridge;

    @Before
    public void setUp() {
        NtpThemeBridgeJni.setInstanceForTesting(mNatives);
        when(mNatives.init(any())).thenReturn(1L);
        mNtpThemeBridge = new NtpThemeBridge(mProfile);
    }

    @Test
    public void testInitAndDestroy() {
        verify(mNatives).init(mProfile);
        mNtpThemeBridge.destroy();
        verify(mNatives).destroy(1L);
    }

    @Test
    public void testGetBackgroundCollections() {
        mNtpThemeBridge.getBackgroundCollections(mBackgroundCollectionsCallback);
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
                new BackgroundCollection("id1", "label1", JUnitTestGURLs.URL_1);
        BackgroundCollection collection2 =
                new BackgroundCollection("id2", "label2", JUnitTestGURLs.URL_2);
        Object[] collectionsWithData = new Object[] {collection1, collection2};
        mObjectArrayCallbackCaptor.getValue().onResult(collectionsWithData);
        verify(mBackgroundCollectionsCallback).onResult(eq(List.of(collection1, collection2)));
    }

    @Test
    public void testGetBackgroundImages() {
        String collectionId = "test_id";
        mNtpThemeBridge.getBackgroundImages(collectionId, mCollectionImagesCallback);
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
        BackgroundCollection collection = NtpThemeBridge.createCollection(id, label, url);
        Assert.assertEquals(id, collection.id);
        Assert.assertEquals(label, collection.label);
        Assert.assertEquals(url, collection.previewImageUrl);
    }

    @Test
    public void testCreateImage() {
        String collectionId = "collectionId";
        GURL imageUrl = JUnitTestGURLs.URL_1;
        GURL previewImageUrl = JUnitTestGURLs.URL_2;
        String[] attribution = new String[] {"foo", "bar"};
        GURL attributionUrl = JUnitTestGURLs.URL_3;
        CollectionImage image =
                NtpThemeBridge.createImage(
                        collectionId, imageUrl, previewImageUrl, attribution, attributionUrl);
        Assert.assertEquals(collectionId, image.collectionId);
        Assert.assertEquals(imageUrl, image.imageUrl);
        Assert.assertEquals(previewImageUrl, image.previewImageUrl);
        Assert.assertEquals(List.of("foo", "bar"), image.attribution);
        Assert.assertEquals(attributionUrl, image.attributionUrl);
    }

    @Test
    public void testThemeSelection() {
        // Initially, selection is null.
        Assert.assertNull(mNtpThemeBridge.getSelectedThemeCollectionId());
        Assert.assertNull(mNtpThemeBridge.getSelectedThemeCollectionImageUrl());

        // Add a listener.
        mNtpThemeBridge.addListener(mListener);

        // Set a theme.
        String collectionId = "test_id";
        GURL imageUrl = JUnitTestGURLs.URL_1;
        mNtpThemeBridge.setSelectedTheme(collectionId, imageUrl);

        // Verify getters and listener callback.
        Assert.assertEquals(collectionId, mNtpThemeBridge.getSelectedThemeCollectionId());
        Assert.assertEquals(imageUrl, mNtpThemeBridge.getSelectedThemeCollectionImageUrl());
        verify(mListener).onThemeCollectionSelectionChanged(eq(collectionId), eq(imageUrl));

        // Remove the listener.
        mNtpThemeBridge.removeListener(mListener);
        Mockito.clearInvocations(mListener);

        // Set a different theme.
        mNtpThemeBridge.setSelectedTheme("id2", JUnitTestGURLs.URL_2);

        // Verify the listener was not called again.
        verify(mListener, never()).onThemeCollectionSelectionChanged(any(), any());
    }
}
