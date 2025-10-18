// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.picture_in_picture;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;
import static org.chromium.ui.test.util.DeviceRestriction.RESTRICTION_TYPE_NON_AUTO;

import android.app.Activity;
import android.app.RemoteAction;
import android.content.res.Configuration;
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
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.blink_public.common.BlinkFeatures;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.media.PictureInPictureActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.components.content_settings.ContentSetting;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.media.MediaFeatures;
import org.chromium.media.MediaSwitches;

import java.util.List;
import java.util.concurrent.TimeoutException;

/** Test suite for {@link AutoPictureInPictureTabHelper}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    ContentSwitches.AUTO_ACCEPT_CAMERA_AND_MICROPHONE_CAPTURE,
    ContentSwitches.USE_FAKE_DEVICE_FOR_MEDIA_STREAM,
    MediaSwitches.AUTOPLAY_NO_GESTURE_REQUIRED_POLICY,
})
@EnableFeatures({
    BlinkFeatures.MEDIA_SESSION_ENTER_PICTURE_IN_PICTURE,
    MediaFeatures.AUTO_PICTURE_IN_PICTURE_ANDROID,
    MediaFeatures.AUTO_PICTURE_IN_PICTURE_FOR_VIDEO_PLAYBACK
})
@Restriction({RESTRICTION_TYPE_NON_AUTO, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
@DisableIf.Build(sdk_is_less_than = VERSION_CODES.R) // crbug.com/430452403
@Batch(Batch.PER_CLASS)
public class AutoPictureInPictureTabHelperTest {
    @Rule
    public final FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    private ChromeTabbedActivity mActivity;
    private WebPageStation mPage;

    private static final String VIDEO_ID = "video";
    private static final String PIP_BUTTON_ID = "pip";
    private static final String AUTO_PIP_VIDEO_PAGE =
            "/chrome/test/data/media/picture-in-picture/autopip-video.html";
    private static final String AUTO_PIP_NOT_REGISTERED_PAGE =
            "/chrome/test/data/media/picture-in-picture/autopip-no-register.html";
    private static final String VIDEO_CONFERENCING_PAGE =
            "/chrome/test/data/media/picture-in-picture/video-conferencing-usermedia.html";

    @Before
    public void setUp() {
        mActivityTestRule.getEmbeddedTestServerRule().setServerUsesHttps(true);
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
    public void testCanAutopipWithMediaPlaying() throws TimeoutException {
        WebContents webContents = loadUrlAndInitializeForTest(AUTO_PIP_VIDEO_PAGE);
        // Verify if the loaded page registers auto pip.
        assertTrue(
                AutoPictureInPictureTabHelperTestUtils.hasAutoPictureInPictureBeenRegistered(
                        webContents));

        // Create a new tab in the background to switch to later.
        Tab originalTab = mPage.getTab();
        Tab newTab = createNewTabInBackground(originalTab);
        AutoPictureInPictureTabHelperTestUtils.waitForAutoPictureInPictureState(
                webContents, false, "Should not enter auto-PiP with new background tab.");

        fulfillVideoPlaybackConditions(webContents);

        // Switch away from the tab. This should trigger auto-PiP.
        switchToTab(newTab);
        AutoPictureInPictureTabHelperTestUtils.waitForAutoPictureInPictureState(
                webContents, true, "Did not enter auto-PiP after tab hidden.");

        // Return to the tab. This should exit auto-PiP.
        switchToTab(originalTab);
        AutoPictureInPictureTabHelperTestUtils.waitForAutoPictureInPictureState(
                webContents, false, "Did not exit auto-PiP after tab shown.");
    }

    @Test
    @MediumTest
    public void testCanAutopipWithConferenceCall() {
        WebContents webContents = loadUrlAndInitializeForTest(VIDEO_CONFERENCING_PAGE);
        // Verify if the loaded page registers auto pip.
        assertTrue(
                AutoPictureInPictureTabHelperTestUtils.hasAutoPictureInPictureBeenRegistered(
                        webContents));
        DOMUtils.waitForMediaPlay(webContents, VIDEO_ID);

        // Create a new tab in the background to switch to later.
        Tab originalTab = mPage.getTab();
        Tab newTab = createNewTabInBackground(originalTab);
        AutoPictureInPictureTabHelperTestUtils.waitForAutoPictureInPictureState(
                webContents, false, "Should not enter auto-PiP with new background tab.");

        // Fulfill the camera/mic usage condition.
        AutoPictureInPictureTabHelperTestUtils.setIsUsingCameraOrMicrophone(webContents, true);

        // Switch away from the tab. This should trigger auto-PiP.
        switchToTab(newTab);
        AutoPictureInPictureTabHelperTestUtils.waitForAutoPictureInPictureState(
                webContents, true, "Did not enter auto-PiP after tab hidden.");

        // Return to the tab. This should exit auto-PiP.
        switchToTab(originalTab);
        AutoPictureInPictureTabHelperTestUtils.waitForAutoPictureInPictureState(
                webContents, false, "Did not exit auto-PiP after tab shown.");
    }

    @Test
    @MediumTest
    public void testHideAutoPip() throws TimeoutException {
        WebContents webContents = loadUrlAndInitializeForTest(AUTO_PIP_VIDEO_PAGE);
        Tab originalTab = mPage.getTab();
        PictureInPictureActivity pipActivity = enterAutoPip(webContents, originalTab);
        // After enterAutoPip, we are on a new tab.
        final Tab[] newTab = new Tab[1];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    newTab[0] = mActivity.getTabModelSelector().getCurrentTab();
                });
        assertNotEquals("Should be on a new tab after entering auto-PiP.", originalTab, newTab[0]);

        // Simulate clicking the hide button.
        ThreadUtils.runOnUiThreadBlocking(pipActivity::triggerHideActionForTesting);

        // Wait for the PictureInPictureActivity to be destroyed.
        CriteriaHelper.pollUiThread(pipActivity::isDestroyed);

        // Now that the activity is gone, verify the C++ state.
        AutoPictureInPictureTabHelperTestUtils.waitForAutoPictureInPictureState(
                webContents, false, "Did not exit auto-PiP after hide.");

        // Verify we are still on the new tab.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(
                            "Should still be on the new tab.",
                            newTab[0].getId(),
                            mActivity.getTabModelSelector().getCurrentTab().getId());
                });

        // Verify that the video is still playing on the original tab.
        switchToTab(originalTab);
        assertFalse(
                "Video should still be playing.", DOMUtils.isMediaPaused(webContents, VIDEO_ID));
    }

    @Test
    @MediumTest
    public void testManualPipDoesNotHaveHideAction() throws TimeoutException {
        WebContents webContents = loadUrlAndInitializeForTest(AUTO_PIP_VIDEO_PAGE);
        fulfillVideoPlaybackConditions(webContents);

        // Manually open PiP by simulating a click on the PiP button.
        DOMUtils.clickNodeWithJavaScript(webContents, PIP_BUTTON_ID);
        AutoPictureInPictureTabHelperTestUtils.waitForPictureInPictureVideoState(
                webContents, true, "Did not enter PiP after manual request.");

        PictureInPictureActivity pipActivity = getPictureInPictureActivity();
        assertNotNull("PictureInPictureActivity not found.", pipActivity);
        CriteriaHelper.pollUiThread(pipActivity::isInPictureInPictureMode);

        // Verify that the "Hide" action is not present for manually entered PiP.
        for (RemoteAction action : pipActivity.getActionsForTesting()) {
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        assertNotEquals(
                                "Hide action should not be present for manual PiP.",
                                action.getTitle(),
                                mActivity.getString(
                                        R.string.accessibility_listen_in_the_background));
                    });
        }
    }

    @Test
    @MediumTest
    public void testBackToTabFromAutoPip() throws TimeoutException {
        WebContents webContents = loadUrlAndInitializeForTest(AUTO_PIP_VIDEO_PAGE);
        assertTrue(
                "Page should have registered for auto-pip.",
                AutoPictureInPictureTabHelperTestUtils.hasAutoPictureInPictureBeenRegistered(
                        webContents));

        Tab originalTab = mPage.getTab();
        PictureInPictureActivity pipActivity = enterAutoPip(webContents, originalTab);

        // Simulate clicking the "back to tab" button.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Configuration config = pipActivity.getResources().getConfiguration();
                    pipActivity.onPictureInPictureModeChanged(false, config);
                });

        // Wait for the PictureInPictureActivity to be destroyed.
        CriteriaHelper.pollUiThread(pipActivity::isDestroyed);

        // Now that the activity is gone, verify the C++ state.
        AutoPictureInPictureTabHelperTestUtils.waitForAutoPictureInPictureState(
                webContents, false, "Did not exit auto-PiP after back-to-tab.");
    }

    @Test
    @MediumTest
    public void testDoesNotAutopipIfNotRegistered() throws TimeoutException {
        WebContents webContents = loadUrlAndInitializeForTest(AUTO_PIP_NOT_REGISTERED_PAGE);
        // Verify the page does not register for auto-pip.
        assertFalse(
                "Page should not have registered for auto-pip.",
                AutoPictureInPictureTabHelperTestUtils.hasAutoPictureInPictureBeenRegistered(
                        webContents));

        // Create a new tab in the background to switch to later.
        Tab originalTab = mPage.getTab();
        Tab newTab = createNewTabInBackground(originalTab);

        fulfillVideoPlaybackConditions(webContents);

        // Switch away from the tab.
        switchToTab(newTab);

        // Since the site did not register for auto-pip, it should not enter.
        AutoPictureInPictureTabHelperTestUtils.waitForAutoPictureInPictureState(
                webContents, false, "Should not enter auto-PiP if not registered.");
    }

    @Test
    @MediumTest
    public void testDoesNotAutopipWithoutPlayback() {
        WebContents webContents = loadUrlAndInitializeForTest(AUTO_PIP_VIDEO_PAGE);
        assertTrue(
                "Page should have registered for auto-pip.",
                AutoPictureInPictureTabHelperTestUtils.hasAutoPictureInPictureBeenRegistered(
                        webContents));

        // Create a new tab in the background to switch to later.
        Tab originalTab = mPage.getTab();
        Tab newTab = createNewTabInBackground(originalTab);

        // Fulfill media engagement conditions, but do not start playback.
        AutoPictureInPictureTabHelperTestUtils.setHasHighMediaEngagement(webContents, true);

        // Switch away from the tab.
        switchToTab(newTab);

        // Should not enter auto-PiP without playback.
        AutoPictureInPictureTabHelperTestUtils.waitForAutoPictureInPictureState(
                webContents, false, "Should not enter auto-PiP without playback.");
    }

    @Test
    @MediumTest
    public void testDoesNotAutopipWhenPaused() throws TimeoutException {
        WebContents webContents = loadUrlAndInitializeForTest(AUTO_PIP_VIDEO_PAGE);
        assertTrue(
                "Page should have registered for auto-pip.",
                AutoPictureInPictureTabHelperTestUtils.hasAutoPictureInPictureBeenRegistered(
                        webContents));

        // Create a new tab in the background to switch to later.
        Tab originalTab = mPage.getTab();
        Tab newTab = createNewTabInBackground(originalTab);

        fulfillVideoPlaybackConditions(webContents);
        // Pause playback.
        DOMUtils.pauseMedia(webContents, VIDEO_ID);
        DOMUtils.waitForMediaPauseBeforeEnd(webContents, VIDEO_ID);

        // Switch away from the tab.
        switchToTab(newTab);

        // Should not enter auto-PiP when paused.
        AutoPictureInPictureTabHelperTestUtils.waitForAutoPictureInPictureState(
                webContents, false, "Should not enter auto-PiP when paused.");
    }

    @Test
    @MediumTest
    public void testDoesNotCloseManuallyOpenedPip() throws TimeoutException {
        WebContents webContents = loadUrlAndInitializeForTest(AUTO_PIP_VIDEO_PAGE);
        assertTrue(
                "Page should have registered for auto-pip.",
                AutoPictureInPictureTabHelperTestUtils.hasAutoPictureInPictureBeenRegistered(
                        webContents));

        // Fulfill all conditions for auto-PiP to ensure its logic is exercised when the tab is
        // hidden later. This also starts video playback.
        fulfillVideoPlaybackConditions(webContents);

        // Manually open PiP by simulating a click on the PiP button.
        DOMUtils.clickNodeWithJavaScript(webContents, PIP_BUTTON_ID);
        AutoPictureInPictureTabHelperTestUtils.waitForPictureInPictureVideoState(
                webContents, true, "Did not enter PiP after manual request.");

        // Create a new tab in the background to switch to later.
        Tab originalTab = mPage.getTab();
        Tab newTab = createNewTabInBackground(originalTab);

        // Switch away from the tab.
        switchToTab(newTab);

        // The PiP window should still be open.
        AutoPictureInPictureTabHelperTestUtils.waitForPictureInPictureVideoState(
                webContents, true, "PiP should remain open after switching tabs.");
        AutoPictureInPictureTabHelperTestUtils.waitForAutoPictureInPictureState(
                webContents, false, "Should not trigger auto-PiP with existing PiP.");

        // Switch back to the original tab.
        switchToTab(originalTab);

        // The PiP window should still be open.
        AutoPictureInPictureTabHelperTestUtils.waitForPictureInPictureVideoState(
                webContents, true, "PiP should remain open after switching back.");
    }

    @Test
    @MediumTest
    public void testDoesNotAutopipWithoutHttps() throws TimeoutException {
        mActivityTestRule.getEmbeddedTestServerRule().setServerUsesHttps(false);

        WebContents webContents = loadUrlAndInitializeForTest(AUTO_PIP_VIDEO_PAGE);
        assertTrue(
                "Page should have registered for auto-pip.",
                AutoPictureInPictureTabHelperTestUtils.hasAutoPictureInPictureBeenRegistered(
                        webContents));

        // Create a new tab in the background to switch to later.
        Tab originalTab = mPage.getTab();
        Tab newTab = createNewTabInBackground(originalTab);

        fulfillVideoPlaybackConditions(webContents);

        // Switch away from the tab.
        switchToTab(newTab);

        // Should not enter auto-PiP on an insecure context.
        AutoPictureInPictureTabHelperTestUtils.waitForAutoPictureInPictureState(
                webContents, false, "Should not enter auto-PiP on an insecure context.");
    }

    @Test
    @MediumTest
    public void testDoesNotAutopipWhenPermissionIsBlocked() throws TimeoutException {
        WebContents webContents = loadUrlAndInitializeForTest(AUTO_PIP_VIDEO_PAGE);
        assertTrue(
                "Page should have registered for auto-pip.",
                AutoPictureInPictureTabHelperTestUtils.hasAutoPictureInPictureBeenRegistered(
                        webContents));

        // Block auto-pip via content setting.
        AutoPictureInPictureTabHelperTestUtils.setPermission(
                mPage.getTab().getProfile(),
                ContentSettingsType.AUTO_PICTURE_IN_PICTURE,
                mActivityTestRule.getTestServer().getURL(AUTO_PIP_VIDEO_PAGE),
                ContentSetting.BLOCK);

        // Create a new tab in the background to switch to later.
        Tab originalTab = mPage.getTab();
        Tab newTab = createNewTabInBackground(originalTab);

        fulfillVideoPlaybackConditions(webContents);

        // Switch away from the tab.
        switchToTab(newTab);

        // Should not enter auto-PiP when the permission is blocked.
        AutoPictureInPictureTabHelperTestUtils.waitForAutoPictureInPictureState(
                webContents, false, "Should not enter auto-PiP when permission is blocked.");
    }

    @Test
    @MediumTest
    public void testQuickDismissalIncrementsDismissCount() throws TimeoutException {
        WebContents webContents = loadUrlAndInitializeForTest(AUTO_PIP_VIDEO_PAGE);
        String url = mActivityTestRule.getTestServer().getURL(AUTO_PIP_VIDEO_PAGE);
        Tab originalTab = mPage.getTab();

        // Verify the initial dismiss count is 0.
        assertEquals(
                "Initial dismiss count should be 0.",
                0,
                AutoPictureInPictureTabHelperTestUtils.getDismissCountForTesting(webContents, url));

        PictureInPictureActivity pipActivity = enterAutoPip(webContents, originalTab);

        // Immediately close the PiP window to simulate a quick dismissal.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Configuration config = pipActivity.getResources().getConfiguration();
                    pipActivity.onPictureInPictureModeChanged(false, config);
                });

        // Wait for the PiP activity to be destroyed.
        CriteriaHelper.pollUiThread(pipActivity::isDestroyed);

        // Verify that the dismiss count is now 1.
        assertEquals(
                "Dismiss count should be 1 after a quick dismissal.",
                1,
                AutoPictureInPictureTabHelperTestUtils.getDismissCountForTesting(webContents, url));
    }

    @Test
    @MediumTest
    public void testSwitchingBackToTabDoesNotIncrementDismissCount() throws TimeoutException {
        WebContents webContents = loadUrlAndInitializeForTest(AUTO_PIP_VIDEO_PAGE);
        String url = mActivityTestRule.getTestServer().getURL(AUTO_PIP_VIDEO_PAGE);
        Tab originalTab = mPage.getTab();

        // Verify the initial dismiss count is 0.
        assertEquals(
                "Initial dismiss count should be 0.",
                0,
                AutoPictureInPictureTabHelperTestUtils.getDismissCountForTesting(webContents, url));

        PictureInPictureActivity pipActivity = enterAutoPip(webContents, originalTab);

        // Switch back to the original tab, which should auto-close the PiP window.
        switchToTab(originalTab);

        // Wait for the PiP activity to be destroyed.
        CriteriaHelper.pollUiThread(pipActivity::isDestroyed);

        // Verify that the dismiss count is still 0.
        assertEquals(
                "Dismiss count should not be incremented when manually switching back to the tab.",
                0,
                AutoPictureInPictureTabHelperTestUtils.getDismissCountForTesting(webContents, url));
    }

    @Test
    @MediumTest
    public void testClosingAfterTimerExpiresDoesNotIncrementDismissCount() throws TimeoutException {
        WebContents webContents = loadUrlAndInitializeForTest(AUTO_PIP_VIDEO_PAGE);
        String url = mActivityTestRule.getTestServer().getURL(AUTO_PIP_VIDEO_PAGE);
        Tab originalTab = mPage.getTab();

        // Verify the initial dismiss count is 0.
        assertEquals(
                "Initial dismiss count should be 0.",
                0,
                AutoPictureInPictureTabHelperTestUtils.getDismissCountForTesting(webContents, url));

        PictureInPictureActivity pipActivity = enterAutoPip(webContents, originalTab);

        // Manually expire the timer.
        ThreadUtils.runOnUiThreadBlocking(pipActivity::expireQuickDismissalTimerForTesting);

        // Close the PiP window.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Configuration config = pipActivity.getResources().getConfiguration();
                    pipActivity.onPictureInPictureModeChanged(false, config);
                });

        // Wait for the PiP activity to be destroyed.
        CriteriaHelper.pollUiThread(pipActivity::isDestroyed);

        // Verify that the dismiss count is still 0.
        assertEquals(
                "Dismiss count should not be incremented after the timer expires.",
                0,
                AutoPictureInPictureTabHelperTestUtils.getDismissCountForTesting(webContents, url));
    }

    @Test
    @MediumTest
    public void testHideButtonIncrementsDismissCount() throws TimeoutException {
        WebContents webContents = loadUrlAndInitializeForTest(AUTO_PIP_VIDEO_PAGE);
        String url = mActivityTestRule.getTestServer().getURL(AUTO_PIP_VIDEO_PAGE);
        Tab originalTab = mPage.getTab();

        // Verify the initial dismiss count is 0.
        assertEquals(
                "Initial dismiss count should be 0.",
                0,
                AutoPictureInPictureTabHelperTestUtils.getDismissCountForTesting(webContents, url));

        PictureInPictureActivity pipActivity = enterAutoPip(webContents, originalTab);

        // Simulate clicking the hide button.
        ThreadUtils.runOnUiThreadBlocking(pipActivity::triggerHideActionForTesting);

        // Wait for the PiP activity to be destroyed.
        CriteriaHelper.pollUiThread(pipActivity::isDestroyed);

        // Verify that the dismiss count is now 1.
        assertEquals(
                "Dismiss count should be 1 after hide button dismissal.",
                1,
                AutoPictureInPictureTabHelperTestUtils.getDismissCountForTesting(webContents, url));
    }

    /**
     * Fulfills the video playback conditions required for auto-PiP to trigger.
     *
     * @param webContents The WebContents on which to fulfill the conditions.
     * @throws TimeoutException if the media does not start playing within the timeout period.
     */
    private void fulfillVideoPlaybackConditions(WebContents webContents) throws TimeoutException {
        // Override and mock high media engagement value.
        AutoPictureInPictureTabHelperTestUtils.setHasHighMediaEngagement(webContents, true);

        // Start playing the video.
        DOMUtils.playMedia(webContents, VIDEO_ID);
        DOMUtils.waitForMediaPlay(webContents, VIDEO_ID);
    }

    /**
     * Loads the given page URL and initializes {@link AutoPictureInPictureTabHelper} for testing.
     *
     * @param pageUrl The URL of the page to load.
     * @return The {@link WebContents} of the loaded page.
     */
    private WebContents loadUrlAndInitializeForTest(String pageUrl) {
        String url = mActivityTestRule.getTestServer().getURL(pageUrl);
        mPage = mPage.loadWebPageProgrammatically(url);

        WebContents webContents = mActivityTestRule.getWebContents();
        AutoPictureInPictureTabHelperTestUtils.initializeForTesting(webContents);
        return webContents;
    }

    /**
     * Creates a new tab in the background.
     *
     * @param parentTab The parent tab for the new tab.
     * @return The newly created {@link Tab}.
     */
    private Tab createNewTabInBackground(Tab parentTab) {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    return mActivity
                            .getCurrentTabCreator()
                            .createNewTab(
                                    new LoadUrlParams("about:blank"),
                                    TabLaunchType.FROM_LONGPRESS_BACKGROUND,
                                    parentTab);
                });
    }

    /**
     * Switches to the given tab and waits for the tab switch to complete.
     *
     * @param tab The tab to switch to.
     */
    private void switchToTab(Tab tab) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabModelUtils.selectTabById(
                            mActivity.getTabModelSelector(),
                            tab.getId(),
                            TabSelectionType.FROM_USER);
                });
        CriteriaHelper.pollUiThread(
                () -> {
                    Tab currentTab = mActivity.getTabModelSelector().getCurrentTab();
                    return currentTab != null && currentTab.getId() == tab.getId();
                },
                "Tab switch did not complete.");
    }

    /**
     * Triggers auto-PiP and waits for the {@link PictureInPictureActivity} to be created.
     *
     * @param webContents The WebContents on which to trigger auto-PiP.
     * @param originalTab The tab to switch away from.
     * @return The created {@link PictureInPictureActivity}.
     * @throws TimeoutException if the conditions to trigger auto-PiP are not met.
     */
    private PictureInPictureActivity enterAutoPip(WebContents webContents, Tab originalTab)
            throws TimeoutException {
        Tab newTab = createNewTabInBackground(originalTab);
        fulfillVideoPlaybackConditions(webContents);
        switchToTab(newTab);
        AutoPictureInPictureTabHelperTestUtils.waitForAutoPictureInPictureState(
                webContents, true, "Did not enter auto-PiP after tab hidden.");

        PictureInPictureActivity pipActivity = getPictureInPictureActivity();
        assertNotNull("PictureInPictureActivity not found.", pipActivity);
        CriteriaHelper.pollUiThread(pipActivity::isInPictureInPictureMode);
        return pipActivity;
    }

    /**
     * Waits for and returns the currently active {@link PictureInPictureActivity}.
     *
     * @return The current {@link PictureInPictureActivity} instance, or null if not found.
     */
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
}
