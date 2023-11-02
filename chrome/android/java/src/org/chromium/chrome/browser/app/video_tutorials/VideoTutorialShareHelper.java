// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.video_tutorials;

import android.text.TextUtils;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.init.BrowserParts;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.init.EmptyBrowserParts;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.video_tutorials.FeatureType;
import org.chromium.chrome.browser.video_tutorials.Tutorial;
import org.chromium.chrome.browser.video_tutorials.VideoTutorialService;
import org.chromium.chrome.browser.video_tutorials.VideoTutorialServiceFactory;
import org.chromium.chrome.browser.video_tutorials.metrics.VideoTutorialMetrics;
import org.chromium.chrome.browser.video_tutorials.metrics.VideoTutorialMetrics.UserAction;

import java.util.HashSet;
import java.util.Set;

/**
 * Helper class to handle share intents for video tutorials.
 */
public class VideoTutorialShareHelper {
    /**
     * Handles the URL, if it is a video tutorial share URL, otherwise returns false.
     * @param url The URL being checked.
     * @return True, if the URL was recognized as a video tutorial URL and handled, false otherwise.
     */
    public static boolean handleVideoTutorialURL(String url) {
        Set<String> videoTutorialUrls = SharedPreferencesManager.getInstance().readStringSet(
                ChromePreferenceKeys.VIDEO_TUTORIALS_SHARE_URL_SET, new HashSet<>());
        if (!videoTutorialUrls.contains(url)) return false;
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.VIDEO_TUTORIALS)) return false;

        launchVideoPlayer(url);
        return true;
    }

    /**
     * Writes the video tutorial share URLs to shared preferences. This is later used to identify
     * whether a URL is a video tutorial.
     */
    public static void saveUrlsToSharedPrefs() {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.VIDEO_TUTORIALS)) {
            SharedPreferencesManager.getInstance().removeKey(
                    ChromePreferenceKeys.VIDEO_TUTORIALS_SHARE_URL_SET);
            return;
        }

        VideoTutorialService videoTutorialService =
                VideoTutorialServiceFactory.getForProfile(Profile.getLastUsedRegularProfile());
        videoTutorialService.getTutorials(tutorials -> {
            Set<String> shareUrlSet = new HashSet<>();
            for (Tutorial tutorial : tutorials) {
                if (TextUtils.isEmpty(tutorial.shareUrl)) continue;
                shareUrlSet.add(tutorial.shareUrl);
            }
            SharedPreferencesManager.getInstance().writeStringSet(
                    ChromePreferenceKeys.VIDEO_TUTORIALS_SHARE_URL_SET, shareUrlSet);
        });
    }

    private static void launchVideoPlayer(String shareUrl) {
        assert !TextUtils.isEmpty(shareUrl);

        BrowserParts parts = new EmptyBrowserParts() {
            @Override
            public void finishNativeInitialization() {
                VideoTutorialService videoTutorialService =
                        VideoTutorialServiceFactory.getForProfile(
                                Profile.getLastUsedRegularProfile());
                videoTutorialService.getTutorials(tutorials -> {
                    for (Tutorial tutorial : tutorials) {
                        if (TextUtils.equals(tutorial.shareUrl, shareUrl)) {
                            VideoTutorialMetrics.recordUserAction(
                                    tutorial.featureType, UserAction.OPEN_SHARED_VIDEO);
                            VideoPlayerActivity.playVideoTutorial(
                                    ContextUtils.getApplicationContext(), tutorial.featureType);
                            return;
                        }
                    }
                    VideoTutorialMetrics.recordUserAction(
                            FeatureType.INVALID, UserAction.INVALID_SHARE_URL);
                });
            }
        };
        ChromeBrowserInitializer.getInstance().handlePreNativeStartupAndLoadLibraries(parts);
        ChromeBrowserInitializer.getInstance().handlePostNativeStartup(true, parts);
    }
}
