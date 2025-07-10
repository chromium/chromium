// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.picture_in_picture;

import static org.junit.Assert.assertTrue;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;
import static org.chromium.ui.test.util.DeviceRestriction.RESTRICTION_TYPE_NON_AUTO;

import android.os.Build.VERSION_CODES;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.blink_public.common.BlinkFeatures;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.page.PageStation;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.media.MediaFeatures;
import org.chromium.media.MediaSwitches;

import java.util.concurrent.TimeoutException;

/** Test suite for {@link AutoPictureInPictureTabHelper}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    MediaSwitches.AUTOPLAY_NO_GESTURE_REQUIRED_POLICY
})
@EnableFeatures({
    BlinkFeatures.MEDIA_SESSION_ENTER_PICTURE_IN_PICTURE,
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
    private static final String AUTO_PIP_VIDEO_PAGE =
            "/chrome/test/data/media/picture-in-picture/autopip-video.html";

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
    public void testAutoPipWorksAndStops() throws TimeoutException {
        WebContents webContents = loadUrlAndInitializeForTest(AUTO_PIP_VIDEO_PAGE);
        // Verify if the loaded page registers auto pip.
        assertTrue(
                AutoPictureInPictureTabHelperTestUtils.hasAutoPictureInPictureBeenRegistered(
                        webContents));

        // Create a new tab to switch to later, then switch back to set up the test state.
        Tab originalTab = mPage.getTab();
        PageStation page = mPage.openNewTabFast();
        Tab newTab = page.getTab();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabModelUtils.selectTabById(
                            mActivity.getTabModelSelector(),
                            originalTab.getId(),
                            TabSelectionType.FROM_USER);
                });

        // Override and mock high media engagement value.
        AutoPictureInPictureTabHelperTestUtils.setHasHighMediaEngagement(webContents, true);

        // Starting playing.
        DOMUtils.playMedia(webContents, VIDEO_ID);
        DOMUtils.waitForMediaPlay(webContents, VIDEO_ID);

        // Manually set audio focus for testing. It's needed because programmatically
        // starting a video on Android doesn't gain audio focus.
        AutoPictureInPictureTabHelperTestUtils.setHasAudioFocusForTesting(webContents, true);

        // Switch away from the tab. This should trigger auto-PiP.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabModelUtils.selectTabById(
                            mActivity.getTabModelSelector(),
                            newTab.getId(),
                            TabSelectionType.FROM_USER);
                });
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

    private WebContents loadUrlAndInitializeForTest(String pageUrl) {
        String url = mActivityTestRule.getTestServer().getURL(pageUrl);
        mPage = mPage.loadWebPageProgrammatically(url);

        WebContents webContents = mActivity.getCurrentWebContents();
        AutoPictureInPictureTabHelperTestUtils.initializeForTesting(webContents);
        return webContents;
    }
}
