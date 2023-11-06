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
import android.util.AttributeSet;
import android.view.View;
import android.view.animation.Interpolator;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.TextView;

import androidx.annotation.ColorInt;

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
    private static final long SHOW_HIDE_DURATION_MS = 150L;
    private static final Interpolator SHOW_HIDE_INTERPOLATOR =
            Interpolators.FAST_OUT_SLOW_IN_INTERPOLATOR;

    private TextView mTitle;
    private TextView mPublisher;
    private ProgressBar mProgressBar;
    private ImageView mPlayPauseView;

    // Layouts related to different playback states.
    private LinearLayout mNormalLayout;
    private LinearLayout mBufferingLayout;
    private LinearLayout mErrorLayout;

    private @PlaybackListener.State int mLastPlaybackState;
    private boolean mEnableAnimations;
    private InteractionHandler mInteractionHandler;
    private int mHeightPx;
    private ObjectAnimator mTranslationYAnimator;
    private @VisibilityState int mFinalVisibility;
    private boolean mPendingShowHideAnimation;
    private MiniPlayerMediator mMediator;
    private long mNextAnimationPlayTime;

    /** Constructor for inflating from XML. */
    public MiniPlayerLayout(Context context, AttributeSet attrs) {
        super(context, attrs);
        mFinalVisibility = VisibilityState.GONE;
    }

    void destroy() {
        destroyYTranslationAnimator();
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

        Context context = getContext();
        @ColorInt int backgroundColor = SemanticColorUtils.getDefaultBgColor(context);
        @ColorInt int progressBarColor = SemanticColorUtils.getDefaultIconColor(context);
        if (ColorUtils.inNightMode(context)) {
            // This color should change to "Surface Container High" in the next Material update.
            backgroundColor = ChromeColors.getSurfaceColor(context, R.dimen.default_elevation_4);
            progressBarColor = context.getColor(R.color.baseline_primary_80);
        }
        findViewById(R.id.backdrop).setBackgroundColor(backgroundColor);
        mProgressBar.setProgressTintList(ColorStateList.valueOf(progressBarColor));

        mLastPlaybackState = PlaybackListener.State.UNKNOWN;
        mNextAnimationPlayTime = 0L;
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        super.onLayout(changed, left, top, right, bottom);
        int height = getHeight();
        if (height != 0) {
            mHeightPx = height;
        }

        if (mPendingShowHideAnimation) {
            runShowHideAnimation();
            mPendingShowHideAnimation = false;
        }
    }

    void updateVisibility(@VisibilityState int visibility) {
        // If "showing" or "hiding", eventually the view will be "visible" or "gone".
        if (visibility == VisibilityState.SHOWING) {
            visibility = VisibilityState.VISIBLE;
        } else if (visibility == VisibilityState.HIDING) {
            visibility = VisibilityState.GONE;
        }

        // Stop now if no change is needed.
        if (mFinalVisibility == visibility) {
            return;
        } else {
            mFinalVisibility = visibility;
        }

        // If there's an animation running, it must be going in the wrong direction.
        // Destroy it.
        mNextAnimationPlayTime = 0L;
        if (mTranslationYAnimator != null) {
            // Record the old animation's progress so the new one can start from the same
            // position rather than starting at fully shown or hidden.
            mNextAnimationPlayTime =
                    SHOW_HIDE_DURATION_MS - mTranslationYAnimator.getCurrentPlayTime();
            destroyYTranslationAnimator();
        }

        // If animation is disabled, show or hide the view immediately and return.
        if (!mEnableAnimations) {
            setTranslationY(0);
            setVisibility(mFinalVisibility == VisibilityState.VISIBLE ? View.VISIBLE : View.GONE);
            notifyVisibilityChanged();
            return;
        }

        // Otherwise kick off animations. Need to calculate view height at least once.
        if (mHeightPx == 0) {
            mPendingShowHideAnimation = true;
            // Causes onLayout() to run.
            setVisibility(View.INVISIBLE);
        } else {
            runShowHideAnimation();
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

    private void notifyVisibilityChanged() {
        if (mMediator != null) {
            mMediator.onVisibilityChanged(mFinalVisibility);
        }
    }

    private void runShowHideAnimation() {
        int startTranslationY = mHeightPx;
        int endTranslationY = 0;
        if (mFinalVisibility == VisibilityState.GONE) {
            startTranslationY = 0;
            endTranslationY = mHeightPx;
        }

        setTranslationY(startTranslationY);
        // View starts out VISIBLE for both show and hide.
        setVisibility(View.VISIBLE);

        mTranslationYAnimator = ObjectAnimator.ofFloat(this, View.TRANSLATION_Y, endTranslationY);
        mTranslationYAnimator.setDuration(SHOW_HIDE_DURATION_MS);
        mTranslationYAnimator.setInterpolator(SHOW_HIDE_INTERPOLATOR);
        mTranslationYAnimator.addListener(
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationEnd(Animator animation) {
                        destroyYTranslationAnimator();
                        if (mFinalVisibility == VisibilityState.GONE) {
                            setVisibility(View.GONE);
                        }
                        notifyVisibilityChanged();
                    }
                });
        // This call must come after setDuration().
        mTranslationYAnimator.setCurrentPlayTime(mNextAnimationPlayTime);
        mTranslationYAnimator.start();
    }

    private void destroyYTranslationAnimator() {
        if (mTranslationYAnimator != null) {
            mTranslationYAnimator.removeAllListeners();
            mTranslationYAnimator.cancel();
            mTranslationYAnimator = null;
        }
    }

    ObjectAnimator getYTranslationAnimatorForTesting() {
        return mTranslationYAnimator;
    }
}
