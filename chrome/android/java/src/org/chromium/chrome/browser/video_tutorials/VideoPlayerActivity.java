// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.video_tutorials;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.util.Pair;
import android.view.WindowManager;

import org.chromium.base.IntentUtils;
import org.chromium.chrome.browser.WebContentsFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.video_tutorials.player.VideoPlayerCoordinator;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.components.version_info.VersionConstants;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.WindowAndroid;

/**
 * Java interface for interacting with the native video tutorial service. Responsible for
 * initializing and fetching data fo be shown on the UI.
 */
public class VideoPlayerActivity extends Activity {
    public static final String EXTRA_VIDEO_TUTORIAL = "extra_video_tutorial";

    private WindowAndroid mWindowAndroid;
    private VideoPlayerCoordinator mCoordinator;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_HARDWARE_ACCELERATED);

        VideoTutorialService videoTutorialService =
                VideoTutorialServiceFactory.getForProfile(Profile.getLastUsedRegularProfile());
        mWindowAndroid = new ActivityWindowAndroid(this);
        mCoordinator = VideoTutorialServiceFactory.createVideoPlayerCoordinator(
                this, videoTutorialService, this::createWebContents, this::finish);
        setContentView(mCoordinator.getView());

        int featureType = IntentUtils.safeGetIntExtra(getIntent(), EXTRA_VIDEO_TUTORIAL, 0);
        videoTutorialService.getTutorial(
                featureType, tutorial -> { mCoordinator.playVideoTutorial(tutorial); });
    }

    private Pair<WebContents, ContentView> createWebContents() {
        Profile profile = Profile.getLastUsedRegularProfile();
        WebContents webContents = WebContentsFactory.createWebContents(profile, false);
        ContentView contentView =
                ContentView.createContentView(this, null /* eventOffsetHandler */, webContents);
        webContents.initialize(VersionConstants.PRODUCT_VERSION,
                ViewAndroidDelegate.createBasicDelegate(contentView), contentView, mWindowAndroid,
                WebContents.createDefaultInternalsHolder());
        return Pair.create(webContents, contentView);
    }

    @Override
    public void onBackPressed() {
        if (mCoordinator.onBackPressed()) return;
        super.onBackPressed();
    }

    @Override
    protected void onDestroy() {
        mCoordinator.destroy();
        super.onDestroy();
    }

    /** Called to launch this activity to play a given video tutorial. */
    static void playVideoTutorial(Context context, Tutorial tutorial) {
        Intent intent = new Intent();
        intent.setClass(context, VideoPlayerActivity.class);
        intent.putExtra(VideoPlayerActivity.EXTRA_VIDEO_TUTORIAL, tutorial.featureType);
        context.startActivity(intent);
    }
}
