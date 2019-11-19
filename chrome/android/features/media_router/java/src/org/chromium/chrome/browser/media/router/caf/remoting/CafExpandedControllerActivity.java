// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.router.caf.remoting;

import android.graphics.Color;
import android.os.Bundle;
import android.os.Handler;
import android.support.v4.app.FragmentActivity;
import android.support.v4.media.session.PlaybackStateCompat;
import android.support.v7.app.MediaRouteButton;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.view.WindowManager;
import android.widget.TextView;

import org.chromium.chrome.browser.media.router.caf.BaseSessionController;
import org.chromium.chrome.browser.metrics.MediaNotificationUma;
import org.chromium.chrome.media.router.R;
import org.chromium.third_party.android.media.MediaController;

/**
 * The activity that's opened by clicking the video flinging (casting) notification.
 */
public class CafExpandedControllerActivity
        extends FragmentActivity implements BaseSessionController.Callback {
    private static final int PROGRESS_UPDATE_PERIOD_IN_MS = 1000;

    private Handler mHandler;
    // We don't use the standard android.media.MediaController, but a custom one.
    // See the class itself for details.
    private MediaController mMediaController;
    private RemotingSessionController mSessionController;
    private MediaRouteButton mMediaRouteButton;
    private TextView mTitleView;
    private Runnable mUpdateProgressRunnable;

    /**
     * Handle actions from on-screen media controls.
     */
    private MediaController.Delegate mControllerDelegate = new MediaController.Delegate() {
        @Override
        public void play() {
            if (!mSessionController.isConnected()) return;

            mSessionController.getSession().getRemoteMediaClient().play();
        }

        @Override
        public void pause() {
            if (!mSessionController.isConnected()) return;

            mSessionController.getSession().getRemoteMediaClient().pause();
        }

        @Override
        public long getDuration() {
            if (!mSessionController.isConnected()) return 0;
            return mSessionController.getFlingingController().getDuration();
        }

        @Override
        public long getPosition() {
            if (!mSessionController.isConnected()) return 0;
            return mSessionController.getFlingingController().getApproximateCurrentTime();
        }

        @Override
        public void seekTo(long pos) {
            if (!mSessionController.isConnected()) return;

            mSessionController.safelySeek(pos);
        }

        @Override
        public boolean isPlaying() {
            if (!mSessionController.isConnected()) return false;

            return mSessionController.getSession().getRemoteMediaClient().isPlaying();
        }

        @Override
        public long getActionFlags() {
            long flags =
                    PlaybackStateCompat.ACTION_REWIND | PlaybackStateCompat.ACTION_FAST_FORWARD;
            if (mSessionController.isConnected()
                    && mSessionController.getSession().getRemoteMediaClient().isPlaying()) {
                flags |= PlaybackStateCompat.ACTION_PAUSE;
            } else {
                flags |= PlaybackStateCompat.ACTION_PLAY;
            }
            return flags;
        }
    };

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        mSessionController = RemotingSessionController.getInstance();

        MediaNotificationUma.recordClickSource(getIntent());

        if (mSessionController == null || !mSessionController.isConnected()) {
            finish();
            return;
        }

        mSessionController.addCallback(this);

        // Make the activity full screen.
        requestWindowFeature(Window.FEATURE_NO_TITLE);
        getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN,
                WindowManager.LayoutParams.FLAG_FULLSCREEN);

        // requestWindowFeature must be called before adding content.
        setContentView(R.layout.expanded_cast_controller);

        ViewGroup rootView = (ViewGroup) findViewById(android.R.id.content);
        rootView.setBackgroundColor(Color.BLACK);

        // Create and initialize the media control UI.
        mMediaController = (MediaController) findViewById(R.id.cast_media_controller);
        mMediaController.setDelegate(mControllerDelegate);

        View castButtonView = getLayoutInflater().inflate(
                R.layout.caf_controller_media_route_button, rootView, false);
        if (castButtonView instanceof MediaRouteButton) {
            mMediaRouteButton = (MediaRouteButton) castButtonView;
            rootView.addView(mMediaRouteButton);
            mMediaRouteButton.bringToFront();
            mMediaRouteButton.setRouteSelector(mSessionController.getSource().buildRouteSelector());
        }

        mTitleView = (TextView) findViewById(R.id.cast_screen_title);

        mHandler = new Handler();
        mUpdateProgressRunnable = this::updateProgress;

        updateUi();
    }

    @Override
    protected void onResume() {
        super.onResume();

        if (mSessionController == null || !mSessionController.isConnected()) {
            finish();
            return;
        }
    }

    @Override
    protected void onDestroy() {
        mSessionController.removeCallback(this);
        super.onDestroy();
    }

    @Override
    public void onSessionStarted() {}

    @Override
    public void onSessionEnded() {
        finish();
    }

    @Override
    public void onStatusUpdated() {
        updateUi();
    }

    @Override
    public void onMetadataUpdated() {
        updateUi();
    }

    private void updateUi() {
        if (!mSessionController.isConnected()) return;

        String deviceName = mSessionController.getSession().getCastDevice().getFriendlyName();
        String titleText = "";
        if (deviceName != null) {
            titleText = getResources().getString(R.string.cast_casting_video, deviceName);
        }
        mTitleView.setText(titleText);

        mMediaController.refresh();
        mMediaController.updateProgress();

        cancelProgressUpdateTask();
        if (mSessionController.getSession().getRemoteMediaClient().isPlaying()) {
            scheduleProgressUpdateTask();
        }
    }

    private void scheduleProgressUpdateTask() {
        mHandler.postDelayed(mUpdateProgressRunnable, PROGRESS_UPDATE_PERIOD_IN_MS);
    }

    private void cancelProgressUpdateTask() {
        mHandler.removeCallbacks(mUpdateProgressRunnable);
    }

    private void updateProgress() {
        mMediaController.updateProgress();
        scheduleProgressUpdateTask();
    }
}
