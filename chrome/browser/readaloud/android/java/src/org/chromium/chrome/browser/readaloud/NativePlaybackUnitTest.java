// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertSame;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyFloat;
import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.anyLong;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.never;
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
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.modules.readaloud.Feedback.FeedbackType;
import org.chromium.chrome.modules.readaloud.Feedback.NegativeFeedbackReason;
import org.chromium.chrome.modules.readaloud.PlaybackArgs.PlaybackMode;
import org.chromium.chrome.modules.readaloud.PlaybackListener;
import org.chromium.chrome.modules.readaloud.ReadAloudPlaybackHooks.SendFeedbackCallback;
import org.chromium.content_public.browser.WebContents;

/** Unit tests for {@link NativePlayback}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class NativePlaybackUnitTest {
    private static final long NATIVE_PTR = 12345L;
    private static final String LANGUAGE = "en";
    private static final String CANONICAL_URL = "https://example.com/article";

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ReadAloudController.Natives mNativeMock;
    @Mock private WebContents mWebContents;
    @Mock private PlaybackListener mListener;
    @Mock private SendFeedbackCallback mFeedbackCallback;
    @Captor private ArgumentCaptor<PlaybackListener.PlaybackData> mDataCaptor;

    private NativePlayback mPlayback;

    @Before
    public void setUp() {
        ReadAloudControllerJni.setInstanceForTesting(mNativeMock);
        mPlayback =
                new NativePlayback(
                        NATIVE_PTR, mWebContents, LANGUAGE, CANONICAL_URL, PlaybackMode.CLASSIC);
    }

    @Test
    public void testInitialStateAndMetadata() {
        assertNotNull(mPlayback.getMetadata());
        assertEquals(LANGUAGE, mPlayback.getMetadata().languageCode());
        assertEquals(CANONICAL_URL, mPlayback.getMetadata().canonicalUrl());
        assertEquals(PlaybackMode.CLASSIC, mPlayback.getMetadata().playbackMode());
        assertEquals(PlaybackListener.State.BUFFERING, mPlayback.getState());
    }

    @Test
    public void testAddListenerNotifiesInitialData() {
        mPlayback.addListener(mListener);
        verify(mListener).onPlaybackDataChanged(mDataCaptor.capture());
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
        mPlayback.updateMetadata("Title", "Publisher");
        assertEquals("Title", mPlayback.getMetadata().title());
        assertEquals("Publisher", mPlayback.getMetadata().publisher());
    }

    @Test
    public void testPlay() {
        when(mWebContents.isDestroyed()).thenReturn(false);
        mPlayback.play();
        verify(mNativeMock).play(eq(NATIVE_PTR), eq(mWebContents));
    }

    @Test
    public void testPlay_destroyedWebContents() {
        when(mWebContents.isDestroyed()).thenReturn(true);
        mPlayback.play();
        verify(mNativeMock, never()).play(anyLong(), any());
    }

    @Test
    public void testNullConstructorInputs_safeFallback() {
        NativePlayback nullPlayback =
                new NativePlayback(
                        NATIVE_PTR,
                        /* webContents= */ null,
                        /* languageCode= */ null,
                        /* canonicalUrl= */ null,
                        /* playbackMode= */ null);

        assertEquals("", nullPlayback.getMetadata().languageCode());
        assertEquals(PlaybackMode.CLASSIC, nullPlayback.getMetadata().playbackMode());

        nullPlayback.play();
        verify(mNativeMock, never()).play(anyLong(), any());
    }

    @Test
    public void testPauseSeekRateAndRelease() {
        mPlayback.pause();
        verify(mNativeMock).pause(eq(NATIVE_PTR));

        mPlayback.seek(100L);
        verify(mNativeMock).seek(eq(NATIVE_PTR), eq(100L));

        mPlayback.seekRelative(50L);
        verify(mNativeMock).seekRelative(eq(NATIVE_PTR), eq(50L));

        mPlayback.setRate(1.5f);
        verify(mNativeMock).setPlaybackRate(eq(NATIVE_PTR), eq(1.5f));

        mPlayback.release();
        verify(mNativeMock).stop(eq(NATIVE_PTR));
    }

    @Test
    public void testSetNativeServicePtrToZero_preventsJniCalls() {
        mPlayback.setNativeServicePtr(0);
        when(mWebContents.isDestroyed()).thenReturn(false);

        mPlayback.play();
        mPlayback.pause();
        mPlayback.seek(100L);
        mPlayback.seekRelative(50L);
        mPlayback.setRate(1.5f);
        mPlayback.release();
        mPlayback.sendFeedback(
                FeedbackType.POSITIVE, NegativeFeedbackReason.OTHER, mFeedbackCallback);

        verify(mNativeMock, never()).play(anyLong(), any());
        verify(mNativeMock, never()).pause(anyLong());
        verify(mNativeMock, never()).seek(anyLong(), anyLong());
        verify(mNativeMock, never()).seekRelative(anyLong(), anyLong());
        verify(mNativeMock, never()).setPlaybackRate(anyLong(), anyFloat());
        verify(mNativeMock, never()).stop(anyLong());
        verify(mNativeMock, never()).sendFeedback(anyLong(), anyInt());
        verify(mFeedbackCallback).onFailure(any(Exception.class));
    }

    @Test
    public void testSendFeedback() {
        mPlayback.sendFeedback(
                FeedbackType.POSITIVE, NegativeFeedbackReason.OTHER, mFeedbackCallback);
        verify(mNativeMock).sendFeedback(eq(NATIVE_PTR), eq(FeedbackType.POSITIVE.getValue()));
        verify(mFeedbackCallback).onSuccess();
    }
}
