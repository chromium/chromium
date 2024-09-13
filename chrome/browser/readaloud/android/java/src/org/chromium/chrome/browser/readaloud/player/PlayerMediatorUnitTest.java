// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.readaloud.player;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.hamcrest.Matchers.hasItems;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyLong;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.modules.readaloud.PlaybackListener.State.BUFFERING;
import static org.chromium.chrome.modules.readaloud.PlaybackListener.State.ERROR;
import static org.chromium.chrome.modules.readaloud.PlaybackListener.State.PAUSED;
import static org.chromium.chrome.modules.readaloud.PlaybackListener.State.PLAYING;
import static org.chromium.chrome.modules.readaloud.PlaybackListener.State.STOPPED;
import static org.chromium.chrome.modules.readaloud.PlaybackListener.State.UNKNOWN;

import android.widget.SeekBar;
import android.widget.SeekBar.OnSeekBarChangeListener;

import org.junit.After;
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
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Promise;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.JniMocker;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.readaloud.ReadAloudMetrics;
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
    private final PlaybackVoice mPlaybackVoiceA = new PlaybackVoice("en", "a", "");

    private ObservableSupplierImpl<List<PlaybackVoice>> mVoicesSupplier;
    private ObservableSupplierImpl<String> mSelectedVoiceIdSupplier;
    private ObservableSupplierImpl<Boolean> mHighlightingEnabledSupplier;
    @Captor private ArgumentCaptor<PlaybackListener> mPlaybackListenerCaptor;
    public UserActionTester mUserActionTester;

    private PropertyModel mModel;
    private FakeClock mClock;

    /** FakeClock for setting the time. */
    static class FakeClock implements PlayerMediator.Clock {
        private long mCurrentTimeMillis;

        FakeClock() {
            mCurrentTimeMillis = 0;
        }

        @Override
        public long currentTimeMillis() {
            return mCurrentTimeMillis;
        }

        void advanceCurrentTimeMillis(long millis) {
            mCurrentTimeMillis += millis;
        }
    }

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
    private Promise<Playback> mPreviewPromise;
    @Mock private Playback mPreviewPlayback;

    private PlayerMediator mMediator;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        resetPlayback();
        doReturn(TITLE).when(mPlaybackMetadata).title();
        doReturn(PUBLISHER).when(mPlaybackMetadata).publisher();
        mVoicesSupplier = new ObservableSupplierImpl<>();
        mVoicesSupplier.set(List.of(new PlaybackVoice("en", "a")));
        mSelectedVoiceIdSupplier = new ObservableSupplierImpl<>();
        mSelectedVoiceIdSupplier.set("a");
        mHighlightingEnabledSupplier = new ObservableSupplierImpl<>();
        mHighlightingEnabledSupplier.set(true);
        mJniMocker.mock(ReadAloudPrefsJni.TEST_HOOKS, mPrefsNative);
        mMockPrefServiceHelper = new MockPrefServiceHelper();
        mPlaybackData = new TestPlaybackData();
        mClock = new FakeClock();

        doReturn(true).when(mDelegate).isHighlightingSupported();
        doReturn(mHighlightingEnabledSupplier).when(mDelegate).getHighlightingEnabledSupplier();
        doReturn(mVoicesSupplier).when(mDelegate).getCurrentLanguageVoicesSupplier();
        doReturn(mSelectedVoiceIdSupplier).when(mDelegate).getVoiceIdSupplier();
        doReturn(mMockPrefServiceHelper.getPrefService()).when(mDelegate).getPrefService();
        mPreviewPromise = new Promise<>();
        doReturn(mPreviewPromise).when(mDelegate).previewVoice(any());

        mModel = Mockito.spy(new PropertyModel.Builder(PlayerProperties.ALL_KEYS).build());
        mModel.set(PlayerProperties.HIDDEN_AND_PLAYING, false);
        mMediator = new PlayerMediator(mPlayerCoordinator, mDelegate, mModel);
        mMediator.setClockForTesting(mClock);
        mOnSeekBarChangeListener = mMediator.getSeekBarChangeListener();
        mUserActionTester = new UserActionTester();
    }

    @After
    public void tearDown() {
        mUserActionTester.tearDown();
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
        mMediator.setPlaybackState(PLAYING);
        assertEquals(PLAYING, (int) mModel.get(PlayerProperties.PLAYBACK_STATE));
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

        mPlaybackData.mState = PLAYING;
        mPlaybackListenerCaptor.getValue().onPlaybackDataChanged(mPlaybackData);

        assertEquals(PLAYING, (int) mModel.get(PlayerProperties.PLAYBACK_STATE));
    }

    @Test
    public void testPlaybackDurationRecorded() {
        final String histogramName = ReadAloudMetrics.TIME_SPENT_LISTENING;

        var histogram = HistogramWatcher.newSingleRecordWatcher(histogramName, 123);

        mMediator.setPlayback(mPlayback);
        verify(mPlayback).addListener(mPlaybackListenerCaptor.capture());

        mPlaybackData.mState = PLAYING;
        mPlaybackListenerCaptor.getValue().onPlaybackDataChanged(mPlaybackData);
        mClock.advanceCurrentTimeMillis(100);
        mPlaybackData.mState = PAUSED;
        mPlaybackListenerCaptor.getValue().onPlaybackDataChanged(mPlaybackData);

        mPlaybackData.mState = PLAYING;
        mPlaybackListenerCaptor.getValue().onPlaybackDataChanged(mPlaybackData);
        mClock.advanceCurrentTimeMillis(23);
        mPlaybackData.mState = STOPPED;
        mPlaybackListenerCaptor.getValue().onPlaybackDataChanged(mPlaybackData);

        mMediator.recordPlaybackDuration();

        histogram.assertExpected();
    }

    @Test
    public void testLockedPlaybackDurationRecorded() {
        final String histogramName = ReadAloudMetrics.TIME_SPENT_LISTENING_LOCKED_SCREEN;

        var histogram = HistogramWatcher.newSingleRecordWatcher(histogramName, 240);

        mMediator.setPlayback(mPlayback);
        verify(mPlayback).addListener(mPlaybackListenerCaptor.capture());
        // this time shouldn't be added since the screen hasn't been set to locked
        mClock.advanceCurrentTimeMillis(2000);
        mMediator.onScreenStatusChanged(true);

        mPlaybackData.mState = PLAYING;
        mPlaybackListenerCaptor.getValue().onPlaybackDataChanged(mPlaybackData);
        mClock.advanceCurrentTimeMillis(100);
        mPlaybackData.mState = PAUSED;
        mPlaybackListenerCaptor.getValue().onPlaybackDataChanged(mPlaybackData);
        // this time shouldn't be added since playback is paused
        mClock.advanceCurrentTimeMillis(2000);

        mPlaybackData.mState = PLAYING;
        mPlaybackListenerCaptor.getValue().onPlaybackDataChanged(mPlaybackData);
        mClock.advanceCurrentTimeMillis(140);

        mMediator.onScreenStatusChanged(false);
        // this time shouldn't be added since the screen was set to unlocked
        mClock.advanceCurrentTimeMillis(2000);

        mPlaybackData.mState = STOPPED;
        mPlaybackListenerCaptor.getValue().onPlaybackDataChanged(mPlaybackData);

        mMediator.recordPlaybackDuration();

        histogram.assertExpected();
    }

    @Test
    public void testLockedPlaybackDurationRecorded_NeverPlayed() {
        final String histogramName = ReadAloudMetrics.TIME_SPENT_LISTENING_LOCKED_SCREEN;

        var histogram = HistogramWatcher.newBuilder().expectNoRecords(histogramName).build();

        mMediator.setPlayback(mPlayback);
        verify(mPlayback).addListener(mPlaybackListenerCaptor.capture());
        mPlaybackData.mState = PLAYING;
        mPlaybackListenerCaptor.getValue().onPlaybackDataChanged(mPlaybackData);
        // this time shouldn't be added since the screen hasn't been set to locked
        mClock.advanceCurrentTimeMillis(2000);

        // Pause first then lock screen
        mPlaybackData.mState = PAUSED;
        mPlaybackListenerCaptor.getValue().onPlaybackDataChanged(mPlaybackData);
        mMediator.onScreenStatusChanged(true);

        // this time shouldn't be added since the playback is paused
        mClock.advanceCurrentTimeMillis(360);

        // unlock screen then play
        mMediator.onScreenStatusChanged(false);
        mPlaybackData.mState = PLAYING;
        mPlaybackListenerCaptor.getValue().onPlaybackDataChanged(mPlaybackData);
        // this time shouldn't be added since the screen was set to unlocked
        mClock.advanceCurrentTimeMillis(2000);

        mPlaybackData.mState = STOPPED;
        mPlaybackListenerCaptor.getValue().onPlaybackDataChanged(mPlaybackData);

        mMediator.recordPlaybackDuration();

        histogram.assertExpected();
    }

    @Test
    public void testPlayClicked() {
        mMediator.setPlayback(mPlayback);
        mMediator.setPlaybackState(PAUSED);

        mMediator.onPlayPauseClick();
        verify(mPlayback).play();

        Mockito.reset(mPlayback);
        mMediator.setPlaybackState(STOPPED);
        mMediator.onPlayPauseClick();
        verify(mPlayback).seek(0L);
        verify(mPlayback).play();
    }

    @Test
    public void testPlayClicked_fromStopped() {
        mMediator.setPlayback(mPlayback);
        mMediator.setPlaybackState(STOPPED);

        mMediator.onPlayPauseClick();
        verify(mPlayback).play();
    }

    @Test
    public void testPlayClicked_restoresPlayback() {
        mMediator.setPlayback(null);
        mMediator.setPlaybackState(STOPPED);
        mModel.set(PlayerProperties.RESTORABLE_PLAYBACK, true);

        mMediator.onPlayPauseClick();
        verify(mDelegate).restorePlayback();
    }

    @Test
    public void testPlayClicked_restoresPlayback_false() {
        mMediator.setPlayback(null);
        mMediator.setPlaybackState(STOPPED);
        mModel.set(PlayerProperties.RESTORABLE_PLAYBACK, false);

        mMediator.onPlayPauseClick();
        verify(mDelegate, never()).restorePlayback();
    }

    @Test
    public void testSetPlayerRestorable() {
        mMediator.setPlayerRestorable(true);
        verify(mModel).set(eq(PlayerProperties.RESTORABLE_PLAYBACK), eq(true));

        mMediator.setPlayerRestorable(false);
        verify(mModel).set(eq(PlayerProperties.RESTORABLE_PLAYBACK), eq(false));
    }

    @Test
    public void testPauseClicked() {
        mMediator.setPlayback(mPlayback);
        mMediator.setPlaybackState(PLAYING);

        mMediator.onPlayPauseClick();
        verify(mPlayback).pause();
    }

    @Test
    public void testCloseClicked() {
        mMediator.onCloseClick();
        verify(mPlayerCoordinator).closeClicked();
    }

    @Test
    public void testOnPublisherClick() {
        mMediator.onPublisherClick();
        verify(mPlayerCoordinator).hideExpandedPlayer();
        verify(mDelegate).navigateToPlayingTab();
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
    public void testBackClickActionRecords() {
        mMediator.onSeekBackClick();
        assertThat(mUserActionTester.getActions(), hasItems("ReadAloud.SeekBackward"));
    }

    @Test
    public void testForwardClickActionRecords() {
        mMediator.onSeekForwardClick();
        assertThat(mUserActionTester.getActions(), hasItems("ReadAloud.SeekForward"));
    }

    @Test
    public void testOnSeekForward() {
        mMediator.setPlayback(mPlayback);
        mModel.set(PlayerProperties.ELAPSED_NANOS, 0L);
        mModel.set(PlayerProperties.DURATION_NANOS, 40 * 1_000_000_000L);

        mMediator.onSeekForwardClick();
        verify(mPlayback).seekRelative(10 * 1_000_000_000L);
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
        PlaybackVoice voice = new PlaybackVoice("language", "voice");
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
    public void testOnStartStopTrackingTouch() {
        int initialState = PLAYING;
        mMediator.setPlaybackState(initialState);

        // Should set playback state to paused
        mOnSeekBarChangeListener.onStartTrackingTouch(mSeekbar);
        assertEquals(mModel.get(PlayerProperties.PLAYBACK_STATE), PAUSED);

        // Should set playback state to initial state
        mOnSeekBarChangeListener.onStopTrackingTouch(mSeekbar);
        assertEquals(mModel.get(PlayerProperties.PLAYBACK_STATE), initialState);
    }

    @Test
    public void testScrubbingSeekbarHistogramRecords() {
        // should record only the duration scrubbed forwards on the seekbar
        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        ReadAloudMetrics.DURATION_SCRUBBING_FORWARDS_SEEKBAR, 20000);
        mMediator.setPlayback(mPlayback);
        verify(mPlayback).addListener(mPlaybackListenerCaptor.capture());

        mPlaybackData.mState = PLAYING;

        mPlaybackData.mAbsolutePositionNanos = 0L;
        mPlaybackData.mTotalDurationNanos = 40 * 1_000_000_000L;
        mPlaybackListenerCaptor.getValue().onPlaybackDataChanged(mPlaybackData);

        mOnSeekBarChangeListener.onStartTrackingTouch(mSeekbar);
        mPlaybackData.mAbsolutePositionNanos = 20 * 1_000_000_000L;
        mPlaybackListenerCaptor.getValue().onPlaybackDataChanged(mPlaybackData);
        mOnSeekBarChangeListener.onStopTrackingTouch(mSeekbar);

        histogram.assertExpected();

        // should record only the duration scrubbed backwards on the seekbar
        histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        ReadAloudMetrics.DURATION_SCRUBBING_BACKWARDS_SEEKBAR, 20000);

        mPlaybackData.mAbsolutePositionNanos = 40 * 1_000_000_000L;
        mPlaybackData.mTotalDurationNanos = 40 * 1_000_000_000L;
        mPlaybackListenerCaptor.getValue().onPlaybackDataChanged(mPlaybackData);

        mOnSeekBarChangeListener.onStartTrackingTouch(mSeekbar);
        mPlaybackData.mAbsolutePositionNanos = 20 * 1_000_000_000L;
        mPlaybackListenerCaptor.getValue().onPlaybackDataChanged(mPlaybackData);
        mOnSeekBarChangeListener.onStopTrackingTouch(mSeekbar);

        histogram.assertExpected();
    }

    @Test
    public void testObserveVoiceList() {
        // Set up a Spanish voice pref that isn't the first in the list.
        MockPrefServiceHelper.setVoices(mPrefsNative, Map.of("es", "c"));

        // Setting the voice list here should trigger UI updates.
        mVoicesSupplier.set(List.of(new PlaybackVoice("es", "b"), new PlaybackVoice("es", "c")));

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
        mVoicesSupplier.set(List.of(new PlaybackVoice("es", "b"), new PlaybackVoice("es", "c")));
        assertEquals("b", mModel.get(PlayerProperties.SELECTED_VOICE_ID));
    }

    @Test
    public void testPreviewVoice_firstPreview() {
        triggerVoicePreview();
        fulfillPreview();
        updatePreviewPlaybackState(PLAYING);
    }

    @Test
    public void testPreviewVoice_pause() {
        triggerVoicePreview();
        fulfillPreview();
        updatePreviewPlaybackState(PLAYING);

        // Pause.
        updatePreviewPlaybackState(PAUSED);
    }

    @Test
    public void testPreviewVoice_stop() {
        triggerVoicePreview();
        fulfillPreview();
        updatePreviewPlaybackState(PLAYING);

        // Playback finishes.
        updatePreviewPlaybackState(STOPPED);
        verify(mPreviewPlayback).removeListener(eq(mPlaybackListenerCaptor.getValue()));
    }

    @Test
    public void testPreviewVoice_stopThenPlay() {
        triggerVoicePreview();
        fulfillPreview();
        updatePreviewPlaybackState(PLAYING);

        // Playback finishes.
        updatePreviewPlaybackState(STOPPED);
        verify(mPreviewPlayback).removeListener(eq(mPlaybackListenerCaptor.getValue()));

        // Playing again should trigger a new request.
        mMediator.onPreviewVoiceClick(mPlaybackVoiceA);
        assertEquals("a", mModel.get(PlayerProperties.PREVIEWING_VOICE_ID));
        assertEquals(BUFFERING, mModel.get(PlayerProperties.VOICE_PREVIEW_PLAYBACK_STATE));
        verify(mDelegate, times(2)).previewVoice(eq(mPlaybackVoiceA));
    }

    @Test
    public void testPreviewVoice_playThenPlayAnother() {
        triggerVoicePreview();
        fulfillPreview();
        updatePreviewPlaybackState(PLAYING);

        // Different voice preview requested.
        var newVoice = new PlaybackVoice("en", "b", "");
        mMediator.onPreviewVoiceClick(newVoice);
        verify(mModel).set(eq(PlayerProperties.VOICE_PREVIEW_PLAYBACK_STATE), eq(STOPPED));
        assertEquals("b", mModel.get(PlayerProperties.PREVIEWING_VOICE_ID));
        assertEquals(BUFFERING, mModel.get(PlayerProperties.VOICE_PREVIEW_PLAYBACK_STATE));
        verify(mDelegate).previewVoice(eq(newVoice));
    }

    @Test
    public void testPreviewVoice_errorBeforePlayback() {
        triggerVoicePreview();

        // Preview fails to load.
        failPreview();
        assertEquals(ERROR, mModel.get(PlayerProperties.VOICE_PREVIEW_PLAYBACK_STATE));
    }

    @Test
    public void testPreviewVoice_errorDuringPlayback() {
        triggerVoicePreview();
        fulfillPreview();
        updatePreviewPlaybackState(PLAYING);

        // Playback stops with an error.
        var data = new TestPlaybackData();
        data.mState = ERROR;
        mPlaybackListenerCaptor.getValue().onPlaybackDataChanged(data);
        verify(mModel).set(eq(PlayerProperties.VOICE_PREVIEW_PLAYBACK_STATE), eq(ERROR));
        // UI is reset.
        assertEquals(STOPPED, mModel.get(PlayerProperties.VOICE_PREVIEW_PLAYBACK_STATE));
        verify(mPreviewPlayback).removeListener(eq(mPlaybackListenerCaptor.getValue()));
    }

    @Test
    public void testVoiceMenuClosed() {
        mMediator.onVoiceMenuClosed();
        verify(mPlayerCoordinator).voiceMenuClosed();
    }

    @Test
    public void testShouldRestoreMiniPlayer_null() {
        mMediator.onShouldRestoreMiniPlayer();
        verify(mPlayerCoordinator, never()).restoreMiniPlayer();
    }

    @Test
    public void testShouldRestoreMiniPlayer_error() {
        // Set playback state through playback update, as if the error happened during playback.
        mMediator.setPlayback(mPlayback);
        verify(mPlayback).addListener(mPlaybackListenerCaptor.capture());

        mPlaybackData.mState = ERROR;
        mPlaybackListenerCaptor.getValue().onPlaybackDataChanged(mPlaybackData);

        assertEquals(ERROR, (int) mModel.get(PlayerProperties.PLAYBACK_STATE));

        // Clear playback.
        mMediator.setPlayback(null);

        // The mini player should restore.
        mMediator.onShouldRestoreMiniPlayer();
        verify(mPlayerCoordinator).restoreMiniPlayer();
    }

    @Test
    public void testShouldRestoreMiniPlayer() {
        mMediator.setPlayback(mPlayback);
        mMediator.onShouldRestoreMiniPlayer();
        verify(mPlayerCoordinator).restoreMiniPlayer();
    }

    @Test
    public void testShouldHideMiniPlayer_null() {
        mMediator.onShouldHideMiniPlayer();
        verify(mPlayerCoordinator, never()).hideMiniPlayer();
    }

    @Test
    public void testShouldHideMiniPlayer() {
        mMediator.setPlayback(mPlayback);
        mMediator.onShouldHideMiniPlayer();
        verify(mPlayerCoordinator).hideMiniPlayer();
    }

    @Test
    public void testVoiceMenuClosedDuringPreviewLoading() {
        triggerVoicePreview();

        // Closing the voice menu should cause the preview play button to reset.
        mMediator.onVoiceMenuClosed();
        verify(mPlayerCoordinator).voiceMenuClosed();
        assertEquals(STOPPED, mModel.get(PlayerProperties.VOICE_PREVIEW_PLAYBACK_STATE));
    }

    @Test
    public void testVoiceMenuClosedDuringPreviewPlaying() {
        triggerVoicePreview();
        fulfillPreview();

        // Closing the voice menu should cause the preview play button to reset and the listener to
        // be removed.
        mMediator.onVoiceMenuClosed();
        verify(mPlayerCoordinator).voiceMenuClosed();
        assertEquals(STOPPED, mModel.get(PlayerProperties.VOICE_PREVIEW_PLAYBACK_STATE));
        verify(mPreviewPlayback).removeListener(eq(mPlaybackListenerCaptor.getValue()));
    }

    @Test
    public void testNoStateUpdatesWhenHidden() {
        mModel.set(PlayerProperties.HIDDEN_AND_PLAYING, true);
        mMediator.setPlayback(mPlayback);
        verify(mPlayback).addListener(mPlaybackListenerCaptor.capture());

        assertEquals(UNKNOWN, (int) mModel.get(PlayerProperties.PLAYBACK_STATE));
        assertEquals(0.0f, (float) mModel.get(PlayerProperties.PROGRESS), /* delta= */ 1e-8f);

        mPlaybackData.mState = PLAYING;

        mPlaybackData.mAbsolutePositionNanos = POSITION_NS;
        mPlaybackData.mTotalDurationNanos = DURATION_NS;
        mPlaybackListenerCaptor.getValue().onPlaybackDataChanged(mPlaybackData);

        assertEquals(0.0f, (float) mModel.get(PlayerProperties.PROGRESS), /* delta= */ 1e-8f);

        // Playback state is the exception: if it changes while hidden, UI should show
        // the new state when it is restored.
        assertEquals(PLAYING, (int) mModel.get(PlayerProperties.PLAYBACK_STATE));
    }

    private void resetPlayback() {
        reset(mPlayback);
        doReturn(mPlaybackMetadata).when(mPlayback).getMetadata();
    }

    // Voice preview helpers
    private void triggerVoicePreview() {
        mMediator.onPreviewVoiceClick(mPlaybackVoiceA);
        assertEquals("a", mModel.get(PlayerProperties.PREVIEWING_VOICE_ID));
        assertEquals(BUFFERING, mModel.get(PlayerProperties.VOICE_PREVIEW_PLAYBACK_STATE));
        verify(mDelegate).previewVoice(eq(mPlaybackVoiceA));
    }

    private void fulfillPreview() {
        mPreviewPromise.fulfill(mPreviewPlayback);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mPreviewPlayback).addListener(mPlaybackListenerCaptor.capture());
    }

    private void updatePreviewPlaybackState(@PlaybackListener.State int state) {
        TestPlaybackData data = new TestPlaybackData();
        data.mState = state;
        mPlaybackListenerCaptor.getValue().onPlaybackDataChanged(data);
        assertEquals(state, mModel.get(PlayerProperties.VOICE_PREVIEW_PLAYBACK_STATE));
    }

    private void failPreview() {
        mPreviewPromise.reject(new Exception());
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
    }
}
