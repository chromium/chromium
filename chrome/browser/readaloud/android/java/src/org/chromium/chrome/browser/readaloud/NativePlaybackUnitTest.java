// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertSame;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
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

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.modules.readaloud.Feedback.FeedbackType;
import org.chromium.chrome.modules.readaloud.Feedback.NegativeFeedbackReason;
import org.chromium.chrome.modules.readaloud.Playback;
import org.chromium.chrome.modules.readaloud.Playback.PlaybackTextType;
import org.chromium.chrome.modules.readaloud.PlaybackArgs.PlaybackMode;
import org.chromium.chrome.modules.readaloud.PlaybackListener;
import org.chromium.chrome.modules.readaloud.ReadAloudPlaybackHooks.SendFeedbackCallback;
import org.chromium.content_public.browser.WebContents;

/** Unit tests for {@link NativePlayback}. */
@RunWith(BaseRobolectricTestRunner.class)
public class NativePlaybackUnitTest {
    private static final String LANGUAGE = "en";
    private static final String CANONICAL_URL = "https://example.com/article";

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ReadAloudNativeBridge mBridgeMock;
    @Mock private WebContents mWebContents;
    @Mock private PlaybackListener mListener;
    @Mock private SendFeedbackCallback mFeedbackCallback;
    @Captor private ArgumentCaptor<PlaybackListener.PlaybackData> mDataCaptor;

    private NativePlayback mPlayback;

    @Before
    public void setUp() {
        when(mBridgeMock.isInitialized()).thenReturn(true);
        mPlayback =
                new NativePlayback(
                        mBridgeMock, mWebContents, LANGUAGE, CANONICAL_URL, PlaybackMode.CLASSIC);
    }

    @Test
    public void testInitialStateAndMetadata() {
        assertNotNull(mPlayback.getMetadata());
        assertEquals(LANGUAGE, mPlayback.getMetadata().languageCode());
        assertEquals(CANONICAL_URL, mPlayback.getMetadata().canonicalUrl());
        assertEquals(PlaybackMode.CLASSIC, mPlayback.getMetadata().playbackMode());
        assertEquals(PlaybackListener.State.BUFFERING, mPlayback.getState());
        verify(mBridgeMock).setPlaybackMode(PlaybackMode.CLASSIC.getValue());
        mPlayback.initializeSession();
        verify(mBridgeMock).initializeSession(mWebContents);
    }

    @Test
    public void testConstructor_setsOverviewPlaybackModeOnBridge() {
        NativePlayback playback =
                new NativePlayback(
                        mBridgeMock, mWebContents, LANGUAGE, CANONICAL_URL, PlaybackMode.OVERVIEW);
        verify(mBridgeMock).setPlaybackMode(PlaybackMode.OVERVIEW.getValue());
        assertEquals(PlaybackMode.OVERVIEW, playback.getMetadata().playbackMode());
    }

    @Test
    public void testAddListenerNotifiesInitialData() {
        mPlayback.addListener(mListener);
        verify(mListener).onPlaybackDataChanged(mDataCaptor.capture());
        verify(mListener).onMetadataChanged(eq(mPlayback.getMetadata()));
        assertEquals(PlaybackListener.State.BUFFERING, mDataCaptor.getValue().state());
    }

    @Test
    public void testRemoveListener() {
        mPlayback.addListener(mListener);
        verify(mListener).onPlaybackDataChanged(any());

        mPlayback.removeListener(mListener);
        mPlayback.notifyPlaybackStateChanged(PlaybackListener.State.PLAYING);

        verify(mListener, times(1)).onPlaybackDataChanged(any());
    }

    @Test
    public void testZeroAllocationProgressUpdates() {
        mPlayback.addListener(mListener);
        verify(mListener).onPlaybackDataChanged(mDataCaptor.capture());
        PlaybackListener.PlaybackData initialData = mDataCaptor.getValue();

        mPlayback.notifyPlaybackProgressUpdated(1000L, 5000L);
        verify(mListener, times(2)).onPlaybackDataChanged(mDataCaptor.capture());
        PlaybackListener.PlaybackData updatedData = mDataCaptor.getValue();

        assertSame(initialData, updatedData);
        assertEquals(1000L, updatedData.absolutePositionNanos());
        assertEquals(1000L, updatedData.positionInParagraphNanos());
        assertEquals(5000L, updatedData.totalDurationNanos());
        assertEquals(5000L, updatedData.paragraphDurationNanos());
    }

    @Test
    public void testNotifyPlaybackStateChanged() {
        mPlayback.addListener(mListener);
        mPlayback.notifyPlaybackStateChanged(PlaybackListener.State.PLAYING);
        assertEquals(PlaybackListener.State.PLAYING, mPlayback.getState());
        verify(mListener, times(2)).onPlaybackDataChanged(any());
    }

    @Test
    public void testUpdateMetadata() {
        mPlayback.addListener(mListener);
        reset(mListener);
        mPlayback.updateMetadata("Title", "Publisher");
        assertEquals("Title", mPlayback.getMetadata().title());
        assertEquals("Publisher", mPlayback.getMetadata().publisher());
        verify(mListener).onMetadataChanged(mPlayback.getMetadata());
    }

    @Test
    public void testPlay() {
        mPlayback.play();
        verify(mBridgeMock).play(mWebContents);
    }

    @Test
    public void testNullConstructorInputs_safeFallback() {
        NativePlayback nullPlayback =
                new NativePlayback(
                        mBridgeMock,
                        /* webContents= */ null,
                        /* languageCode= */ null,
                        /* canonicalUrl= */ null,
                        /* playbackMode= */ null);

        assertEquals("", nullPlayback.getMetadata().languageCode());
        assertEquals(PlaybackMode.CLASSIC, nullPlayback.getMetadata().playbackMode());

        nullPlayback.play();
        verify(mBridgeMock).play(null);
    }

