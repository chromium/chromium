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

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.ObjectAnimator;
import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Rect;
import android.util.AttributeSet;
import android.view.TouchDelegate;
import android.view.View;
import android.view.animation.Interpolator;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.TextView;

import androidx.annotation.ColorInt;

import com.google.android.material.animation.ChildrenAlphaProperty;

import org.chromium.chrome.browser.readaloud.player.InteractionHandler;
import org.chromium.chrome.browser.readaloud.player.R;
import org.chromium.chrome.browser.readaloud.player.VisibilityState;
import org.chromium.chrome.modules.readaloud.PlaybackListener;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.ui.interpolators.Interpolators;
import org.chromium.ui.util.ColorUtils;

/** Convenience class for manipulating mini player UI layout. */
public class MiniPlayerLayout extends LinearLayout {
    private static final long FADE_DURATION_MS = 300L;
    private static final Interpolator FADE_INTERPOLATOR =
            Interpolators.FAST_OUT_SLOW_IN_INTERPOLATOR;

    private TextView mTitle;
    private TextView mPublisher;
    private ProgressBar mProgressBar;
    private ImageView mPlayPauseView;
    private FrameLayout mBackdrop;
    private View mContents;

    // Layouts related to different playback states.
    private LinearLayout mNormalLayout;
    private LinearLayout mBufferingLayout;
    private LinearLayout mErrorLayout;

    private @PlaybackListener.State int mLastPlaybackState;
    private boolean mEnableAnimations;
    private InteractionHandler mInteractionHandler;
    private ObjectAnimator mAnimator;
    private @VisibilityState int mFinalVisibility;
    private MiniPlayerMediator mMediator;
    private float mFinalOpacity;
    private @ColorInt int mBackgroundColorArgb;

    /** Constructor for inflating from XML. */
    public MiniPlayerLayout(Context context, AttributeSet attrs) {
        super(context, attrs);
        mFinalVisibility = VisibilityState.GONE;
    }

    void destroy() {
        destroyAnimator();
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        // Cache important views.
        mTitle = (TextView) findViewById(R.id.title);
        mPublisher = (TextView) findViewById(R.id.publisher);
        mProgressBar = (ProgressBar) findViewById(R.id.progress_bar);
        mPlayPauseView = (ImageView) findViewById(R.id.play_button);

        mBackdrop = (FrameLayout) findViewById(R.id.backdrop);
        mContents = findViewById(R.id.mini_player_container);
        mNormalLayout = (LinearLayout) findViewById(R.id.normal_layout);
        mBufferingLayout = (LinearLayout) findViewById(R.id.buffering_layout);
        mErrorLayout = (LinearLayout) findViewById(R.id.error_layout);

        // Set dynamic colors.
        Context context = getContext();
        mBackgroundColorArgb = SemanticColorUtils.getDefaultBgColor(context);
        @ColorInt int progressBarColor = SemanticColorUtils.getDefaultIconColor(context);
        if (ColorUtils.inNightMode(context)) {
            // This color should change to "Surface Container High" in the next Material update.
            mBackgroundColorArgb =
                    ChromeColors.getSurfaceColor(context, R.dimen.default_elevation_4);
            progressBarColor = context.getColor(R.color.baseline_primary_80);
        }
        findViewById(R.id.backdrop).setBackgroundColor(mBackgroundColorArgb);
        if (mMediator != null) {
            mMediator.onBackgroundColorUpdated(mBackgroundColorArgb);
        }

        mProgressBar.setProgressTintList(ColorStateList.valueOf(progressBarColor));
        mLastPlaybackState = PlaybackListener.State.UNKNOWN;
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        super.onLayout(changed, left, top, right, bottom);

        int height = mBackdrop.getHeight();
        if (height == 0) {
            return;
        }

        if (mMediator != null) {
            mMediator.onBackgroundColorUpdated(mBackgroundColorArgb);
            mMediator.onHeightKnown(height);
        }

        // Make the close button touch target bigger.
        View closeButton = findViewById(R.id.close_button);
        Rect target = new Rect();
        closeButton.getHitRect(target);
        int halfWidth = target.width() / 2;
        int halfHeight = target.height() / 2;
        target.left -= halfWidth;
        target.top -= halfHeight;
        target.right += halfWidth;
        target.bottom += halfHeight;
        ((View) closeButton.getParent()).setTouchDelegate(new TouchDelegate(target, closeButton));
    }

    void changeOpacity(float startValue, float endValue) {
        assert (mMediator != null)
                : "Can't call changeOpacity() before setMediator() which should happen during"
                        + " mediator init.";
        if (endValue == mFinalOpacity) {
            return;
        }
        mFinalOpacity = endValue;

        Runnable onFinished =
                endValue == 1f ? mMediator::onFullOpacityReached : mMediator::onZeroOpacityReached;

        if (mEnableAnimations) {
            // TODO: handle case where existing animation is incomplete and needs to be reversed
            destroyAnimator();
            mAnimator =
                    ObjectAnimator.ofFloat(
                            mBackdrop, ChildrenAlphaProperty.CHILDREN_ALPHA, endValue);
            mAnimator.setDuration(FADE_DURATION_MS);
            mAnimator.setInterpolator(FADE_INTERPOLATOR);
            mAnimator.addListener(
                    new AnimatorListenerAdapter() {
                        @Override
                        public void onAnimationEnd(Animator animation) {
                            destroyAnimator();
                            onFinished.run();
                        }
                    });
            mAnimator.start();
        } else {
            mContents.setAlpha(endValue);
            mProgressBar.setAlpha(endValue);
            onFinished.run();
        }
    }

    void enableAnimations(boolean enable) {
        mEnableAnimations = enable;
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
        mInteractionHandler = handler;
        setOnClickListener(R.id.close_button, handler::onCloseClick);
        setOnClickListener(R.id.mini_player_container, handler::onMiniPlayerExpandClick);
        setOnClickListener(R.id.play_button, handler::onPlayPauseClick);
    }

    void setMediator(MiniPlayerMediator mediator) {
        mMediator = mediator;
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

            case STOPPED:
            case PAUSED:
                if (mLastPlaybackState != PLAYING && mLastPlaybackState != PAUSED) {
                    showOnly(mNormalLayout);
                    mProgressBar.setVisibility(View.VISIBLE);
                }
                mPlayPauseView.setImageResource(R.drawable.mini_play_button);
                mPlayPauseView.setContentDescription(
                        getResources().getString(R.string.readaloud_play));
                break;

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
        findViewById(id)
                .setOnClickListener(
                        (v) -> {
                            handler.run();
                        });
    }

    private void destroyAnimator() {
        if (mAnimator != null) {
            mAnimator.removeAllListeners();
            mAnimator.cancel();
            mAnimator = null;
        }
    }

    ObjectAnimator getAnimatorForTesting() {
        return mAnimator;
    }
}
