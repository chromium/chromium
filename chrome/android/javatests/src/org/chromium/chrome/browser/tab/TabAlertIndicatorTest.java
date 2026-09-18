// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.fail;
import static org.junit.Assume.assumeTrue;

import android.app.Activity;
import android.app.Instrumentation;
import android.content.Context;
import android.content.Intent;
import android.os.Build;

import androidx.test.espresso.intent.Intents;
import androidx.test.espresso.intent.matcher.IntentMatchers;
import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;

import org.chromium.base.ServiceLoaderUtil;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.media.MediaCapturePickerDelegate;
import org.chromium.chrome.browser.media.MediaCapturePickerManager;
import org.chromium.chrome.browser.media.PictureInPicture;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabRemover;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.util.browser.TabLoadObserver;
import org.chromium.components.browser_ui.notifications.ForegroundServiceUtils;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.content_settings.ContentSetting;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.tabs.TabAlert;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.media.MediaSwitches;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.test.util.DeviceRestriction;
import org.chromium.url.GURL;

import java.util.List;
import java.util.concurrent.TimeoutException;

/** Tests for {@link TabAlert} and {@link MediaState}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    MediaSwitches.AUTOPLAY_NO_GESTURE_REQUIRED_POLICY,
    ContentSwitches.USE_FAKE_DEVICE_FOR_MEDIA_STREAM,
    "enable-experimental-web-platform-features",
    "enable-features=UserMediaScreenCapturing,AndroidMediaPicker",
})
@Batch(Batch.PER_CLASS)
public class TabAlertIndicatorTest {
    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    private static final String TEST_PATH = "/chrome/test/data/media/tab_media_indicator.html";
    private static final String GOOGLE_PATH = "/chrome/test/data/android/google.html";
    private static final String VIDEO_ID = "video";
    private static final String MUTE_VIDEO_ID = "mute";
    private static final String UNMUTE_VIDEO_ID = "unmute";
    private static final String REQUEST_PIP_ID = "request-pip";
    private static final String EXIT_PIP_ID = "exit-pip";
    private static final String REQUEST_MIC_ID = "request-mic";
    private static final String REQUEST_CAM_ID = "request-cam";
    private static final String REQUEST_TAB_CAPTURE_ID = "request-tab-capture";
    private static final String STOP_TAB_CAPTURE_ID = "stop-tab-capture";
    private static final String SCREEN_CAPTURE_INTENT_ACTION = "CUSTOM_ACTION";

    /** Extra time allowed on top of the default poll timeout when waiting for a title change. */
    private static final long TITLE_TIMEOUT_SLACK_MS = 2000;

    private WebPageStation mPage;
    private TabModel mTabModel;
    private Tab mTab;
    private MockMediaCapturePickerDelegate mMediaPickerDelegate;

    private class MockMediaCapturePickerDelegate implements MediaCapturePickerDelegate {
        private Tab mPickedTab;
        public boolean mCreateScreenCaptureIntentCalled;

        public void setPickedTab(Tab tab) {
            mPickedTab = tab;
        }

        @Override
        public Intent createScreenCaptureIntent(
                Context context,
                MediaCapturePickerManager.Params params,
                MediaCapturePickerManager.Delegate delegate) {
            mCreateScreenCaptureIntentCalled = true;
            return new Intent(SCREEN_CAPTURE_INTENT_ACTION);
        }

        @Override
        public Tab getPickedTab() {
            return mPickedTab != null ? mPickedTab : mTab;
        }

        @Override
        public boolean shouldShareAudio() {
            return false;
        }
    }

    @Before
    public void setUp() throws Exception {
        ChromeTabbedActivity.interceptMoveTaskToBackForTesting();
        mPage = mActivityTestRule.startOnBlankPage();
        mTabModel = mActivityTestRule.getActivity().getTabModelSelector().getModel(false);
        mTab = mPage.getTab();

        new TabLoadObserver(mTab).fullyLoadUrl(mActivityTestRule.getTestServer().getURL(TEST_PATH));
        DOMUtils.waitForNonZeroNodeBounds(mTab.getWebContents(), VIDEO_ID);
        assertEquals(TabAlert.NONE, mTab.getAlertState());

        grantRecordingPermissions();

        ForegroundServiceUtils.setInstanceForTesting(Mockito.mock(ForegroundServiceUtils.class));

        mMediaPickerDelegate = new MockMediaCapturePickerDelegate();
        ServiceLoaderUtil.setInstanceForTesting(
                MediaCapturePickerDelegate.class, mMediaPickerDelegate);
        Intents.init();
        Intents.intending(IntentMatchers.hasAction(SCREEN_CAPTURE_INTENT_ACTION))
                .respondWith(new Instrumentation.ActivityResult(Activity.RESULT_OK, new Intent()));
    }

    @After
    public void tearDown() {
        Intents.release();
    }

    @Test
    @SmallTest
    public void testAlertStateAudioPlaying() throws TimeoutException {
        DOMUtils.playMedia(mTab.getWebContents(), VIDEO_ID);
        DOMUtils.waitForMediaPlay(mTab.getWebContents(), VIDEO_ID);
        waitForAlertState(mTab, TabAlert.AUDIO_PLAYING);
    }

    @Test
    @SmallTest
    public void testAlertStateAudioMuting() throws TimeoutException {
        setMuteState(true);
        DOMUtils.playMedia(mTab.getWebContents(), VIDEO_ID);
        DOMUtils.waitForMediaPlay(mTab.getWebContents(), VIDEO_ID);
        waitForAlertState(mTab, TabAlert.AUDIO_MUTING);
    }

    @Test
    @SmallTest
    public void testAlertStateAudioMutingThenUnmute() throws TimeoutException {
        setMuteState(true);
        DOMUtils.playMedia(mTab.getWebContents(), VIDEO_ID);
        DOMUtils.waitForMediaPlay(mTab.getWebContents(), VIDEO_ID);
        waitForAlertState(mTab, TabAlert.AUDIO_MUTING);
        setMuteState(false);
        waitForAlertState(mTab, TabAlert.AUDIO_PLAYING);
    }

    @Test
    @SmallTest
    public void testAlertStateAudioPlayingThenMute() throws TimeoutException {
        DOMUtils.playMedia(mTab.getWebContents(), VIDEO_ID);
        DOMUtils.waitForMediaPlay(mTab.getWebContents(), VIDEO_ID);
        waitForAlertState(mTab, TabAlert.AUDIO_PLAYING);
        setMuteState(true);
        waitForAlertState(mTab, TabAlert.AUDIO_MUTING);
    }

    @Test
    @SmallTest
    public void testAlertStateAudioPlayingMuteWithPause() throws TimeoutException {
        DOMUtils.playMedia(mTab.getWebContents(), VIDEO_ID);
        DOMUtils.waitForMediaPlay(mTab.getWebContents(), VIDEO_ID);
        waitForAlertState(mTab, TabAlert.AUDIO_PLAYING);

        // Pause video.
        DOMUtils.pauseMedia(mTab.getWebContents(), VIDEO_ID);
        DOMUtils.waitForMediaPauseBeforeEnd(mTab.getWebContents(), VIDEO_ID);

        // Wait for the recently audible state to clear.
        waitForAlertState(mTab, TabAlert.NONE);

        // Mute video.
        setMuteState(true);
        assertEquals(TabAlert.NONE, mTab.getAlertState());

        // Play the video again.
        DOMUtils.playMedia(mTab.getWebContents(), VIDEO_ID);
        DOMUtils.waitForMediaPlay(mTab.getWebContents(), VIDEO_ID);
        waitForAlertState(mTab, TabAlert.AUDIO_MUTING);
    }

    @Test
    @SmallTest
    public void testAlertStateWithVideoMutedAndUnmuted() throws TimeoutException {
        DOMUtils.playMedia(mTab.getWebContents(), VIDEO_ID);
        DOMUtils.waitForMediaPlay(mTab.getWebContents(), VIDEO_ID);
        waitForAlertState(mTab, TabAlert.AUDIO_PLAYING);

        // Mute video element.
        DOMUtils.clickNodeWithJavaScript(mTab.getWebContents(), MUTE_VIDEO_ID);

        // Wait for the recently audible state to clear.
        assertFalse(DOMUtils.isMediaPaused(mTab.getWebContents(), VIDEO_ID));
        waitForAlertState(mTab, TabAlert.NONE);

        // Unmute video element.
        DOMUtils.clickNodeWithJavaScript(mTab.getWebContents(), UNMUTE_VIDEO_ID);
        waitForAlertState(mTab, TabAlert.AUDIO_PLAYING);
    }

    @Test
    @SmallTest
    public void testAlertStateAudioRecording() {
        requestMic();
        waitForAlertState(mTab, TabAlert.AUDIO_RECORDING);
    }

    @Test
    @SmallTest
    public void testAlertStateVideoRecording() {
        requestCam();
        waitForAlertState(mTab, TabAlert.VIDEO_RECORDING);
    }

    @Test
    @SmallTest
    // PictureInPicture#isEnabled() is true on Android 11+.
    @DisableIf.Build(sdk_is_less_than = Build.VERSION_CODES.R)
    // PiP is not supported for automotive.
    @Restriction(DeviceRestriction.RESTRICTION_TYPE_NON_AUTO)
    public void testAlertStatePipPlaying() throws TimeoutException {
        assumeTrue("PiP is not enabled", isPiPEnabled());

        DOMUtils.playMedia(mTab.getWebContents(), VIDEO_ID);
        DOMUtils.waitForMediaPlay(mTab.getWebContents(), VIDEO_ID);
        waitForAlertState(mTab, TabAlert.AUDIO_PLAYING);

        // Enter Picture-in-Picture
        enterPictureInPicture();
        waitForAlertState(mTab, TabAlert.PIP_PLAYING);

        // Exit Picture-in-Picture
        exitPictureInPicture();
        waitForAlertState(mTab, TabAlert.AUDIO_PLAYING);
    }

    @Test
    @SmallTest
    public void testAlertStatePriority() throws TimeoutException {
        // MUTED
        setMuteState(true);
        DOMUtils.playMedia(mTab.getWebContents(), VIDEO_ID);
        DOMUtils.waitForMediaPlay(mTab.getWebContents(), VIDEO_ID);
        waitForAlertState(mTab, TabAlert.AUDIO_MUTING);

        // AUDIBLE
        setMuteState(false);
        waitForAlertState(mTab, TabAlert.AUDIO_PLAYING);

        // RECORDING
        requestMic();
        waitForAlertState(mTab, TabAlert.AUDIO_RECORDING);

        if (isPiPEnabled()) {
            // PICTURE_IN_PICTURE
            // Indicator should stay RECORDING as it has higher priority.
            enterPictureInPicture();
            waitForAlertState(mTab, TabAlert.AUDIO_RECORDING);

            // Stop recording, indicator should drop to PiP.
            stopMic();
            waitForAlertState(mTab, TabAlert.PIP_PLAYING);

            // Exit PiP, indicator should drop to AUDIBLE.
            exitPictureInPicture();
            waitForAlertState(mTab, TabAlert.AUDIO_PLAYING);
        }
    }

    @Test
    @SmallTest
    @Restriction(DeviceFormFactor.DESKTOP)
    public void testAlertStateTabCapturingOverridesRecording() {
        requestMic();
        waitForAlertState(mTab, TabAlert.AUDIO_RECORDING);

        startTabCapture(mTab, mTab);
        stopTabCapture(mTab);
        waitForAlertState(mTab, TabAlert.AUDIO_RECORDING);
    }

    @Test
    @SmallTest
    @Restriction(DeviceFormFactor.DESKTOP)
    public void testAlertStateTabCapturingNewTab() {
        Tab newTab = openNewTabWith(GOOGLE_PATH);

        // Pick the new tab to be captured
        mMediaPickerDelegate.setPickedTab(newTab);

        selectTab(mTab);
        startTabCapture(mTab, newTab);
        stopTabCapture(mTab);
        waitForAlertState(newTab, TabAlert.NONE);
    }

    @Test
    @SmallTest
    @Restriction(DeviceFormFactor.DESKTOP)
    public void testAlertStateTabCapturingDisappearsWhenCapturerTabIsClosed() {
        Tab newTab = openNewTabWith(GOOGLE_PATH);

        // Pick the new tab to be captured
        mMediaPickerDelegate.setPickedTab(newTab);

        selectTab(mTab);
        startTabCapture(mTab, newTab);

        closeTab(mTab);
        waitForAlertState(newTab, TabAlert.NONE);
    }

    @Test
    @SmallTest
    @Restriction(DeviceFormFactor.DESKTOP)
    public void testAlertStateTabCapturingWithTwoCapturers() {
        Tab capturer1Tab = mTab;

        // Create and setup a capturee tab.
        Tab captureeTab = openNewTabWith(GOOGLE_PATH);

        // Create and setup a second capturer tab.
        Tab capturer2Tab = openNewTabWith(TEST_PATH);
        DOMUtils.waitForNonZeroNodeBounds(capturer2Tab.getWebContents(), REQUEST_TAB_CAPTURE_ID);

        // Start capture from the first tab.
        selectTab(capturer1Tab);
        mMediaPickerDelegate.setPickedTab(captureeTab);
        startTabCapture(capturer1Tab, captureeTab);

        // Start capture from the second tab.
        selectTab(capturer2Tab);
        mMediaPickerDelegate.setPickedTab(captureeTab);
        startTabCapture(capturer2Tab, captureeTab);

        // Stop capture from the first tab and verify the indicator is still present.
        selectTab(capturer1Tab);
        stopTabCapture(capturer1Tab);
        // The alert state should persist as the second capturer is still active.
        waitForAlertState(captureeTab, TabAlert.TAB_CAPTURING);

        // Stop capture from the second tab and verify the indicator is gone.
        selectTab(capturer2Tab);
        stopTabCapture(capturer2Tab);
        waitForAlertState(captureeTab, TabAlert.NONE);
    }

    @Test
    @SmallTest
    public void testMediaStateHistogram() throws TimeoutException {
        // Expect AUDIBLE
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Tab.Android.MediaState", MediaState.AUDIBLE);
        DOMUtils.playMedia(mTab.getWebContents(), VIDEO_ID);
        DOMUtils.waitForMediaPlay(mTab.getWebContents(), VIDEO_ID);
        waitForMediaState(mTab, MediaState.AUDIBLE);
        watcher.assertExpected();

        // Expect MUTED
        watcher =
                HistogramWatcher.newSingleRecordWatcher("Tab.Android.MediaState", MediaState.MUTED);
        setMuteState(true);
        waitForMediaState(mTab, MediaState.MUTED);
        watcher.assertExpected();

        // Expect NONE
        watcher =
                HistogramWatcher.newSingleRecordWatcher("Tab.Android.MediaState", MediaState.NONE);
        // Pause video.
        DOMUtils.pauseMedia(mTab.getWebContents(), VIDEO_ID);
        DOMUtils.waitForMediaPauseBeforeEnd(mTab.getWebContents(), VIDEO_ID);

        // Wait for the recently audible state to clear.
        waitForMediaState(mTab, MediaState.NONE);
        watcher.assertExpected();

        // Expect RECORDING
        watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Tab.Android.MediaState", MediaState.RECORDING);
        requestMic();
        waitForMediaState(mTab, MediaState.RECORDING);
        watcher.assertExpected();

        if (isPiPEnabled()) {
            // Remove the mic recording so we can drop down to NONE and avoid flakiness with PiP.
            stopMic();
            waitForMediaState(mTab, MediaState.NONE);

            // Expect PICTURE_IN_PICTURE
            watcher =
                    HistogramWatcher.newSingleRecordWatcher(
                            "Tab.Android.MediaState", MediaState.PICTURE_IN_PICTURE);
            enterPictureInPicture();
            waitForMediaState(mTab, MediaState.PICTURE_IN_PICTURE);
            watcher.assertExpected();
        }
    }

    @Test
    @SmallTest
    @Restriction(DeviceFormFactor.DESKTOP)
    public void testMediaStateSharing() {
        // Expect SHARING
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Tab.Android.MediaState", MediaState.SHARING);
        startTabCapture(mTab, mTab);
        watcher.assertExpected();

        // Expect NONE
        watcher =
                HistogramWatcher.newSingleRecordWatcher("Tab.Android.MediaState", MediaState.NONE);
        stopTabCapture(mTab);
        waitForMediaState(mTab, MediaState.NONE);
        watcher.assertExpected();
    }

    private void enterPictureInPicture() {
        DOMUtils.clickNodeWithJavaScript(mTab.getWebContents(), REQUEST_PIP_ID);
    }

    private void exitPictureInPicture() {
        DOMUtils.clickNodeWithJavaScript(mTab.getWebContents(), EXIT_PIP_ID);
    }

    private void requestMic() {
        DOMUtils.clickNodeWithJavaScript(mTab.getWebContents(), REQUEST_MIC_ID);
        waitForTitle(mTab, "mic_ready");
    }

    private void requestCam() {
        DOMUtils.clickNodeWithJavaScript(mTab.getWebContents(), REQUEST_CAM_ID);
        waitForTitle(mTab, "cam_ready");
    }

    private Tab openNewTabWith(String path) {
        mPage =
                mPage.openNewTabFast()
                        .loadWebPageProgrammatically(
                                mActivityTestRule.getTestServer().getURL(path));
        return mPage.getTab();
    }

    private void grantRecordingPermissions() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // The test page is loaded at the top level, so the requesting
                    // (primary) and embedding (secondary) origins are the same.
                    GURL url = mTab.getUrl();
                    WebsitePreferenceBridge.setContentSettingDefaultScope(
                            mTab.getProfile(),
                            ContentSettingsType.MEDIASTREAM_MIC,
                            /* primaryUrl= */ url,
                            /* secondaryUrl= */ url,
                            ContentSetting.ALLOW);
                    WebsitePreferenceBridge.setContentSettingDefaultScope(
                            mTab.getProfile(),
                            ContentSettingsType.MEDIASTREAM_CAMERA,
                            /* primaryUrl= */ url,
                            /* secondaryUrl= */ url,
                            ContentSetting.ALLOW);
                });
    }

    private void startTabCapture(Tab capturer, Tab capturee) {
        // Reset before triggering: tests that capture more than once would otherwise see the flag
        // left true by the previous capture and skip the verification below.
        mMediaPickerDelegate.mCreateScreenCaptureIntentCalled = false;
        DOMUtils.clickNodeWithJavaScript(capturer.getWebContents(), REQUEST_TAB_CAPTURE_ID);
        CriteriaHelper.pollUiThread(
                () -> {
                    if ("capture_error".equals(capturer.getTitle())) {
                        fail("Tab capture failed with title: capture_error");
                    }
                    Criteria.checkThat(
                            "createScreenCaptureIntent was not called",
                            mMediaPickerDelegate.mCreateScreenCaptureIntentCalled,
                            Matchers.is(true));
                });
        waitForTitle(capturer, "stream_ready");
        waitForAlertState(capturee, TabAlert.TAB_CAPTURING);
    }

    private void stopTabCapture(Tab capturer) {
        DOMUtils.clickNodeWithJavaScript(capturer.getWebContents(), STOP_TAB_CAPTURE_ID);
        waitForTitle(capturer, "stopped_successfully");
    }

    private void stopMic() {
        JavaScriptUtils.executeJavaScript(mTab.getWebContents(), "stopMic();");
        waitForTitle(mTab, "mic_stopped");
    }

    private void waitForAlertState(Tab tab, @TabAlert int expectedState) {
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            "Tab alert state should be " + expectedState,
                            tab.getAlertState(),
                            Matchers.is(expectedState));
                });
    }

    private void waitForMediaState(Tab tab, @MediaState int expectedState) {
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            "Tab media state should be " + expectedState,
                            tab.getMediaState(),
                            Matchers.is(expectedState));
                });
    }

    private void waitForTitle(Tab tab, String expectedTitle) {
        CriteriaHelper.pollUiThread(
                () -> {
                    String title = tab.getTitle();
                    // The page reports acquisition failures as "<stream>_error" titles. Those are
                    // terminal, so fail immediately rather than polling until the timeout.
                    if (title.endsWith("_error")) {
                        fail("Media stream acquisition failed with title: " + title);
                    }
                    Criteria.checkThat(
                            "Tab title should be " + expectedTitle,
                            title,
                            Matchers.is(expectedTitle));
                },
                CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL + TITLE_TIMEOUT_SLACK_MS,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    private void closeTab(Tab tab) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabRemover tabRemover = mTabModel.getTabRemover();
                    tabRemover.closeTabs(
                            TabClosureParams.closeTab(tab).allowUndo(false).build(),
                            /* allowDialog= */ false);
                });
    }

    private void setMuteState(boolean mute) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabModel.setMuteSetting(List.of(mTab), mute);
                });
    }

    private void selectTab(Tab tab) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabModel.setIndex(mTabModel.indexOf(tab), TabSelectionType.FROM_USER);
                });
    }

    private boolean isPiPEnabled() {
        return PictureInPicture.isEnabled(mActivityTestRule.getActivity());
    }
}
