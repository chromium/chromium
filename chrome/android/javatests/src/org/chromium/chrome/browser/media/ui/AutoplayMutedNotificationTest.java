// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.ui;

import android.content.Context;
import android.content.res.Resources;
import android.media.AudioManager;
import android.provider.Settings;

import androidx.test.filters.SmallTest;

import org.hamcrest.Matcher;
import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.components.browser_ui.media.MediaNotificationManager;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;

/**
 * Integration test that checks that autoplay muted doesn't show a notification nor take audio focus
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class AutoplayMutedNotificationTest {
    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    private static final String TEST_PATH = "/content/test/data/media/session/autoplay-muted.html";
    private static final String VIDEO_ID = "video";
    private static final String PLAY_BUTTON_ID = "play";
    private static final String UNMUTE_BUTTON_ID = "unmute";
    private static final int AUDIO_FOCUS_CHANGE_TIMEOUT = 500; // ms
    private static final String AUDIO_FOCUS_REQUEST_RESULT_HISTOGRAM =
            "Media.Android.AudioFocusRequestResult";
    // Must match AudioFocusDelegate.AudioFocusRequestResult.GRANTED.
    private static final int AUDIO_FOCUS_REQUEST_GRANTED = 1;

    private AudioManager getAudioManager() {
        return (AudioManager)
                ContextUtils.getApplicationContext().getSystemService(Context.AUDIO_SERVICE);
    }

    private boolean isMultiAudioFocusEnabled() {
        Context context = ContextUtils.getApplicationContext();
        Resources res = context.getResources();
        int resId =
                res.getIdentifier("config_multi_audio_focus_enabled_default", "bool", "android");
        boolean defaultEnabled = resId != 0 && res.getBoolean(resId);
        return Settings.System.getInt(
                        context.getContentResolver(),
                        "multi_audio_focus_enabled",
                        defaultEnabled ? 1 : 0)
                != 0;
    }

    private boolean isMediaNotificationVisible() {
        return MediaNotificationManager.getActiveOrFallbackControllerByMediaTypeId(
                        R.id.media_playback_notification)
                != null;
    }

    private class MockAudioFocusChangeListener implements AudioManager.OnAudioFocusChangeListener {
        private int mAudioFocusState = AudioManager.AUDIOFOCUS_LOSS;

        @Override
        public void onAudioFocusChange(int focusChange) {
            mAudioFocusState = focusChange;
        }

        public int getAudioFocusState() {
            return mAudioFocusState;
        }

        public void requestAudioFocus(int focusType) {
            int result =
                    getAudioManager().requestAudioFocus(this, AudioManager.STREAM_MUSIC, focusType);
            if (result != AudioManager.AUDIOFOCUS_REQUEST_GRANTED) {
                Assert.fail("Did not get audio focus");
            } else {
                mAudioFocusState = focusType;
            }
        }
    }

    private MockAudioFocusChangeListener mAudioFocusChangeListener;

    @Before
    public void setUp() {
        mAudioFocusChangeListener = new MockAudioFocusChangeListener();
    }

    @After
    public void tearDown() {
        if (mAudioFocusChangeListener != null) {
            getAudioManager().abandonAudioFocus(mAudioFocusChangeListener);
        }
        MediaNotificationManager.resetForTesting();
    }

    @Test
    @SmallTest
    public void testBasic() throws Exception {
        // Taking audio focus.
        Assert.assertEquals(
                AudioManager.AUDIOFOCUS_LOSS, mAudioFocusChangeListener.getAudioFocusState());
        mAudioFocusChangeListener.requestAudioFocus(AudioManager.AUDIOFOCUS_GAIN);
        Assert.assertEquals(
                AudioManager.AUDIOFOCUS_GAIN, mAudioFocusChangeListener.getAudioFocusState());

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(AUDIO_FOCUS_REQUEST_RESULT_HISTOGRAM)
                        .build();
        mActivityTestRule.startOnTestServerUrl(TEST_PATH);
        Tab tab = mActivityTestRule.getActivityTab();

        // The page will autoplay the video.
        DOMUtils.waitForMediaPlay(tab.getWebContents(), VIDEO_ID);

        // Audio focus notification is OS-driven.
        Thread.sleep(AUDIO_FOCUS_CHANGE_TIMEOUT);

        // Audio focus was not taken and no notification is visible.
        Assert.assertEquals(
                AudioManager.AUDIOFOCUS_GAIN, mAudioFocusChangeListener.getAudioFocusState());
        histogramWatcher.assertExpected();
        Assert.assertFalse(isMediaNotificationVisible());
    }

    @Test
    @SmallTest
    public void testDoesNotReactToAudioFocus() throws Exception {
        mActivityTestRule.startOnTestServerUrl(TEST_PATH);
        Tab tab = mActivityTestRule.getActivityTab();

        // The page will autoplay the video.
        DOMUtils.waitForMediaPlay(tab.getWebContents(), VIDEO_ID);

        // Taking audio focus.
        Assert.assertEquals(
                AudioManager.AUDIOFOCUS_LOSS, mAudioFocusChangeListener.getAudioFocusState());
        mAudioFocusChangeListener.requestAudioFocus(AudioManager.AUDIOFOCUS_GAIN);
        Assert.assertEquals(
                AudioManager.AUDIOFOCUS_GAIN, mAudioFocusChangeListener.getAudioFocusState());

        // Audio focus notification is OS-driven.
        Thread.sleep(AUDIO_FOCUS_CHANGE_TIMEOUT);

        // Video did not pause.
        Assert.assertFalse(DOMUtils.isMediaPaused(tab.getWebContents(), VIDEO_ID));

        // Still no notification.
        Assert.assertFalse(isMediaNotificationVisible());
    }

    @Test
    @SmallTest
    public void testAutoplayMutedThenUnmute() throws Exception {
        // Taking audio focus.
        Assert.assertEquals(
                AudioManager.AUDIOFOCUS_LOSS, mAudioFocusChangeListener.getAudioFocusState());
        mAudioFocusChangeListener.requestAudioFocus(AudioManager.AUDIOFOCUS_GAIN);
        Assert.assertEquals(
                AudioManager.AUDIOFOCUS_GAIN, mAudioFocusChangeListener.getAudioFocusState());

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(AUDIO_FOCUS_REQUEST_RESULT_HISTOGRAM)
                        .build();
        mActivityTestRule.startOnTestServerUrl(TEST_PATH);
        Tab tab = mActivityTestRule.getActivityTab();

        // The page will autoplay the video.
        DOMUtils.waitForMediaPlay(tab.getWebContents(), VIDEO_ID);

        StringBuilder sb = new StringBuilder();
        sb.append("(function() {");
        sb.append("  var video = document.querySelector('video');");
        sb.append("  video.muted = false;");
        sb.append("  return video.muted;");
        sb.append("})();");

        // Unmute from script.
        String result =
                JavaScriptUtils.executeJavaScriptAndWaitForResult(
                        tab.getWebContents(), sb.toString());
        Assert.assertTrue(result.trim().equalsIgnoreCase("false"));

        // Video is paused.
        Assert.assertTrue(DOMUtils.isMediaPaused(tab.getWebContents(), VIDEO_ID));

        // Audio focus notification is OS-driven.
        Thread.sleep(AUDIO_FOCUS_CHANGE_TIMEOUT);

        // Audio focus was not taken and no notification is visible.
        Assert.assertEquals(
                AudioManager.AUDIOFOCUS_GAIN, mAudioFocusChangeListener.getAudioFocusState());
        histogramWatcher.assertExpected();
        Assert.assertFalse(isMediaNotificationVisible());
    }

    @Test
    @SmallTest
    public void testMutedPlaybackDoesNotTakeAudioFocus() throws Exception {
        // Taking audio focus.
        Assert.assertEquals(
                AudioManager.AUDIOFOCUS_LOSS, mAudioFocusChangeListener.getAudioFocusState());
        mAudioFocusChangeListener.requestAudioFocus(AudioManager.AUDIOFOCUS_GAIN);
        Assert.assertEquals(
                AudioManager.AUDIOFOCUS_GAIN, mAudioFocusChangeListener.getAudioFocusState());

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(AUDIO_FOCUS_REQUEST_RESULT_HISTOGRAM)
                        .build();
        mActivityTestRule.startOnTestServerUrl(TEST_PATH);
        Tab tab = mActivityTestRule.getActivityTab();

        // The page will autoplay the video.
        DOMUtils.waitForMediaPlay(tab.getWebContents(), VIDEO_ID);

        // Audio focus notification is OS-driven.
        Thread.sleep(AUDIO_FOCUS_CHANGE_TIMEOUT);

        DOMUtils.pauseMedia(tab.getWebContents(), VIDEO_ID);

        // Restart the video with a gesture: no longer "muted autoplay".
        DOMUtils.clickNodeWithJavaScript(tab.getWebContents(), PLAY_BUTTON_ID);
        DOMUtils.waitForMediaPlay(tab.getWebContents(), VIDEO_ID);

        // Audio focus notification is OS-driven.
        Thread.sleep(AUDIO_FOCUS_CHANGE_TIMEOUT);

        // Audio focus was not taken and no notification is visible.
        Assert.assertEquals(
                AudioManager.AUDIOFOCUS_GAIN, mAudioFocusChangeListener.getAudioFocusState());
        histogramWatcher.assertExpected();
        Assert.assertFalse(isMediaNotificationVisible());
    }

    @Test
    @SmallTest
    public void testUnmutedPlaybackTakesAudioFocus() throws Exception {
        // Taking audio focus.
        Assert.assertEquals(
                AudioManager.AUDIOFOCUS_LOSS, mAudioFocusChangeListener.getAudioFocusState());
        mAudioFocusChangeListener.requestAudioFocus(AudioManager.AUDIOFOCUS_GAIN);
        Assert.assertEquals(
                AudioManager.AUDIOFOCUS_GAIN, mAudioFocusChangeListener.getAudioFocusState());

        HistogramWatcher mutedHistogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(AUDIO_FOCUS_REQUEST_RESULT_HISTOGRAM)
                        .build();
        mActivityTestRule.startOnTestServerUrl(TEST_PATH);
        Tab tab = mActivityTestRule.getActivityTab();

        // The page will autoplay the video.
        DOMUtils.waitForMediaPlay(tab.getWebContents(), VIDEO_ID);

        // Audio focus notification is OS-driven.
        Thread.sleep(AUDIO_FOCUS_CHANGE_TIMEOUT);
        Assert.assertEquals(
                AudioManager.AUDIOFOCUS_GAIN, mAudioFocusChangeListener.getAudioFocusState());
        mutedHistogramWatcher.assertExpected();

        HistogramWatcher unmutedHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        AUDIO_FOCUS_REQUEST_RESULT_HISTOGRAM, AUDIO_FOCUS_REQUEST_GRANTED);

        // Restart the video with a gesture: no longer "muted autoplay".
        DOMUtils.clickNodeWithJavaScript(tab.getWebContents(), UNMUTE_BUTTON_ID);
        Assert.assertFalse(DOMUtils.isMediaPaused(tab.getWebContents(), VIDEO_ID));

        // Audio focus was requested by Chrome and a notification is visible.
        // The pre-existing audio focus holder only loses focus if multi-audio focus is disabled.
        unmutedHistogramWatcher.pollInstrumentationThreadUntilSatisfied();
        Matcher<Integer> expectedFocusStateMatcher =
                isMultiAudioFocusEnabled()
                        ? Matchers.is(AudioManager.AUDIOFOCUS_GAIN)
                        : Matchers.not(AudioManager.AUDIOFOCUS_GAIN);
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    Criteria.checkThat(
                            mAudioFocusChangeListener.getAudioFocusState(),
                            expectedFocusStateMatcher);
                    Criteria.checkThat(isMediaNotificationVisible(), Matchers.is(true));
                });
    }
}
