// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.net.Uri;
import android.support.test.InstrumentationRegistry;
import android.view.KeyEvent;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.media.MediaViewerUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.media.MediaSwitches;

import java.util.concurrent.TimeoutException;

/**
 * Integration tests for the feature that auto locks the orientation when a video goes fullscreen.
 * See also content layer org.chromium.content.browser.VideoFullscreenOrientationLockTest
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        MediaSwitches.AUTOPLAY_NO_GESTURE_REQUIRED_POLICY})
public class VideoFullscreenOrientationLockChromeTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private static final String TEST_URL = "content/test/data/media/video-player.html";
    private static final String VIDEO_URL = "content/test/data/media/bear.webm";
    private static final String VIDEO_ID = "video";

    private WebContents getWebContents() {
        return mActivityTestRule.getActivity().getCurrentWebContents();
    }

    private void waitForContentsFullscreenState(boolean fullscreenValue) {
        CriteriaHelper.pollInstrumentationThread(
                Criteria.equals(fullscreenValue, () -> DOMUtils.isFullscreen(getWebContents())));
    }

    private boolean isScreenOrientationLocked() {
        return mActivityTestRule.getActivity().getRequestedOrientation()
                != ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED;
    }

    private boolean isScreenOrientationLandscape() throws TimeoutException {
        StringBuilder sb = new StringBuilder();
        sb.append("(function() {");
        sb.append("  return  screen.orientation.type.startsWith('landscape');");
        sb.append("})();");

        return JavaScriptUtils.executeJavaScriptAndWaitForResult(getWebContents(), sb.toString())
                .equals("true");
    }

    private void waitUntilLockedToLandscape() {
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                try {
                    return isScreenOrientationLocked() && isScreenOrientationLandscape();
                } catch (TimeoutException e) {
                    return false;
                }
            }
        });
    }

    private void waitUntilUnlocked() {
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return !isScreenOrientationLocked();
            }
        });
    }

    @Before
    public void setUp() {
        mActivityTestRule.startMainActivityWithURL(UrlUtils.getIsolatedTestFileUrl(TEST_URL));
    }

    @Test
    // Test is disabled due to flakiness - crbug.com/888161
    // @MediumTest
    // @RetryOnFailure // The final waitForContentsFullscreenState(false) is flaky -
    // crbug.com/711005.
    @DisabledTest
    public void testUnlockWithDownloadViewerActivity() throws Exception {
        if (mActivityTestRule.getActivity().isTablet()) {
            return;
        }

        // Start playback to guarantee it's properly loaded.
        Assert.assertTrue(DOMUtils.isMediaPaused(getWebContents(), VIDEO_ID));
        DOMUtils.playMedia(getWebContents(), VIDEO_ID);
        DOMUtils.waitForMediaPlay(getWebContents(), VIDEO_ID);

        // Trigger requestFullscreen() via a click on a button.
        Assert.assertTrue(DOMUtils.clickNode(getWebContents(), "fullscreen"));
        waitForContentsFullscreenState(true);

        // Should be locked to landscape now, `waitUntilLockedToLandscape` will throw otherwise.
        waitUntilLockedToLandscape();

        // Orientation lock should be disabled when download viewer activity is started.
        Uri fileUri = Uri.parse(UrlUtils.getIsolatedTestFileUrl(VIDEO_URL));
        String mimeType = "video/mp4";
        Intent intent = MediaViewerUtils.getMediaViewerIntent(
                fileUri, fileUri, mimeType, true /* allowExternalAppHandlers */);
        IntentHandler.startActivityForTrustedIntent(intent);
        waitUntilUnlocked();

        // Sometimes the web page doesn't transition out of fullscreen until we exit the download
        // activity and return to the web page.
        InstrumentationRegistry.getInstrumentation().sendKeyDownUpSync(KeyEvent.KEYCODE_BACK);
        waitForContentsFullscreenState(false);
    }
}
