// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.readaloud.player;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.anyLong;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.verify;

import android.widget.SeekBar;
import android.widget.SeekBar.OnSeekBarChangeListener;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.readaloud.ReadAloudPrefs;
import org.chromium.chrome.browser.readaloud.ReadAloudPrefsJni;
import org.chromium.chrome.browser.readaloud.testing.MockPrefServiceHelper;
import org.chromium.chrome.modules.readaloud.Playback;
import org.chromium.chrome.modules.readaloud.PlaybackArgs.PlaybackVoice;
import org.chromium.chrome.modules.readaloud.PlaybackListener;
import org.chromium.chrome.modules.readaloud.Player;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;
import java.util.Map;

/** Unit tests for {@link PlayerMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PlayerMediatorUnitTest {
    private static final String TITLE = "Title";
    private static final String PUBLISHER = "Publisher";
    private static final long POSITION_NS = 1_000_000_000L; // one second
    private static final long DURATION_NS = 10_000_000_000L; // ten seconds

    @Rule public JniMocker mJniMocker = new JniMocker();
    @Mock ReadAloudPrefs.Natives mPrefsNative;
    @Mock private PlayerCoordinator mPlayerCoordinator;
    @Mock private Playback mPlayback;
    @Mock private Playback.Metadata mPlaybackMetadata;
    @Mock private SeekBar mSeekbar;
    private MockPrefServiceHelper mMockPrefServiceHelper;
    private OnSeekBarChangeListener mOnSeekBarChangeListener;

    private ObservableSupplierImpl<List<PlaybackVoice>> mVoicesSupplier;
    private ObservableSupplierImpl<String> mSelectedVoiceIdSupplier;
    private ObservableSupplierImpl<Boolean> mHighlightingEnabledSupplier;
    @Captor private ArgumentCaptor<PlaybackListener> mPlaybackListenerCaptor;

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
    @Mock private Player.Delegate mDelegate;

    private PlayerMediator mMediator;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        resetPlayback();
        doReturn(TITLE).when(mPlaybackMetadata).title();
        doReturn(PUBLISHER).when(mPlaybackMetadata).publisher();
        mVoicesSupplier = new ObservableSupplierImpl<>();
        mVoicesSupplier.set(List.of(new PlaybackVoice("en", "a", "description")));
        mSelectedVoiceIdSupplier = new ObservableSupplierImpl<>();
        mSelectedVoiceIdSupplier.set("a");
        mHighlightingEnabledSupplier = new ObservableSupplierImpl<>();
        mHighlightingEnabledSupplier.set(true);
        mJniMocker.mock(ReadAloudPrefsJni.TEST_HOOKS, mPrefsNative);
        mMockPrefServiceHelper = new MockPrefServiceHelper();
        mPlaybackData = new TestPlaybackData();

        doReturn(true).when(mDelegate).isHighlightingSupported();
        doReturn(mHighlightingEnabledSupplier).when(mDelegate).getHighlightingEnabledSupplier();
        doReturn(mVoicesSupplier).when(mDelegate).getCurrentLanguageVoicesSupplier();
        doReturn(mSelectedVoiceIdSupplier).when(mDelegate).getVoiceIdSupplier();
        doReturn(mMockPrefServiceHelper.getPrefService()).when(mDelegate).getPrefService();

        mModel = new PropertyModel.Builder(PlayerProperties.ALL_KEYS).build();
        mMediator = new PlayerMediator(mPlayerCoordinator, mDelegate, mModel);
        mOnSeekBarChangeListener = mMediator.getSeekBarChangeListener();
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
        assertEquals(true, mModel.get(PlayerProperties.HIGHLIGHTING_SUPPORTED));
        assertEquals(true, mModel.get(PlayerProperties.HIGHLIGHTING_ENABLED));
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

        assertEquals(0.1f, (float) mModel.get(PlayerProperties.PROGRESS), /* delta= */ 1e-8f);
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

    @Test
    public void testPlayClicked() {
        mMediator.setPlayback(mPlayback);
        mMediator.setPlaybackState(PlaybackListener.State.PAUSED);

        mMediator.onPlayPauseClick();
        verify(mPlayback).play();

        Mockito.reset(mPlayback);
        mMediator.setPlaybackState(PlaybackListener.State.STOPPED);
        mMediator.onPlayPauseClick();
        verify(mPlayback).seek(0L);
        verify(mPlayback).play();
    }

    @Test
    public void testPlayClicked_fromStopped() {
        mMediator.setPlayback(mPlayback);
        mMediator.setPlaybackState(PlaybackListener.State.STOPPED);

        mMediator.onPlayPauseClick();
        verify(mPlayback).play();
    }

    @Test
    public void testPauseClicked() {
        mMediator.setPlayback(mPlayback);
        mMediator.setPlaybackState(PlaybackListener.State.PLAYING);

        mMediator.onPlayPauseClick();
        verify(mPlayback).pause();
    }

    @Test
    public void testCloseClicked() {
        mMediator.onCloseClick();
        verify(mPlayerCoordinator).closeClicked();
    }

    @Test
    public void testOnSeekBack() {
        // make sure nothing happens if playback hasn't been set yet
        mMediator.setPlayback(null);
        mMediator.onSeekBackClick();
        verify(mPlayback, never()).seekRelative(anyLong());

        mModel.set(PlayerProperties.ELAPSED_NANOS, 0L);
        mModel.set(PlayerProperties.DURATION_NANOS, 40 * 1_000_000_000L);
        mMediator.setPlayback(mPlayback);
        mMediator.onSeekBackClick();
        verify(mPlayback).seekRelative(-10 * 1_000_000_000L);
    }

    @Test
    public void testOnSeekForward() {
        mMediator.setPlayback(mPlayback);
        mModel.set(PlayerProperties.ELAPSED_NANOS, 0L);
        mModel.set(PlayerProperties.DURATION_NANOS, 40 * 1_000_000_000L);

        mMediator.onSeekForwardClick();
        verify(mPlayback).seekRelative(30 * 1_000_000_000L);
    }

    @Test
    public void testOnSeekForwardPastEnd() {
        // Set playback duration shorter to test the pause at end when seeking beyond duration
        mMediator.setPlayback(mPlayback);
        mModel.set(PlayerProperties.ELAPSED_NANOS, 0L);
        mModel.set(PlayerProperties.DURATION_NANOS, 1L);
        mMediator.onSeekForwardClick();
        verify(mPlayback).pause();
        verify(mPlayback).seek(1L);
    }

    @Test
    public void testOnSpeedChange() {
        mMediator.setPlayback(mPlayback);
        mMediator.onSpeedChange(0.5f);
        verify(mPlayback).setRate(0.5f);
        mMediator.onSpeedChange(2f);
        assertEquals(2f, ReadAloudPrefs.getSpeed(mDelegate.getPrefService()), /* delta= */ 0f);
        assertEquals(2f, mModel.get(PlayerProperties.SPEED), /* delta= */ 0f);
    }

    @Test
    public void testOnMiniPlayerExpandClick() {
        mMediator.onMiniPlayerExpandClick();
        verify(mPlayerCoordinator).expand();
    }

    @Test
    public void testOnVoiceSelected() {
        PlaybackVoice voice = new PlaybackVoice("language", "voice", "description");
        mMediator.onVoiceSelected(voice);
        verify(mDelegate).setVoiceOverrideAndApplyToPlayback(eq(voice));
    }

    @Test
    public void testOnHighlightingChanged() {
        assertTrue(mHighlightingEnabledSupplier.get());

        mMediator.onHighlightingChange(false);
        assertFalse(mHighlightingEnabledSupplier.get());

        mMediator.onHighlightingChange(true);
        assertTrue(mHighlightingEnabledSupplier.get());
    }

    @Test
    public void testOnProgressChanged() {
        mMediator.setPlayback(mPlayback);
        mModel.set(PlayerProperties.DURATION_NANOS, 100L);
        // if not from user, make sure doesn't seek playback
        mOnSeekBarChangeListener.onProgressChanged(mSeekbar, 20, false);
        verify(mPlayback, never()).seek(anyLong());

        // from user, so should seek
        mOnSeekBarChangeListener.onProgressChanged(mSeekbar, 20, true);
        verify(mPlayback).seek(anyLong());
    }

    @Test
    public void testObserveVoiceList() {
        // Set up a Spanish voice pref that isn't the first in the list.
        MockPrefServiceHelper.setVoices(mPrefsNative, Map.of("es", "c"));

        // Setting the voice list here should trigger UI updates.
        mVoicesSupplier.set(
                List.of(new PlaybackVoice("es", "b", ""), new PlaybackVoice("es", "c", "")));

        // Voice list is set in model.
        List<PlaybackVoice> voicesInModel = mModel.get(PlayerProperties.VOICES_LIST);
        assertNotNull(voicesInModel);
        assertEquals(2, voicesInModel.size());
        assertEquals("es", voicesInModel.get(0).getLanguage());
        assertEquals("b", voicesInModel.get(0).getVoiceId());
        assertEquals("es", voicesInModel.get(1).getLanguage());
        assertEquals("c", voicesInModel.get(1).getVoiceId());

        // Current language voice selection should be applied.
        assertEquals("c", mModel.get(PlayerProperties.SELECTED_VOICE_ID));
    }

    @Test
    public void testObserveVoiceList_defaultVoiceSelection() {
        // Setting the voice list here should trigger UI updates. There's no voice ID
        // set for "es" so the first one should be displayed.
        mVoicesSupplier.set(
                List.of(new PlaybackVoice("es", "b", ""), new PlaybackVoice("es", "c", "")));
        assertEquals("b", mModel.get(PlayerProperties.SELECTED_VOICE_ID));
    }

    @Test
    public void testPreviewVoice() {
        var voice = new PlaybackVoice("en", "a", "");
        mMediator.onPreviewVoiceClick(voice);
        verify(mDelegate).previewVoice(eq(voice));
    }

    @Test
    public void testVoiceMenuClosed() {
        mMediator.onVoiceMenuClosed();
        verify(mPlayerCoordinator).voiceMenuClosed();
    }

    @Test
    public void testCloseExpandedPlayer() {
        mMediator.onExpandedPlayerClose();
        verify(mPlayerCoordinator).restoreMiniPlayer();
    }

    private void resetPlayback() {
        reset(mPlayback);
        doReturn(mPlaybackMetadata).when(mPlayback).getMetadata();
    }
}
