// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.ui;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;

import android.content.Context;
import android.media.AudioManager;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.components.browser_ui.media.MediaNotificationManager;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.net.test.EmbeddedTestServer;

/**
 * Integration test that checks that autoplay muted doesn't show a notification nor take audio focus
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class AutoplayMutedNotificationTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private static final String TEST_PATH = "/content/test/data/media/session/autoplay-muted.html";
    private static final String VIDEO_ID = "video";
    private static final String PLAY_BUTTON_ID = "play";
    private static final String UNMUTE_BUTTON_ID = "unmute";
    private static final int AUDIO_FOCUS_CHANGE_TIMEOUT = 500; // ms

    private EmbeddedTestServer mTestServer;

    private AudioManager getAudioManager() {
        return (AudioManager)
                mActivityTestRule
                        .getActivity()
                        .getApplicationContext()
                        .getSystemService(Context.AUDIO_SERVICE);
    }

    private boolean isMediaNotificationVisible() {
        return MediaNotificationManager.getController(R.id.media_playback_notification) != null;
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
        mTestServer =
                EmbeddedTestServer.createAndStartServer(
                        ApplicationProvider.getApplicationContext());
        mAudioFocusChangeListener = new MockAudioFocusChangeListener();
        mActivityTestRule.startMainActivityWithURL(mTestServer.getURL(TEST_PATH));
    }

    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testBasic() throws Exception {
        Tab tab = mActivityTestRule.getActivity().getActivityTab();

        // Taking audio focus.
        Assert.assertEquals(
                AudioManager.AUDIOFOCUS_LOSS, mAudioFocusChangeListener.getAudioFocusState());
        mAudioFocusChangeListener.requestAudioFocus(AudioManager.AUDIOFOCUS_GAIN);
        Assert.assertEquals(
                AudioManager.AUDIOFOCUS_GAIN, mAudioFocusChangeListener.getAudioFocusState());

        // The page will autoplay the video.
        DOMUtils.waitForMediaPlay(tab.getWebContents(), VIDEO_ID);

        // Audio focus notification is OS-driven.
        Thread.sleep(AUDIO_FOCUS_CHANGE_TIMEOUT);

        // Audio focus was not taken and no notification is visible.
        Assert.assertEquals(
                AudioManager.AUDIOFOCUS_GAIN, mAudioFocusChangeListener.getAudioFocusState());
        Assert.assertFalse(isMediaNotificationVisible());
    }

    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testDoesNotReactToAudioFocus() throws Exception {
        Tab tab = mActivityTestRule.getActivity().getActivityTab();

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
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testAutoplayMutedThenUnmute() throws Exception {
        Tab tab = mActivityTestRule.getActivity().getActivityTab();

        // Taking audio focus.
        Assert.assertEquals(
                AudioManager.AUDIOFOCUS_LOSS, mAudioFocusChangeListener.getAudioFocusState());
        mAudioFocusChangeListener.requestAudioFocus(AudioManager.AUDIOFOCUS_GAIN);
        Assert.assertEquals(
                AudioManager.AUDIOFOCUS_GAIN, mAudioFocusChangeListener.getAudioFocusState());

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
        Assert.assertFalse(isMediaNotificationVisible());
    }

    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testMutedPlaybackDoesNotTakeAudioFocus() throws Exception {
        Tab tab = mActivityTestRule.getActivity().getActivityTab();

        // Taking audio focus.
        Assert.assertEquals(
                AudioManager.AUDIOFOCUS_LOSS, mAudioFocusChangeListener.getAudioFocusState());
        mAudioFocusChangeListener.requestAudioFocus(AudioManager.AUDIOFOCUS_GAIN);
        Assert.assertEquals(
                AudioManager.AUDIOFOCUS_GAIN, mAudioFocusChangeListener.getAudioFocusState());

        // The page will autoplay the video.
        DOMUtils.waitForMediaPlay(tab.getWebContents(), VIDEO_ID);

        // Audio focus notification is OS-driven.
        Thread.sleep(AUDIO_FOCUS_CHANGE_TIMEOUT);

        DOMUtils.pauseMedia(tab.getWebContents(), VIDEO_ID);

        // Restart the video with a gesture: no longer "muted autoplay".
        DOMUtils.clickNode(tab.getWebContents(), PLAY_BUTTON_ID);
        DOMUtils.waitForMediaPlay(tab.getWebContents(), VIDEO_ID);

        // Audio focus was not taken and no notification is visible.
        Assert.assertEquals(
                AudioManager.AUDIOFOCUS_GAIN, mAudioFocusChangeListener.getAudioFocusState());
        Assert.assertFalse(isMediaNotificationVisible());
    }

    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testUnmutedPlaybackTakesAudioFocus() throws Exception {
        Tab tab = mActivityTestRule.getActivity().getActivityTab();

        // Taking audio focus.
        Assert.assertEquals(
                AudioManager.AUDIOFOCUS_LOSS, mAudioFocusChangeListener.getAudioFocusState());
        mAudioFocusChangeListener.requestAudioFocus(AudioManager.AUDIOFOCUS_GAIN);
        Assert.assertEquals(
                AudioManager.AUDIOFOCUS_GAIN, mAudioFocusChangeListener.getAudioFocusState());

        // The page will autoplay the video.
        DOMUtils.waitForMediaPlay(tab.getWebContents(), VIDEO_ID);

        // Audio focus notification is OS-driven.
        Thread.sleep(AUDIO_FOCUS_CHANGE_TIMEOUT);

        // Restart the video with a gesture: no longer "muted autoplay".
        DOMUtils.clickNode(tab.getWebContents(), UNMUTE_BUTTON_ID);
        Assert.assertFalse(DOMUtils.isMediaPaused(tab.getWebContents(), VIDEO_ID));

        Thread.sleep(AUDIO_FOCUS_CHANGE_TIMEOUT);

        // Audio focus was taken and a notification is visible.
        Assert.assertNotEquals(
                AudioManager.AUDIOFOCUS_GAIN, mAudioFocusChangeListener.getAudioFocusState());
        Assert.assertTrue(isMediaNotificationVisible());
    }
}