    @Test
    public void testPauseSeekRateAndRelease() {
        mPlayback.pause();
        verify(mBridgeMock).pause();

        mPlayback.seek(100L);
        verify(mBridgeMock).seek(100L);

        mPlayback.seekRelative(50L);
        verify(mBridgeMock).seekRelative(50L);

        mPlayback.setRate(1.5f);
        verify(mBridgeMock).setPlaybackRate(1.5f);

        mPlayback.release();
        verify(mBridgeMock).stop();
    }

    @Test
    public void testUninitializedBridge_failsFeedback() {
        when(mBridgeMock.isInitialized()).thenReturn(false);

        mPlayback.sendFeedback(
                FeedbackType.POSITIVE, NegativeFeedbackReason.OTHER, mFeedbackCallback);

        verify(mFeedbackCallback).onFailure(any(Exception.class));
    }

    @Test
    public void testSendFeedback() {
        mPlayback.sendFeedback(
                FeedbackType.POSITIVE, NegativeFeedbackReason.OTHER, mFeedbackCallback);
        verify(mBridgeMock).sendFeedback(FeedbackType.POSITIVE.getValue());
        verify(mFeedbackCallback).onSuccess();
    }

    @Test
    public void testOnTextChunked_populatesMetadataAndNotifiesListeners() {
        mPlayback.addListener(mListener);
        reset(mListener);

        String[] chunks = new String[] {"First chunk.", "Second chunk.", "Third chunk."};
        mPlayback.onTextChunked(chunks);

        Playback.Metadata metadata = mPlayback.getMetadata();
        assertEquals("First chunk. Second chunk. Third chunk.", metadata.fullText());
        assertEquals(3, metadata.paragraphs().length);

        assertEquals(0, metadata.paragraphs()[0].getParagraphIndex());
        assertEquals(0, metadata.paragraphs()[0].getOffset());
        assertEquals(12, metadata.paragraphs()[0].getLength());
        assertEquals(PlaybackTextType.TEXT_TYPE_NORMAL, metadata.paragraphs()[0].getType());

        assertEquals(1, metadata.paragraphs()[1].getParagraphIndex());
        assertEquals(13, metadata.paragraphs()[1].getOffset());
        assertEquals(13, metadata.paragraphs()[1].getLength());
        assertEquals(PlaybackTextType.TEXT_TYPE_NORMAL, metadata.paragraphs()[1].getType());

        assertEquals(2, metadata.paragraphs()[2].getParagraphIndex());
        assertEquals(27, metadata.paragraphs()[2].getOffset());
        assertEquals(12, metadata.paragraphs()[2].getLength());
        assertEquals(PlaybackTextType.TEXT_TYPE_NORMAL, metadata.paragraphs()[2].getType());

        verify(mListener).onMetadataChanged(metadata);
    }

    @Test
    public void testOnTextChunked_emptyArray() {
        mPlayback.addListener(mListener);
        reset(mListener);

        mPlayback.onTextChunked(new String[0]);

        Playback.Metadata metadata = mPlayback.getMetadata();
        assertEquals("", metadata.fullText());
        assertEquals(0, metadata.paragraphs().length);
        verify(mListener).onMetadataChanged(metadata);
    }

    @Test
    public void testOnTextChunked_sequentialCallsOverwriteCleanly() {
        mPlayback.addListener(mListener);

        mPlayback.onTextChunked(new String[] {"Initial", "content"});
        assertEquals("Initial content", mPlayback.getMetadata().fullText());
        assertEquals(2, mPlayback.getMetadata().paragraphs().length);
        assertEquals(0, mPlayback.getMetadata().paragraphs()[0].getOffset());
        assertEquals(7, mPlayback.getMetadata().paragraphs()[0].getLength());
        assertEquals(8, mPlayback.getMetadata().paragraphs()[1].getOffset());
        assertEquals(7, mPlayback.getMetadata().paragraphs()[1].getLength());

        reset(mListener);
        mPlayback.onTextChunked(new String[] {"Replacement"});
        assertEquals("Replacement", mPlayback.getMetadata().fullText());
        assertEquals(1, mPlayback.getMetadata().paragraphs().length);
        assertEquals(0, mPlayback.getMetadata().paragraphs()[0].getParagraphIndex());
        assertEquals(0, mPlayback.getMetadata().paragraphs()[0].getOffset());
        assertEquals(11, mPlayback.getMetadata().paragraphs()[0].getLength());
        verify(mListener).onMetadataChanged(mPlayback.getMetadata());
    }

    @Test
    public void testOnTextChunked_withEmptyChunkStrings() {
        mPlayback.addListener(mListener);
        reset(mListener);

        String[] chunks = new String[] {"First", "", "Third"};
        mPlayback.onTextChunked(chunks);

        Playback.Metadata metadata = mPlayback.getMetadata();
        assertEquals("First Third", metadata.fullText());
        assertEquals(3, metadata.paragraphs().length);

        assertEquals(0, metadata.paragraphs()[0].getParagraphIndex());
        assertEquals(0, metadata.paragraphs()[0].getOffset());
        assertEquals(5, metadata.paragraphs()[0].getLength());

        assertEquals(1, metadata.paragraphs()[1].getParagraphIndex());
        assertEquals(5, metadata.paragraphs()[1].getOffset());
        assertEquals(0, metadata.paragraphs()[1].getLength());

        assertEquals(2, metadata.paragraphs()[2].getParagraphIndex());
        assertEquals(6, metadata.paragraphs()[2].getOffset());
        assertEquals(5, metadata.paragraphs()[2].getLength());

        verify(mListener).onMetadataChanged(metadata);
    }
}
