// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.readaloud.player;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.modules.readaloud.Playback;
import org.chromium.chrome.modules.readaloud.PlaybackListener;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link PlayerMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PlayerMediatorUnitTest {
    private static final String TITLE = "Title";
    private static final String PUBLISHER = "Publisher";
    private static final long POSITION_NS = 1_000_000_000L; // one second
    private static final long DURATION_NS = 10_000_000_000L; // ten seconds

    @Mock
    private PlayerCoordinator mPlayerCoordinator;
    @Mock
    private Playback mPlayback;
    @Mock
    private Playback.Metadata mPlaybackMetadata;

    @Captor
    private ArgumentCaptor<PlaybackListener> mPlaybackListenerCaptor;

    private PropertyModel mModel;

    private static class TestPlaybackData implements PlaybackListener.PlaybackData {
        public int mState;
        public int mParagraphIndex;
        public long mPositionInParagraphNanos;
        public long mParagraphDurationNanos;
        public long mAbsolutePositionNanos;
        public long mTotalDurationNanos;

        @Override
        @PlaybackListener.State
        public int state() {
            return mState;
        }

        @Override
        public int paragraphIndex() {
            return mParagraphIndex;
        }

        @Override
        public long positionInParagraphNanos() {
            return mPositionInParagraphNanos;
        }

        @Override
        public long paragraphDurationNanos() {
            return mParagraphDurationNanos;
        }

        @Override
        public long absolutePositionNanos() {
            return mAbsolutePositionNanos;
        }

        @Override
        public long totalDurationNanos() {
            return mTotalDurationNanos;
        }
    }
    private TestPlaybackData mPlaybackData;

    private PlayerMediator mMediator;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        resetPlayback();
        doReturn(TITLE).when(mPlaybackMetadata).title();
        doReturn(PUBLISHER).when(mPlaybackMetadata).publisher();
        mPlaybackData = new TestPlaybackData();
        mModel = new PropertyModel.Builder(PlayerProperties.ALL_KEYS).build();
        mMediator = new PlayerMediator(mPlayerCoordinator, mModel);
    }

    @Test
    public void testInitialState() {
        // Constructor should do this.
        assertEquals(mMediator, mModel.get(PlayerProperties.INTERACTION_HANDLER));
    }

    @Test
    public void testSetPlayback() {
        mMediator.setPlayback(mPlayback);

        verify(mPlayback).addListener(mPlaybackListenerCaptor.capture());
        assertEquals(TITLE, mModel.get(PlayerProperties.TITLE));
        assertEquals(PUBLISHER, mModel.get(PlayerProperties.PUBLISHER));
    }

    @Test
    public void testSetPlayback_thenNull() {
        mMediator.setPlayback(mPlayback);
        verify(mPlayback).addListener(mPlaybackListenerCaptor.capture());
        mMediator.setPlayback(null);
        verify(mPlayback).removeListener(eq(mPlaybackListenerCaptor.getValue()));
    }

    @Test
    public void testSetPlayback_thenSetAgain() {
        mMediator.setPlayback(mPlayback);

        verify(mPlayback).addListener(mPlaybackListenerCaptor.capture());
        assertEquals(TITLE, mModel.get(PlayerProperties.TITLE));
        assertEquals(PUBLISHER, mModel.get(PlayerProperties.PUBLISHER));

        resetPlayback();
        mModel.set(PlayerProperties.TITLE, "");
        mModel.set(PlayerProperties.PUBLISHER, "");

        mMediator.setPlayback(mPlayback);
        verify(mPlayback).removeListener(eq(mPlaybackListenerCaptor.getValue()));
        verify(mPlayback).addListener(eq(mPlaybackListenerCaptor.getValue()));
        assertEquals(TITLE, mModel.get(PlayerProperties.TITLE));
        assertEquals(PUBLISHER, mModel.get(PlayerProperties.PUBLISHER));
    }

    @Test
    public void testSetPlaybackState() {
        mMediator.setPlaybackState(PlaybackListener.State.PLAYING);
        assertEquals(
                PlaybackListener.State.PLAYING, (int) mModel.get(PlayerProperties.PLAYBACK_STATE));
    }

    @Test
    public void testProgressUpdated() {
        mMediator.setPlayback(mPlayback);
        verify(mPlayback).addListener(mPlaybackListenerCaptor.capture());

        mPlaybackData.mAbsolutePositionNanos = POSITION_NS;
        mPlaybackData.mTotalDurationNanos = DURATION_NS;
        mPlaybackListenerCaptor.getValue().onPlaybackDataChanged(mPlaybackData);

        assertEquals(0.1f, (float) mModel.get(PlayerProperties.PROGRESS), /*delta=*/1e-8f);
    }

    @Test
    public void testPlaybackStateUpdated() {
        mMediator.setPlayback(mPlayback);
        verify(mPlayback).addListener(mPlaybackListenerCaptor.capture());

        mPlaybackData.mState = PlaybackListener.State.PLAYING;
        mPlaybackListenerCaptor.getValue().onPlaybackDataChanged(mPlaybackData);

        assertEquals(
                PlaybackListener.State.PLAYING, (int) mModel.get(PlayerProperties.PLAYBACK_STATE));
    }

    private void resetPlayback() {
        reset(mPlayback);
        doReturn(mPlaybackMetadata).when(mPlayback).getMetadata();
    }
}
