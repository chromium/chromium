// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.player.mini;

import static org.chromium.chrome.modules.readaloud.PlaybackListener.State.BUFFERING;
import static org.chromium.chrome.modules.readaloud.PlaybackListener.State.ERROR;
import static org.chromium.chrome.modules.readaloud.PlaybackListener.State.PAUSED;
import static org.chromium.chrome.modules.readaloud.PlaybackListener.State.PLAYING;
import static org.chromium.chrome.modules.readaloud.PlaybackListener.State.STOPPED;
import static org.chromium.chrome.modules.readaloud.PlaybackListener.State.UNKNOWN;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.TextView;

import org.chromium.chrome.browser.readaloud.player.InteractionHandler;
import org.chromium.chrome.browser.readaloud.player.R;
import org.chromium.chrome.modules.readaloud.PlaybackListener;

/** Convenience class for manipulating mini player UI layout. */
public class MiniPlayerLayout extends LinearLayout {
    private TextView mTitle;
    private TextView mPublisher;
    private ProgressBar mProgressBar;
    private ImageView mPlayPauseView;

    // Layouts related to different playback states.
    private LinearLayout mNormalLayout;
    private LinearLayout mBufferingLayout;
    private LinearLayout mErrorLayout;

    private @PlaybackListener.State int mLastPlaybackState;

    /** Constructor for inflating from XML. */
    public MiniPlayerLayout(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mTitle = (TextView) findViewById(R.id.title);
        mPublisher = (TextView) findViewById(R.id.publisher);
        mProgressBar = (ProgressBar) findViewById(R.id.progress_bar);
        mPlayPauseView = (ImageView) findViewById(R.id.play_button);

        mNormalLayout = (LinearLayout) findViewById(R.id.normal_layout);
        mBufferingLayout = (LinearLayout) findViewById(R.id.buffering_layout);
        mErrorLayout = (LinearLayout) findViewById(R.id.error_layout);

        mLastPlaybackState = PlaybackListener.State.UNKNOWN;
    }

    void setTitle(String title) {
        mTitle.setText(title);
    }

    void setPublisher(String publisher) {
        mPublisher.setText(publisher);
    }

    /**
     * Set progress bar progress.
     * @param progress Fraction of playback completed in range [0, 1]
     */
    void setProgress(float progress) {
        mProgressBar.setProgress((int) (progress * mProgressBar.getMax()), true);
    }

    void setInteractionHandler(InteractionHandler handler) {
        setOnClickListener(R.id.close_button, handler::onCloseClick);
        setOnClickListener(R.id.mini_player_background, handler::onMiniPlayerExpandClick);
        setOnClickListener(R.id.play_button, handler::onPlayPauseClick);
    }

    void onPlaybackStateChanged(@PlaybackListener.State int state) {
        switch (state) {
            // UNKNOWN is currently the "reset" state and can be treated same as buffering.
            case BUFFERING:
            case UNKNOWN:
                showOnly(mBufferingLayout);
                mProgressBar.setVisibility(View.GONE);
                break;

            case ERROR:
                showOnly(mErrorLayout);
                mProgressBar.setVisibility(View.GONE);
                break;

            case PLAYING:
                if (mLastPlaybackState != PLAYING && mLastPlaybackState != PAUSED) {
                    showOnly(mNormalLayout);
                    mProgressBar.setVisibility(View.VISIBLE);
                }

                mPlayPauseView.setImageResource(R.drawable.mini_pause_button);
                mPlayPauseView.setContentDescription(
                        getResources().getString(R.string.readaloud_pause));
                break;

            case PAUSED:
                if (mLastPlaybackState != PLAYING && mLastPlaybackState != PAUSED) {
                    showOnly(mNormalLayout);
                    mProgressBar.setVisibility(View.VISIBLE);
                }

                mPlayPauseView.setImageResource(R.drawable.mini_play_button);
                mPlayPauseView.setContentDescription(
                        getResources().getString(R.string.readaloud_play));
                break;

            // TODO(b/301657446): handle this case
            case STOPPED:
            default:
                break;
        }
        mLastPlaybackState = state;
    }

    // Show `layout` and hide the other two.
    private void showOnly(LinearLayout layout) {
        setVisibleIfMatch(mNormalLayout, layout);
        setVisibleIfMatch(mBufferingLayout, layout);
        setVisibleIfMatch(mErrorLayout, layout);
    }

    private static void setVisibleIfMatch(LinearLayout a, LinearLayout b) {
        a.setVisibility(a == b ? View.VISIBLE : View.GONE);
    }

    private void setOnClickListener(int id, Runnable handler) {
        findViewById(id).setOnClickListener((v) -> { handler.run(); });
    }
}
