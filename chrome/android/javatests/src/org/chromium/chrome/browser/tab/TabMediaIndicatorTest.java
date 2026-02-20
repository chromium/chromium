// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.fail;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;
import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.app.Activity;
import android.app.Instrumentation;
import android.content.Context;
import android.content.Intent;
import android.view.View;

import androidx.test.espresso.ViewInteraction;
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
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.media.MediaCapturePickerDelegate;
import org.chromium.chrome.browser.media.MediaCapturePickerManager;
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
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.media.MediaSwitches;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.List;
import java.util.concurrent.TimeoutException;

/** Tests for {@link Tab.MediaState}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    MediaSwitches.AUTOPLAY_NO_GESTURE_REQUIRED_POLICY,
    ContentSwitches.USE_FAKE_DEVICE_FOR_MEDIA_STREAM,
    "enable-experimental-web-platform-features",
    "enable-features=UserMediaScreenCapturing,EnableAudioMonitoringOnAndroid,AndroidMediaPicker,"
            + ChromeFeatureList.MEDIA_INDICATORS_ANDROID
            + ":sharing/true",
})
@Batch(Batch.PER_CLASS)
@Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
@EnableFeatures({ChromeFeatureList.ANDROID_NEW_MEDIA_PICKER})
public class TabMediaIndicatorTest {
    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    private static final String TEST_PATH = "/chrome/test/data/media/tab_media_indicator.html";
    private static final String GOOGLE_PATH = "/chrome/test/data/android/google.html";
    private static final String VIDEO_ID = "video";
    private static final String MUTE_VIDEO_ID = "mute";
    private static final String UNMUTE_VIDEO_ID = "unmute";
    private static final String REQUEST_MIC_ID = "request-mic";
    private static final String REQUEST_CAM_ID = "request-cam";
    private static final String REQUEST_TAB_CAPTURE_ID = "request-tab-capture";
    private static final String STOP_TAB_CAPTURE_ID = "stop-tab-capture";
    private static final long WAIT = 2000;

    private WebPageStation mPage;
    private TabModel mTabModel;
    private TabRemover mTabRemover;
    private Tab mTab;
    private MockMediaCapturePickerDelegate mMockDelegate;

    private class MockMediaCapturePickerDelegate implements MediaCapturePickerDelegate {
        private Tab mPickedTab;
        private Intent mScreenCaptureIntent;
        public boolean mCreateScreenCaptureIntentCalled;

        public void setPickedTab(Tab tab) {
            mPickedTab = tab;
        }

        @Override
        public Intent createScreenCaptureIntent(
                Context context, MediaCapturePickerManager.Params params) {
            mCreateScreenCaptureIntentCalled = true;
            return mScreenCaptureIntent != null
                    ? mScreenCaptureIntent
                    : new Intent("CUSTOM_ACTION");
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
        mPage = mActivityTestRule.startOnBlankPage();
        mTabModel = mActivityTestRule.getActivity().getTabModelSelector().getModel(false);
        mTabRemover = mTabModel.getTabRemover();
        mTab = mPage.getTab();

        new TabLoadObserver(mTab).fullyLoadUrl(mActivityTestRule.getTestServer().getURL(TEST_PATH));
        DOMUtils.waitForNonZeroNodeBounds(mTab.getWebContents(), VIDEO_ID);
        assertEquals(Tab.MediaState.NONE, mTab.getMediaState());

        ForegroundServiceUtils.setInstanceForTesting(Mockito.mock(ForegroundServiceUtils.class));

        mMockDelegate = new MockMediaCapturePickerDelegate();
        ServiceLoaderUtil.setInstanceForTesting(MediaCapturePickerDelegate.class, mMockDelegate);
        Intents.init();
        Intents.intending(IntentMatchers.hasAction("CUSTOM_ACTION"))
                .respondWith(new Instrumentation.ActivityResult(Activity.RESULT_OK, new Intent()));
    }

    @After
    public void tearDown() {
        Intents.release();
    }

    @Test
    @SmallTest
    public void testMediaStateAudible() throws TimeoutException {
        DOMUtils.playMedia(mTab.getWebContents(), VIDEO_ID);
        DOMUtils.waitForMediaPlay(mTab.getWebContents(), VIDEO_ID);
        waitForMediaState(mTab, Tab.MediaState.AUDIBLE);
    }

    @Test
    @SmallTest
    public void testMediaStateMuted() throws TimeoutException {
        setMuteState(true);
        DOMUtils.playMedia(mTab.getWebContents(), VIDEO_ID);
        DOMUtils.waitForMediaPlay(mTab.getWebContents(), VIDEO_ID);
        waitForMediaState(mTab, Tab.MediaState.MUTED);
    }

    @Test
    @SmallTest
    public void testMediaStateMutedThenUnmute() throws TimeoutException {
        setMuteState(true);
        DOMUtils.playMedia(mTab.getWebContents(), VIDEO_ID);
        DOMUtils.waitForMediaPlay(mTab.getWebContents(), VIDEO_ID);
        waitForMediaState(mTab, Tab.MediaState.MUTED);
        setMuteState(false);
        waitForMediaState(mTab, Tab.MediaState.AUDIBLE);
    }

    @Test
    @SmallTest
    public void testMediaStateAudibleThenMute() throws TimeoutException {
        DOMUtils.playMedia(mTab.getWebContents(), VIDEO_ID);
        DOMUtils.waitForMediaPlay(mTab.getWebContents(), VIDEO_ID);
        waitForMediaState(mTab, Tab.MediaState.AUDIBLE);
        setMuteState(true);
        waitForMediaState(mTab, Tab.MediaState.MUTED);
    }

    @Test
    @SmallTest
    public void testMediaStateAudibleMuteWithPause() throws Exception {
        DOMUtils.playMedia(mTab.getWebContents(), VIDEO_ID);
        DOMUtils.waitForMediaPlay(mTab.getWebContents(), VIDEO_ID);
        waitForMediaState(mTab, Tab.MediaState.AUDIBLE);

        // Pause video.
        DOMUtils.pauseMedia(mTab.getWebContents(), VIDEO_ID);
        DOMUtils.waitForMediaPauseBeforeEnd(mTab.getWebContents(), VIDEO_ID);

        // Wait for the recently audible state to clear.
        waitForMediaState(mTab, Tab.MediaState.NONE);

        // Mute video.
        setMuteState(true);
        assertEquals(Tab.MediaState.NONE, mTab.getMediaState());

        // Play the video again.
        DOMUtils.playMedia(mTab.getWebContents(), VIDEO_ID);
        DOMUtils.waitForMediaPlay(mTab.getWebContents(), VIDEO_ID);
        waitForMediaState(mTab, Tab.MediaState.MUTED);
    }

    @Test
    @SmallTest
    public void testMediaStateWithVideoMutedAndUnmuted() throws Exception {
        DOMUtils.playMedia(mTab.getWebContents(), VIDEO_ID);
        DOMUtils.waitForMediaPlay(mTab.getWebContents(), VIDEO_ID);
        waitForMediaState(mTab, Tab.MediaState.AUDIBLE);

        // Mute video element.
        DOMUtils.clickNodeWithJavaScript(mTab.getWebContents(), MUTE_VIDEO_ID);

        // Wait for the recently audible state to clear.
        assertFalse(DOMUtils.isMediaPaused(mTab.getWebContents(), VIDEO_ID));
        waitForMediaState(mTab, Tab.MediaState.NONE);

        // Unmute video element.
        DOMUtils.clickNodeWithJavaScript(mTab.getWebContents(), UNMUTE_VIDEO_ID);
        waitForMediaState(mTab, Tab.MediaState.AUDIBLE);
    }

    @Test
    @SmallTest
    public void testMediaStateRecordingMic() throws InterruptedException {
        requestRecording(REQUEST_MIC_ID);
        waitForMediaState(mTab, Tab.MediaState.RECORDING);
    }

    @Test
    @SmallTest
    public void testMediaStateRecordingCam() throws InterruptedException {
        requestRecording(REQUEST_CAM_ID);
        waitForMediaState(mTab, Tab.MediaState.RECORDING);
    }

    @Test
    @SmallTest
    public void testMediaStatePriority() throws Exception {
        assertEquals(Tab.MediaState.NONE, mTab.getMediaState());

        // MUTED
        setMuteState(true);
        DOMUtils.playMedia(mTab.getWebContents(), VIDEO_ID);
        DOMUtils.waitForMediaPlay(mTab.getWebContents(), VIDEO_ID);
        waitForMediaState(mTab, Tab.MediaState.MUTED);

        // AUDIBLE
        setMuteState(false);
        waitForMediaState(mTab, Tab.MediaState.AUDIBLE);

        // RECORDING
        requestRecording(REQUEST_MIC_ID);
        waitForMediaState(mTab, Tab.MediaState.RECORDING);
    }

    @Test
    @SmallTest
    @Restriction(DeviceFormFactor.DESKTOP)
    public void testMediaStateSharing() throws InterruptedException {
        assertEquals(Tab.MediaState.NONE, mTab.getMediaState());

        // Expect SHARING
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Tab.Android.MediaState", Tab.MediaState.SHARING);
        startTabCapture(mTab, mTab);
        watcher.assertExpected();

        // Expect NONE
        watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Tab.Android.MediaState", Tab.MediaState.NONE);
        stopTabCapture(mTab);
        waitForMediaState(mTab, Tab.MediaState.NONE);
        watcher.assertExpected();
    }

    @Test
    @SmallTest
    @Restriction(DeviceFormFactor.DESKTOP)
    public void testMediaStateSharingOverridesRecording() throws Exception {
        requestRecording(REQUEST_MIC_ID);
        waitForMediaState(mTab, Tab.MediaState.RECORDING);

        startTabCapture(mTab, mTab);
        stopTabCapture(mTab);
        waitForMediaState(mTab, Tab.MediaState.RECORDING);
    }

    @Test
    @SmallTest
    @Restriction(DeviceFormFactor.DESKTOP)
    public void testMediaStateSharingNewTab() throws Exception {
        mPage =
                mPage.openNewTabFast()
                        .loadWebPageProgrammatically(
                                mActivityTestRule.getTestServer().getURL(GOOGLE_PATH));
        Tab newTab = mPage.getTab();

        // Pick the new tab to be captured
        mMockDelegate.setPickedTab(newTab);

        selectTab(mTab);
        startTabCapture(mTab, newTab);
        stopTabCapture(mTab);
        waitForMediaState(newTab, Tab.MediaState.NONE);
    }

    @Test
    @SmallTest
    @Restriction(DeviceFormFactor.DESKTOP)
    public void testMediaStateSharingDisappearsWhenCapturerTabIsClosed() throws Exception {
        mPage =
                mPage.openNewTabFast()
                        .loadWebPageProgrammatically(
                                mActivityTestRule.getTestServer().getURL(GOOGLE_PATH));
        Tab newTab = mPage.getTab();

        // Pick the new tab to be captured
        mMockDelegate.setPickedTab(newTab);

        selectTab(mTab);
        startTabCapture(mTab, newTab);

        closeTab(mTab);
        waitForMediaState(newTab, Tab.MediaState.NONE);
    }

    @Test
    @SmallTest
    @Restriction(DeviceFormFactor.DESKTOP)
    public void testMediaStateSharingWithTwoCapturers() throws Exception {
        Tab capturer1Tab = mTab;

        // Create and setup a capturee tab.
        mPage =
                mPage.openNewTabFast()
                        .loadWebPageProgrammatically(
                                mActivityTestRule.getTestServer().getURL(GOOGLE_PATH));
        Tab captureeTab = mPage.getTab();

        // Create and setup a second capturer tab.
        mPage =
                mPage.openNewTabFast()
                        .loadWebPageProgrammatically(
                                mActivityTestRule.getTestServer().getURL(TEST_PATH));
        Tab capturer2Tab = mPage.getTab();
        DOMUtils.waitForNonZeroNodeBounds(capturer2Tab.getWebContents(), REQUEST_TAB_CAPTURE_ID);

        // Start capture from the first tab.
        selectTab(capturer1Tab);
        mMockDelegate.setPickedTab(captureeTab);
        startTabCapture(capturer1Tab, captureeTab);

        // Start capture from the second tab.
        selectTab(capturer2Tab);
        mMockDelegate.setPickedTab(captureeTab);
        startTabCapture(capturer2Tab, captureeTab);

        // Stop capture from the first tab and verify the indicator is still present.
        selectTab(capturer1Tab);
        stopTabCapture(capturer1Tab);
        // The media state should persist as the second capturer is still active.
        assertEquals(Tab.MediaState.SHARING, captureeTab.getMediaState());

        // Stop capture from the second tab and verify the indicator is gone.
        selectTab(capturer2Tab);
        stopTabCapture(capturer2Tab);
        waitForMediaState(captureeTab, Tab.MediaState.NONE);
    }

    @Test
    @SmallTest
    @Restriction(DeviceFormFactor.DESKTOP)
    public void testMediaStateSharingDisappearsWhenCaptureeTabIsClosed() throws Exception {
        mPage =
                mPage.openNewTabFast()
                        .loadWebPageProgrammatically(
                                mActivityTestRule.getTestServer().getURL(GOOGLE_PATH));
        Tab captureeTab = mPage.getTab();

        // Pick the new tab to be captured
        mMockDelegate.setPickedTab(captureeTab);

        // mTab is the capturer tab.
        selectTab(mTab);
        startTabCapture(mTab, captureeTab);

        // When the `onended` event is fired on the track, the title will be updated to 'ended'. We
        // wait for listener_attached before proceeding to close the capturee tab.
        JavaScriptUtils.executeJavaScriptAndWaitForResult(
                mTab.getWebContents(), "monitorStreamEnd();");
        waitForTitle(mTab, "listener_attached");

        closeTab(captureeTab);

        // After capturee is closed, the sharing should stop.
        waitForTitle(mTab, "ended");
        waitForMediaState(captureeTab, Tab.MediaState.NONE);
    }

    @Test
    @SmallTest
    public void testMediaStateHistogram() throws Exception {
        assertEquals(Tab.MediaState.NONE, mTab.getMediaState());

        // Expect AUDIBLE
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Tab.Android.MediaState", Tab.MediaState.AUDIBLE);
        DOMUtils.playMedia(mTab.getWebContents(), VIDEO_ID);
        DOMUtils.waitForMediaPlay(mTab.getWebContents(), VIDEO_ID);
        waitForMediaState(mTab, Tab.MediaState.AUDIBLE);
        watcher.assertExpected();

        // Expect MUTED
        watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Tab.Android.MediaState", Tab.MediaState.MUTED);
        setMuteState(true);
        waitForMediaState(mTab, Tab.MediaState.MUTED);
        watcher.assertExpected();

        // Expect NONE
        watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Tab.Android.MediaState", Tab.MediaState.NONE);
        // Pause video.
        DOMUtils.pauseMedia(mTab.getWebContents(), VIDEO_ID);
        DOMUtils.waitForMediaPauseBeforeEnd(mTab.getWebContents(), VIDEO_ID);

        // Wait for the recently audible state to clear.
        waitForMediaState(mTab, Tab.MediaState.NONE);
        watcher.assertExpected();

        // Expect RECORDING
        watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Tab.Android.MediaState", Tab.MediaState.RECORDING);
        requestRecording(REQUEST_MIC_ID);
        waitForMediaState(mTab, Tab.MediaState.RECORDING);
        watcher.assertExpected();
    }

    private void requestRecording(String id) throws InterruptedException {
        DOMUtils.clickNodeWithJavaScript(mTab.getWebContents(), id);

        @ContentSettingsType.EnumType
        int contentSettingsType =
                REQUEST_MIC_ID.equals(id)
                        ? ContentSettingsType.MEDIASTREAM_MIC
                        : ContentSettingsType.MEDIASTREAM_CAMERA;

        @ContentSetting
        int contentSetting =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return WebsitePreferenceBridge.getContentSetting(
                                    mTab.getProfile(),
                                    contentSettingsType,
                                    mTab.getUrl(),
                                    mTab.getUrl());
                        });

        if (contentSetting == ContentSetting.ASK) {
            Thread.sleep(WAIT); // Reduce flakiness by waiting for the dialog to appear.

            // Remove accessibility checks to prevent flakiness.
            ViewInteraction viewInteraction =
                    onViewWaiting(withText(Matchers.is("Allow while visiting the site")));
            viewInteraction.check(
                    (view, noViewFoundException) -> {
                        if (view != null) {
                            view.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO);
                        }
                    });
            viewInteraction.perform(click());
        }
    }

    private void startTabCapture(Tab capturer, Tab capturee) {
        DOMUtils.clickNodeWithJavaScript(capturer.getWebContents(), REQUEST_TAB_CAPTURE_ID);
        CriteriaHelper.pollUiThread(
                () -> {
                    if ("capture_error".equals(capturer.getTitle())) {
                        fail("Tab capture failed with title: capture_error");
                    }
                    Criteria.checkThat(
                            "createScreenCaptureIntent was not called",
                            mMockDelegate.mCreateScreenCaptureIntentCalled,
                            Matchers.is(true));
                });
        waitForTitle(capturer, "stream_ready");
        waitForMediaState(capturee, Tab.MediaState.SHARING);
    }

    private void stopTabCapture(Tab capturer) {
        DOMUtils.clickNodeWithJavaScript(capturer.getWebContents(), STOP_TAB_CAPTURE_ID);
        waitForTitle(capturer, "stopped_successfully");
    }

    private void waitForMediaState(Tab tab, @Tab.MediaState int expectedState) {
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
                    if ("capture_error".equals(title)) {
                        fail("Tab capture failed with title: " + title);
                    }
                    Criteria.checkThat(
                            "Tab title should be " + expectedTitle,
                            title,
                            Matchers.is(expectedTitle));
                },
                CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL + WAIT,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    private void closeTab(Tab tab) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabRemover.closeTabs(
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
}
