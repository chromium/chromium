// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import static org.junit.Assert.assertFalse;

import android.app.Activity;
import android.os.Build.VERSION_CODES;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.transit.Condition;
import org.chromium.base.test.transit.ConditionWithResult;
import org.chromium.base.test.transit.SimpleConditions;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.transit.HtmlElement;
import org.chromium.content_public.browser.test.transit.WebPageFacility;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.media.MediaSwitches;
import org.chromium.ui.test.util.DeviceRestriction;

import java.util.concurrent.TimeoutException;

/** End-to-end browser tests for PictureInPictureActivity. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({
    ContentSwitches.AUTO_ACCEPT_CAMERA_AND_MICROPHONE_CAPTURE,
    ContentSwitches.USE_FAKE_DEVICE_FOR_MEDIA_STREAM,
    MediaSwitches.AUTOPLAY_NO_GESTURE_REQUIRED_POLICY
})
@Restriction(DeviceRestriction.RESTRICTION_TYPE_NON_AUTO)
// Web Picture-in-Picture requires Android 11+.
@DisableIf.Build(sdk_is_less_than = VERSION_CODES.R)
public class PictureInPictureActivityBrowserTest {
    @Rule
    public AutoResetCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.autoResetCtaActivityRule();

    private ChromeTabbedActivity mActivity;
    private WebPageStation mPage;
    private PipVideoPageFacility mPipFacility;

    private static final String VIDEO_ID = "video";
    private static final String PIP_BUTTON_ID = "pip";
    private static final String VIDEO_PAGE =
            "/chrome/test/data/media/picture-in-picture/autopip-video.html";
    private static final String VIDEO_CONFERENCING_PAGE =
            "/chrome/test/data/media/picture-in-picture/video-conferencing-usermedia.html";

    private static class PipVideoPageFacility extends WebPageFacility {
        public final HtmlElement pipButton = declareHtmlElement("#" + PIP_BUTTON_ID);
    }

    @Before
    public void setUp() {
        // Some of the tests may finish the activity using moveTaskToBack.
        ChromeTabbedActivity.interceptMoveTaskToBackForTesting();
        mPage = mActivityTestRule.startOnBlankPage();
        mActivity = mPage.getActivity();
    }

    @After
    public void tearDown() {
        if (mActivity != null) {
            ApplicationTestUtils.finishActivity(mActivity);
        }
    }

    @Test
    @MediumTest
    public void testClosePipForMediaPlayback() throws TimeoutException {
        WebContents webContents = loadUrlAndInitializeForTest(VIDEO_PAGE);
        DOMUtils.playMedia(webContents, VIDEO_ID);
        DOMUtils.waitForMediaPlay(webContents, VIDEO_ID);

        PictureInPictureActivity pipActivity = enterPip();
        closePip(pipActivity);
        // Video should be paused after closing pip.
        CriteriaHelper.pollInstrumentationThread(
                () -> DOMUtils.isMediaPaused(webContents, VIDEO_ID),
                "Video was not paused after closing PiP");
    }

    @Test
    @MediumTest
    public void testClosePipForConferenceVideo() throws TimeoutException {
        WebContents webContents = loadUrlAndInitializeForTest(VIDEO_CONFERENCING_PAGE);
        DOMUtils.playMedia(webContents, VIDEO_ID);
        DOMUtils.waitForMediaPlay(webContents, VIDEO_ID);

        PictureInPictureActivity pipActivity = enterPip();
        closePip(pipActivity);
        // Conference video should still be playing after closing pip.
        assertFalse(
                "Conference video should still be playing after closing PiP",
                DOMUtils.isMediaPaused(webContents, VIDEO_ID));
    }

    @Test
    @MediumTest
    public void testReplayAtEndOfVideo() throws TimeoutException {
        WebContents webContents = loadUrlAndInitializeForTest(VIDEO_PAGE);
        DOMUtils.playMedia(webContents, VIDEO_ID);
        DOMUtils.waitForMediaPlay(webContents, VIDEO_ID);

        PictureInPictureActivity pipActivity = enterPip();
        waitForRemoteActions(pipActivity);

        // The test video has a 'loop' attribute. Remove it so the video can actually end.
        JavaScriptUtils.executeJavaScriptAndWaitForResult(
                webContents,
                "document.getElementById('" + VIDEO_ID + "').removeAttribute('loop');");

        // Seek the video to the end to trigger the end of video state.
        DOMUtils.seekMediaToEnd(webContents, VIDEO_ID);

        // Wait until the video actually ends.
        CriteriaHelper.pollInstrumentationThread(
                () -> DOMUtils.isMediaEnded(webContents, VIDEO_ID), "Video did not reach the end");

        // Simulate clicking the replay button which will be mapped to MediaSessionAction.PLAY.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    try {
                        pipActivity.mMediaActionsButtonsManager.mReplay.getActionIntent().send();
                    } catch (android.app.PendingIntent.CanceledException e) {
                        throw new RuntimeException(e);
                    }
                });

        // Wait until the video starts playing again.
        CriteriaHelper.pollInstrumentationThread(
                () ->
                        !DOMUtils.isMediaEnded(webContents, VIDEO_ID)
                                && !DOMUtils.isMediaPaused(webContents, VIDEO_ID),
                "Video did not start playing after replay");

        closePip(pipActivity);
    }

    private PictureInPictureActivity enterPip() {
        ConditionWithResult<PictureInPictureActivity> pipActivityCondition =
                SimpleConditions.uiThreadConditionWithResult(
                        "PictureInPictureActivity is in PiP mode",
                        mPage.getActivityElement(),
                        activity -> {
                            for (Activity runningActivity :
                                    ApplicationStatus.getRunningActivities()) {
                                if (runningActivity
                                        instanceof PictureInPictureActivity pipActivity) {
                                    if (pipActivity.isPipTransitionCompleteForTesting(activity)) {
                                        return Condition.fulfilled().withResult(pipActivity);
                                    }
                                    return Condition.notFulfilled(
                                                    "PictureInPictureActivity transition"
                                                            + " incomplete")
                                            .withoutResult();
                                }
                            }
                            return Condition.notFulfilled("PictureInPictureActivity not running")
                                    .withoutResult();
                        });
        mPipFacility
                .pipButton
                .jsClickTo()
                .waitFor(
                        SimpleConditions.instrumentationThreadCondition(
                                "Video element entered Picture-in-Picture mode",
                                mPage.webContentsElement,
                                webContents -> {
                                    try {
                                        // Check that the element in PiP is our video element.
                                        String result =
                                                JavaScriptUtils.executeJavaScriptAndWaitForResult(
                                                        webContents,
                                                        "document.pictureInPictureElement"
                                                                + " ? document"
                                                                + ".pictureInPictureElement.id"
                                                                + " : null");
                                        return Condition.whether(
                                                ("\"" + VIDEO_ID + "\"").equals(result),
                                                "pictureInPictureElement.id=%s",
                                                result);
                                    } catch (TimeoutException e) {
                                        return Condition.error("JS timed out: %s", e.getMessage());
                                    }
                                }),
                        pipActivityCondition);
        return pipActivityCondition.get();
    }

    private void closePip(PictureInPictureActivity activity) {
        // Use finish() to simulate a user/system close. This triggers the correct
        // lifecycle (onPictureInPictureModeChanged).
        ThreadUtils.runOnUiThreadBlocking(() -> activity.finish());

        CriteriaHelper.pollUiThread(
                () -> activity == null || activity.isDestroyed(),
                "Could not close PictureInPictureActivity.");
    }

    private WebContents loadUrlAndInitializeForTest(String pageUrl) {
        String url = mActivityTestRule.getTestServer().getURL(pageUrl);
        mPipFacility = new PipVideoPageFacility();
        mPage =
                mPage.loadPageProgrammatically(
                        url, WebPageStation.newBuilder().withFacility(mPipFacility));

        return mPage.webContentsElement.value();
    }

    private void waitForRemoteActions(PictureInPictureActivity pipActivity) {
        CriteriaHelper.pollUiThread(
                () -> {
                    return !pipActivity.getActionsForTesting().isEmpty();
                },
                "No remote action is loaded.");
    }
}
