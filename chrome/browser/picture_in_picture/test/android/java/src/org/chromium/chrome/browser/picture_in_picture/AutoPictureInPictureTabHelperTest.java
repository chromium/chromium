// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.picture_in_picture;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;
import static org.chromium.ui.test.util.DeviceRestriction.RESTRICTION_TYPE_NON_AUTO;

import android.app.Activity;
import android.content.res.Configuration;
import android.os.Build.VERSION_CODES;

import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
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
import org.chromium.base.test.util.Criteria;
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
import org.chromium.media.MediaFeatures;
import org.chromium.media.MediaSwitches;

import java.util.List;
import java.util.concurrent.TimeoutException;

/** Test suite for {@link AutoPictureInPictureTabHelper}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
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
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabModelUtils.selectTabById(
                            mActivity.getTabModelSelector(),
                            newTab.getId(),
                            TabSelectionType.FROM_USER);
                });

        // Wait for the tab switch to complete.
        CriteriaHelper.pollUiThread(
                () -> {
                    Tab currentTab = mActivity.getTabModelSelector().getCurrentTab();
                    return currentTab != null && currentTab.getId() == newTab.getId();
                },
                "Tab switch did not complete.");

        AutoPictureInPictureTabHelperTestUtils.waitForAutoPictureInPictureState(
                webContents, true, "Did not enter auto-PiP after tab hidden.");

        // Return to the tab. This should exit auto-PiP.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabModelUtils.selectTabById(
                            mActivity.getTabModelSelector(),
                            originalTab.getId(),
                            TabSelectionType.FROM_USER);
                });
        AutoPictureInPictureTabHelperTestUtils.waitForAutoPictureInPictureState(
                webContents, false, "Did not exit auto-PiP after tab shown.");
    }

    @Test
    @MediumTest
    public void testBackToTabFromAutoPip() throws TimeoutException {
        WebContents webContents = loadUrlAndInitializeForTest(AUTO_PIP_VIDEO_PAGE);
        assertTrue(
                "Page should have registered for auto-pip.",
                AutoPictureInPictureTabHelperTestUtils.hasAutoPictureInPictureBeenRegistered(
                        webContents));

        // Create a new tab in the background to switch to later.
        Tab originalTab = mPage.getTab();
        Tab newTab = createNewTabInBackground(originalTab);

        fulfillVideoPlaybackConditions(webContents);

        // Switch away from the tab. This should trigger auto-PiP.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabModelUtils.selectTabById(
                            mActivity.getTabModelSelector(),
                            newTab.getId(),
                            TabSelectionType.FROM_USER);
                });

        // Wait for the tab switch to complete.
        CriteriaHelper.pollUiThread(
                () -> {
                    Tab currentTab = mActivity.getTabModelSelector().getCurrentTab();
                    return currentTab != null && currentTab.getId() == newTab.getId();
                },
                "Tab switch did not complete.");

        AutoPictureInPictureTabHelperTestUtils.waitForAutoPictureInPictureState(
                webContents, true, "Did not enter auto-PiP after tab hidden.");

        // Get the PiP activity.
        PictureInPictureActivity pipActivity = getPictureInPictureActivity();
        assertNotNull("PictureInPictureActivity not found.", pipActivity);

        // Wait for the activity to actually enter PiP mode.
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(pipActivity.isInPictureInPictureMode(), Matchers.is(true));
                });

        // Simulate clicking the "back to tab" button.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Configuration config = pipActivity.getResources().getConfiguration();
                    pipActivity.onPictureInPictureModeChanged(false, config);
                });

        // Wait for the PictureInPictureActivity to be destroyed.
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            pipActivity == null || pipActivity.isDestroyed(), Matchers.is(true));
                });

        // Now that the activity is gone, verify the C++ state.
        AutoPictureInPictureTabHelperTestUtils.waitForAutoPictureInPictureState(
                webContents, false, "Did not exit auto-PiP after back-to-tab.");
    }

    // TODO(crbug.com/421608904): add a test case for camera/mic based video auto-PiP.

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
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabModelUtils.selectTabById(
                            mActivity.getTabModelSelector(),
                            newTab.getId(),
                            TabSelectionType.FROM_USER);
                });

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
        AutoPictureInPictureTabHelperTestUtils.setHasAudioFocusForTesting(webContents, true);

        // Switch away from the tab.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabModelUtils.selectTabById(
                            mActivity.getTabModelSelector(),
                            newTab.getId(),
                            TabSelectionType.FROM_USER);
                });

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
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabModelUtils.selectTabById(
                            mActivity.getTabModelSelector(),
                            newTab.getId(),
                            TabSelectionType.FROM_USER);
                });

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
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabModelUtils.selectTabById(
                            mActivity.getTabModelSelector(),
                            newTab.getId(),
                            TabSelectionType.FROM_USER);
                });

        // The PiP window should still be open.
        AutoPictureInPictureTabHelperTestUtils.waitForPictureInPictureVideoState(
                webContents, true, "PiP should remain open after switching tabs.");
        AutoPictureInPictureTabHelperTestUtils.waitForAutoPictureInPictureState(
                webContents, false, "Should not trigger auto-PiP with existing PiP.");

        // Switch back to the original tab.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabModelUtils.selectTabById(
                            mActivity.getTabModelSelector(),
                            originalTab.getId(),
                            TabSelectionType.FROM_USER);
                });

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
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabModelUtils.selectTabById(
                            mActivity.getTabModelSelector(),
                            newTab.getId(),
                            TabSelectionType.FROM_USER);
                });

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
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabModelUtils.selectTabById(
                            mActivity.getTabModelSelector(),
                            newTab.getId(),
                            TabSelectionType.FROM_USER);
                });

        // Should not enter auto-PiP when the permission is blocked.
        AutoPictureInPictureTabHelperTestUtils.waitForAutoPictureInPictureState(
                webContents, false, "Should not enter auto-PiP when permission is blocked.");
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

        // Manually set audio focus for testing. It's needed because programmatically
        // starting a video on Android doesn't gain audio focus.
        AutoPictureInPictureTabHelperTestUtils.setHasAudioFocusForTesting(webContents, true);
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
