// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.modules.readaloud.PlaybackArgs.PlaybackMode;

/** Unit tests for {@link NativeMetadata}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class NativeMetadataUnitTest {
    private static final String LANGUAGE = "en";
    private static final String CANONICAL_URL = "https://example.com/article";
    private static final PlaybackMode MODE = PlaybackMode.CLASSIC;

    @Test
    public void testConstructorAndInitialState() {
        NativeMetadata metadata = new NativeMetadata(LANGUAGE, CANONICAL_URL, MODE);

        assertEquals(LANGUAGE, metadata.languageCode());
        assertEquals(CANONICAL_URL, metadata.canonicalUrl());
        assertEquals(MODE, metadata.playbackMode());
        assertEquals("", metadata.title());
        assertEquals("", metadata.publisher());
    }

    @Test
    public void testNullInputsDefaultToSafeValues() {
        NativeMetadata metadata =
                new NativeMetadata(
                        /* languageCode= */ null,
                        /* canonicalUrl= */ null,
                        /* playbackMode= */ null);
        metadata.setTitle(null);
        metadata.setPublisher(null);

        assertEquals("", metadata.languageCode());
        assertEquals("", metadata.canonicalUrl());
        assertEquals(PlaybackMode.CLASSIC, metadata.playbackMode());
        assertEquals("", metadata.title());
        assertEquals("", metadata.publisher());
    }

    @Test
    public void testSetTitleAndPublisher() {
        NativeMetadata metadata = new NativeMetadata(LANGUAGE, CANONICAL_URL, MODE);

        metadata.setTitle("Article Title");
        metadata.setPublisher("Publisher Name");

        assertEquals("Article Title", metadata.title());
        assertEquals("Publisher Name", metadata.publisher());
    }

    @Test
    public void testDefaultFieldValues() {
        NativeMetadata metadata = new NativeMetadata(LANGUAGE, CANONICAL_URL, MODE);

        assertEquals("", metadata.author());
        assertEquals("", metadata.fullText());
        assertEquals(0, metadata.estimatedDurationSeconds());
        assertNotNull(metadata.paragraphs());
        assertEquals(0, metadata.paragraphs().length);
    }
}
