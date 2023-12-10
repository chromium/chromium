// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list.view;

import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.Canvas;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.View;
import android.widget.ImageView;

import androidx.annotation.IntDef;
import androidx.annotation.StringRes;

import org.chromium.base.MathUtils;
import org.chromium.chrome.browser.download.internal.R;
import org.chromium.components.browser_ui.widget.async_image.AutoAnimatorDrawable;
import org.chromium.components.browser_ui.widget.async_image.ForegroundDrawableCompat;
import org.chromium.ui.UiUtils;
import org.chromium.ui.widget.ChromeImageButton;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * A representation of a progress bar that supports (1) an indeterminate state, (2) a determinate
 * state, and (3) running, paused, and retry states.
 *
 * The determinate {@link Drawable} will have it's level set via {@link Drawable#setLevel(int)}
 * based on the progress (0 - 10,000).
 *
 * The indeterminate and determinate {@link Drawable}s support {@link Animatable} drawables and the
 * animation will be started/stopped when shown/hidden respectively.
 */
public class CircularProgressView extends ChromeImageButton {
    /**
     * The value to use with {@link #setProgress(int)} to specify that the indeterminate
     * {@link Drawable} should be used.
     */
    public static final int INDETERMINATE = -1;

    /** The various states this {@link CircularProgressView} can be in. */
    @IntDef({UiState.RUNNING, UiState.PAUSED, UiState.RETRY})
    @Retention(RetentionPolicy.SOURCE)
    public @interface UiState {
        /** This progress bar will look like it is actively running based on the XML drawable. */
        int RUNNING = 0;

        /** This progress bar will look like it is paused based on the XML drawable. */
        int PAUSED = 1;

        /** This progress bar will look like it is able to be retried based on the XML drawable. */
        int RETRY = 2;
    }

    private static final int MAX_LEVEL = 10000;

    private final Drawable mIndeterminateProgress;
    private final Drawable mDeterminateProgress;
    private final Drawable mResumeButtonSrc;
    private final Drawable mPauseButtonSrc;
    private final Drawable mRetryButtonSrc;

    private final ForegroundDrawableCompat mForegroundHelper;

    /**
     * Creates an instance of a {@link CircularProgressView}.
     * @param context  The {@link Context} to use.
     * @param attrs    An {@link AttributeSet} instance.
     */
    public CircularProgressView(Context context, AttributeSet attrs) {
        super(context, attrs);

        mForegroundHelper = new ForegroundDrawableCompat(this);
        mForegroundHelper.setScaleType(ImageView.ScaleType.FIT_XY);

        TypedArray types =
                attrs == null
                        ? null
                        : context.obtainStyledAttributes(
                                attrs, R.styleable.CircularProgressView, 0, 0);

        mIndeterminateProgress =
                AutoAnimatorDrawable.wrap(
                        UiUtils.getDrawable(
                                context,
                                types,
                                R.styleable.CircularProgressView_indeterminateProgress));
        mDeterminateProgress =
                AutoAnimatorDrawable.wrap(
                        UiUtils.getDrawable(
                                context,
                                types,
                                R.styleable.CircularProgressView_determinateProgress));
        mResumeButtonSrc =
                UiUtils.getDrawable(context, types, R.styleable.CircularProgressView_resumeSrc);
        mPauseButtonSrc =
                UiUtils.getDrawable(context, types, R.styleable.CircularProgressView_pauseSrc);
        mRetryButtonSrc =
                UiUtils.getDrawable(context, types, R.styleable.CircularProgressView_retrySrc);

        if (types != null) types.recycle();
    }

    /**
     * Sets the progress of this {@link CircularProgressView} to {@code progress}.  If {@code
     * progress} is {@link #INDETERMINATE} an indeterminate {@link Drawable} will be used.
     * Otherwise the value will be clamped between 0 and 100 and a determinate {@link Drawable} will
     * be used and have it's level set via {@link Drawable#setLevel(int)}.
     *
     * @param progress The progress value (0 to 100 or {@link #INDETERMINATE}) to show.
     */
    public void setProgress(int progress) {
        if (progress == INDETERMINATE) {
            mForegroundHelper.setDrawable(mIndeterminateProgress);
        } else {
            if (mDeterminateProgress != null) {
                progress = MathUtils.clamp(progress, 0, 100);
                mDeterminateProgress.setLevel(progress * MAX_LEVEL / 100);
            }
            mForegroundHelper.setDrawable(mDeterminateProgress);
        }
    }

    /**
     * The state this {@link CircularProgressView} should show.  This can be one of the three
     * UiStates defined above.  This will determine what the action drawable is in the view.
     * @param state The UiState to use.
     */
    public void setState(@UiState int state) {
        Drawable imageDrawable;
        @StringRes int contentDescription;
        switch (state) {
            case UiState.RUNNING:
                imageDrawable = mPauseButtonSrc;
                contentDescription = R.string.download_notification_pause_button;
                break;
            case UiState.PAUSED:
                imageDrawable = mResumeButtonSrc;
                contentDescription = R.string.download_notification_resume_button;
                break;
            case UiState.RETRY:
            default:
                imageDrawable = mRetryButtonSrc;
                contentDescription = R.string.download_notification_resume_button;
                break;
        }

        setImageDrawable(imageDrawable);
        setContentDescription(getContext().getText(contentDescription));
    }

    // AppCompatImageButton implementation.
    @Override
    public void draw(Canvas canvas) {
        super.draw(canvas);
        mForegroundHelper.draw(canvas);
    }

    @Override
    protected void onVisibilityChanged(View changedView, int visibility) {
        super.onVisibilityChanged(changedView, visibility);
        mForegroundHelper.onVisibilityChanged(changedView, visibility);
    }

    @Override
    protected void drawableStateChanged() {
        super.drawableStateChanged();
        mForegroundHelper.drawableStateChanged();
    }

    @Override
    protected boolean verifyDrawable(Drawable dr) {
        return super.verifyDrawable(dr) || mForegroundHelper.verifyDrawable(dr);
    }
}
