// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

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
import org.chromium.chrome.modules.readaloud.PlaybackListener;

/** Unit tests for {@link NativeVoicePreviewPlayback}. */
@RunWith(BaseRobolectricTestRunner.class)
public class NativeVoicePreviewPlaybackUnitTest {
    private static final String VOICE_ID = "voice_ruby";
    private static final String LANGUAGE_CODE = "en";

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ReadAloudNativeBridge mBridgeMock;
    @Mock private PlaybackListener mListener;
    @Captor private ArgumentCaptor<PlaybackListener.PlaybackData> mDataCaptor;

    private NativeVoicePreviewPlayback mPlayback;

    @Before
    public void setUp() {
        mPlayback = new NativeVoicePreviewPlayback(mBridgeMock, LANGUAGE_CODE, VOICE_ID);
    }

    @Test
    public void testInitialStateAndMetadata() {
        assertNotNull(mPlayback.getMetadata());
        assertEquals(LANGUAGE_CODE, mPlayback.getMetadata().languageCode());
        assertEquals(VOICE_ID, mPlayback.getVoiceId());
        assertEquals(PlaybackListener.State.BUFFERING, mPlayback.getState());
    }

    @Test
    public void testAddListenerRegistersObserverWithoutImmediateCallback() {
        mPlayback.addListener(mListener);
        verify(mListener, never()).onPlaybackDataChanged(any());
        verify(mListener, never()).onMetadataChanged(any());
    }

    @Test
    public void testRemoveListener() {
        mPlayback.addListener(mListener);
        mPlayback.removeListener(mListener);
        mPlayback.notifyPlaybackStateChanged(PlaybackListener.State.PLAYING);

        verify(mListener, never()).onPlaybackDataChanged(any());
    }

    @Test
    public void testNotifyPlaybackStateChanged() {
        mPlayback.addListener(mListener);
        mPlayback.notifyPlaybackStateChanged(PlaybackListener.State.PLAYING);
        assertEquals(PlaybackListener.State.PLAYING, mPlayback.getState());
        verify(mListener, times(1)).onPlaybackDataChanged(mDataCaptor.capture());
        assertEquals(PlaybackListener.State.PLAYING, mDataCaptor.getValue().state());
    }

    @Test
    public void testPlay() {
        mPlayback.play();
        verify(mBridgeMock).previewVoice(VOICE_ID);
    }

    @Test
    public void testPause() {
        mPlayback.pause();
        verify(mBridgeMock).stopVoicePreview();
    }

    @Test
    public void testRelease() {
        mPlayback.release();
        verify(mBridgeMock).stopVoicePreview();
    }
}
