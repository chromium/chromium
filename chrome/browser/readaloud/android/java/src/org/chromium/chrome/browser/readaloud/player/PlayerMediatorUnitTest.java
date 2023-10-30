// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.readaloud.player;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.verify;

import android.app.Activity;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.readaloud.ReadAloudPrefs;
import org.chromium.chrome.browser.readaloud.testing.MockPrefServiceHelper;
import org.chromium.chrome.modules.readaloud.Playback;
import org.chromium.chrome.modules.readaloud.PlaybackArgs.PlaybackVoice;
import org.chromium.chrome.modules.readaloud.PlaybackListener;
import org.chromium.chrome.modules.readaloud.Player;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.prefs.PrefService;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.HashMap;
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

    @Mock private PlayerCoordinator mPlayerCoordinator;
    @Mock private Playback mPlayback;
    @Mock private Playback.Metadata mPlaybackMetadata;
    private MockPrefServiceHelper mMockPrefServiceHelper;

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

    private class TestPlayerDelegate implements Player.Delegate {
        @Override
        public BottomSheetController getBottomSheetController() {
            return null;
        }

        @Override
        public boolean isHighlightingSupported() {
            return true;
        }

        @Override
        public ObservableSupplierImpl<Boolean> getHighlightingEnabledSupplier() {
            return new ObservableSupplierImpl<Boolean>();
        }

        @Override
        public ObservableSupplier<List<PlaybackVoice>> getCurrentLanguageVoicesSupplier() {
            return new ObservableSupplierImpl<List<PlaybackVoice>>();
        }

        @Override
        public ObservableSupplier<String> getVoiceIdSupplier() {
            return new ObservableSupplierImpl<String>();
        }

        @Override
        public Map<String, String> getVoiceOverrides() {
            return new HashMap<String, String>();
        }

        @Override
        public void setVoiceOverride(PlaybackVoice voice) {}

        @Override
        public void previewVoice(PlaybackVoice voice) {}

        @Override
        public void navigateToPlayingTab() {}

        @Override
        public Activity getActivity() {
            return null;
        }

        @Override
        public PrefService getPrefService() {
            return mMockPrefServiceHelper.getPrefService();
        }
    }

    private TestPlayerDelegate mDelegate;

    private PlayerMediator mMediator;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        resetPlayback();
        doReturn(TITLE).when(mPlaybackMetadata).title();
        doReturn(PUBLISHER).when(mPlaybackMetadata).publisher();
        mMockPrefServiceHelper = new MockPrefServiceHelper();
        mPlaybackData = new TestPlaybackData();
        mDelegate = new TestPlayerDelegate();
        mModel = new PropertyModel.Builder(PlayerProperties.ALL_KEYS).build();
        mMediator = new PlayerMediator(mPlayerCoordinator, mDelegate, mModel);
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
    public void testOnMiniPlayerExpandClick() {
        mMediator.onMiniPlayerExpandClick();
        verify(mPlayerCoordinator).expand();
    }

    @Test
    public void testOnVoiceSelected() {
        mMediator.onVoiceSelected(new PlaybackVoice("language", "voice", "description"));

        Map<String, String> voices = ReadAloudPrefs.getVoices(mDelegate.getPrefService());
        assertEquals(1, voices.size());
        assertEquals("voice", voices.get("language"));
    }

    @Test
    public void testOnHighlightingChanged() {
        mMediator.onHighlightingChange(false);

        PrefService prefs = mDelegate.getPrefService();
        assertFalse(ReadAloudPrefs.isHighlightingEnabled(prefs));

        mMediator.onHighlightingChange(true);
        assertTrue(ReadAloudPrefs.isHighlightingEnabled(prefs));
    }

    @Test
    public void testOnSpeedChange() {
        mMediator.onSpeedChange(2f);
        assertEquals(2f, ReadAloudPrefs.getSpeed(mDelegate.getPrefService()), /* delta= */ 0f);
    }

    private void resetPlayback() {
        reset(mPlayback);
        doReturn(mPlaybackMetadata).when(mPlayback).getMetadata();
    }
}
