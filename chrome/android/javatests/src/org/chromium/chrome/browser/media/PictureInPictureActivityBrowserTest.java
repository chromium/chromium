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
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.media.MediaSwitches;
import org.chromium.ui.test.util.DeviceRestriction;

import java.util.List;
import java.util.concurrent.TimeoutException;

/** End-to-end browser tests for PictureInPictureActivity. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    ContentSwitches.AUTO_ACCEPT_CAMERA_AND_MICROPHONE_CAPTURE,
    ContentSwitches.USE_FAKE_DEVICE_FOR_MEDIA_STREAM,
    MediaSwitches.AUTOPLAY_NO_GESTURE_REQUIRED_POLICY
})
@Restriction({
    DeviceRestriction.RESTRICTION_TYPE_NON_AUTO,
    Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE
})
@DisableIf.Build(sdk_is_less_than = VERSION_CODES.R) // crbug.com/452162997
public class PictureInPictureActivityBrowserTest {
    @Rule
    public AutoResetCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.autoResetCtaActivityRule();

    private ChromeTabbedActivity mActivity;
    private WebPageStation mPage;

    private static final String VIDEO_ID = "video";
    private static final String PIP_BUTTON_ID = "pip";
    private static final String VIDEO_PAGE =
            "/chrome/test/data/media/picture-in-picture/autopip-video.html";
    private static final String VIDEO_CONFERENCING_PAGE =
            "/chrome/test/data/media/picture-in-picture/video-conferencing-usermedia.html";

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

        PictureInPictureActivity pipActivity = enterPip(webContents);
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

        PictureInPictureActivity pipActivity = enterPip(webContents);
        // Wait for remote actions to be loaded in the pip activity. This wait reduces the flakiness
        // where the pip window is "hide" too quickly, and the action is ignored.
        waitForRemoteActions(pipActivity);
        closePip(pipActivity);
        // Conference video should still be playing after closing pip.
        assertFalse(
                "Conference video should still be playing after closing PiP",
                DOMUtils.isMediaPaused(webContents, VIDEO_ID));
    }

    private PictureInPictureActivity enterPip(WebContents webContents) {
        DOMUtils.clickNodeWithJavaScript(webContents, PIP_BUTTON_ID);

        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    try {
                        // Check that the element in PiP is our video element.
                        return JavaScriptUtils.executeJavaScriptAndWaitForResult(
                                        webContents,
                                        "document.pictureInPictureElement && "
                                                + "document.pictureInPictureElement.id == '"
                                                + VIDEO_ID
                                                + "'")
                                .equals("true");
                    } catch (TimeoutException e) {
                        return false;
                    }
                },
                "Video element did not enter Picture-in-Picture mode.");

        return getPictureInPictureActivity();
    }

    private PictureInPictureActivity getPictureInPictureActivity() {
        final Activity[] activityHolder = new Activity[1];
        CriteriaHelper.pollUiThread(
                () -> {
                    List<Activity> activities = ApplicationStatus.getRunningActivities();
                    for (Activity activity : activities) {
                        if (activity instanceof PictureInPictureActivity) {
                            activityHolder[0] = activity;
                            return true;
                        }
                    }
                    return false;
                },
                "Could not find PictureInPictureActivity.");
        return (PictureInPictureActivity) activityHolder[0];
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
        mPage = mPage.loadWebPageProgrammatically(url);

        return mActivityTestRule.getWebContents();
    }

    private void waitForRemoteActions(PictureInPictureActivity pipActivity) {
        CriteriaHelper.pollUiThread(
                () -> {
                    return !pipActivity.getActionsForTesting().isEmpty();
                },
                "No remote action is loaded.");
    }
}
