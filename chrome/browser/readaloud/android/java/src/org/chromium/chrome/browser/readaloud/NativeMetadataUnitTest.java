// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.modules.readaloud.Playback;
import org.chromium.chrome.modules.readaloud.Playback.PlaybackTextType;
import org.chromium.chrome.modules.readaloud.PlaybackArgs.PlaybackMode;

/** Unit tests for {@link NativeMetadata}. */
@RunWith(BaseRobolectricTestRunner.class)
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

    @Test
    public void testSetFullTextAndParagraphs() {
        NativeMetadata metadata = new NativeMetadata(LANGUAGE, CANONICAL_URL, MODE);
        Playback.PlaybackTextPart[] paragraphs =
                new Playback.PlaybackTextPart[] {
                    new NativeMetadata.TextPart(0, 0, 5, PlaybackTextType.TEXT_TYPE_NORMAL),
                    new NativeMetadata.TextPart(1, 5, 6, PlaybackTextType.TEXT_TYPE_NORMAL)
                };

        metadata.setFullText("Hello World");
        metadata.setParagraphs(paragraphs);

        assertEquals("Hello World", metadata.fullText());
        assertEquals(2, metadata.paragraphs().length);
        assertEquals(0, metadata.paragraphs()[0].getParagraphIndex());
        assertEquals(0, metadata.paragraphs()[0].getOffset());
        assertEquals(5, metadata.paragraphs()[0].getLength());
        assertEquals(PlaybackTextType.TEXT_TYPE_NORMAL, metadata.paragraphs()[0].getType());
        assertEquals(1, metadata.paragraphs()[1].getParagraphIndex());
        assertEquals(5, metadata.paragraphs()[1].getOffset());
        assertEquals(6, metadata.paragraphs()[1].getLength());
        assertEquals(PlaybackTextType.TEXT_TYPE_NORMAL, metadata.paragraphs()[1].getType());
    }

    @Test
    public void testSetFullTextAndParagraphs_nullInputsDefaultToSafeValues() {
        NativeMetadata metadata = new NativeMetadata(LANGUAGE, CANONICAL_URL, MODE);
        metadata.setFullText("Initial text");
        metadata.setParagraphs(
                new Playback.PlaybackTextPart[] {
                    new NativeMetadata.TextPart(0, 0, 12, PlaybackTextType.TEXT_TYPE_NORMAL)
                });

        metadata.setFullText(null);
        metadata.setParagraphs(null);

        assertEquals("", metadata.fullText());
        assertNotNull(metadata.paragraphs());
        assertEquals(0, metadata.paragraphs().length);
    }

    @Test
    public void testTextPartGetters() {
        NativeMetadata.TextPart part =
                new NativeMetadata.TextPart(3, 42, 100, PlaybackTextType.TEXT_TYPE_NORMAL);

        assertEquals(3, part.getParagraphIndex());
        assertEquals(42, part.getOffset());
        assertEquals(100, part.getLength());
        assertEquals(PlaybackTextType.TEXT_TYPE_NORMAL, part.getType());
    }

    @Test
    public void testSubstringSliceInvariant() {
        String chunk1 = "First paragraph. ";
        String chunk2 = "Second paragraph.";
        String fullText = chunk1 + chunk2;

        Playback.PlaybackTextPart[] parts =
                new Playback.PlaybackTextPart[] {
                    new NativeMetadata.TextPart(
                            0, 0, chunk1.length(), PlaybackTextType.TEXT_TYPE_NORMAL),
                    new NativeMetadata.TextPart(
                            1, chunk1.length(), chunk2.length(), PlaybackTextType.TEXT_TYPE_NORMAL)
                };

        NativeMetadata metadata = new NativeMetadata(LANGUAGE, CANONICAL_URL, MODE);
        metadata.setFullText(fullText);
        metadata.setParagraphs(parts);

        for (int i = 0; i < metadata.paragraphs().length; i++) {
            Playback.PlaybackTextPart p = metadata.paragraphs()[i];
            String slice =
                    metadata.fullText().substring(p.getOffset(), p.getOffset() + p.getLength());
            if (i == 0) {
                assertEquals(chunk1, slice);
            } else {
                assertEquals(chunk2, slice);
            }
        }
    }
}
