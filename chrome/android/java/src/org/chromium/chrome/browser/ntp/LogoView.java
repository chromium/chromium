// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import android.animation.Animator;
import android.animation.ObjectAnimator;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Bitmap.Config;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Matrix;
import android.graphics.Paint;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffColorFilter;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.util.Property;
import android.view.Gravity;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.FrameLayout;

import androidx.annotation.ColorInt;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ntp.LogoBridge.Logo;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.ui.widget.LoadingView;

import java.lang.ref.WeakReference;

import jp.tomorrowkey.android.gifplayer.BaseGifDrawable;
import jp.tomorrowkey.android.gifplayer.BaseGifImage;

/**
 * This view shows the default search provider's logo and fades in a new logo if one becomes
 * available. It also maintains a {@link BaseGifDrawable} that will be played when the user clicks
 * this view and we have an animated GIF logo ready.
 */
public class LogoView extends FrameLayout implements OnClickListener {

    // Number of milliseconds for a new logo to fade in.
    private static final int LOGO_TRANSITION_TIME_MS = 400;

    // The default logo is shared across all NTPs.
    private static WeakReference<Bitmap> sDefaultLogo;
    private static @ColorInt int sDefaultLogoTint;

    // mLogo and mNewLogo are remembered for cross fading animation.
    private Bitmap mLogo;
    private Bitmap mNewLogo;
    private BaseGifDrawable mAnimatedLogoDrawable;

    private ObjectAnimator mFadeAnimation;
    private Paint mPaint;
    private Matrix mLogoMatrix;
    private Matrix mNewLogoMatrix;
    private Matrix mAnimatedLogoMatrix;
    private boolean mLogoIsDefault;
    private boolean mNewLogoIsDefault;

    private LoadingView mLoadingView;

    /**
     * A measure from 0 to 1 of how much the new logo has faded in. 0 shows the old logo, 1 shows
     * the new logo, and intermediate values show the new logo cross-fading in over the old logo.
     * Set to 0 when not transitioning.
     */
    private float mTransitionAmount;

    private Delegate mDelegate;

    private final Property<LogoView, Float> mTransitionProperty =
            new Property<LogoView, Float>(Float.class, "") {
        @Override
        public Float get(LogoView logoView) {
            return logoView.mTransitionAmount;
        }

        @Override
        public void set(LogoView logoView, Float amount) {
            assert amount >= 0f;
            assert amount <= 1f;
            if (logoView.mTransitionAmount != amount) {
                logoView.mTransitionAmount = amount;
                invalidate();
            }
        }
    };

    /**
     * Handles tasks for the {@link LogoView} shown on an NTP.
     */
    public interface Delegate {
        /**
         * Called when the user clicks on the logo.
         * @param isAnimatedLogoShowing Whether the animated GIF logo is playing.
         */
        void onLogoClicked(boolean isAnimatedLogoShowing);
    }

    /**
     * Constructor used to inflate a LogoView from XML.
     */
    public LogoView(Context context, AttributeSet attrs) {
        super(context, attrs);

        mLogoMatrix = new Matrix();
        mLogoIsDefault = true;

        mPaint = new Paint();
        mPaint.setFilterBitmap(true);

        // Mark this view as non-clickable so that accessibility will ignore it. When a non-default
        // logo is shown, this view will be marked clickable again.
        setOnClickListener(this);
        setClickable(false);
        setWillNotDraw(false);

        mLoadingView = new LoadingView(getContext());
        LayoutParams lp = new LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT);
        lp.gravity = Gravity.CENTER;
        mLoadingView.setLayoutParams(lp);
        mLoadingView.setVisibility(View.GONE);
        addView(mLoadingView);
    }

    /**
     * Sets the {@link Delegate} to notify when the logo is pressed.
     */
    public void setDelegate(Delegate delegate) {
        mDelegate = delegate;
    }

    /**
     * Jumps to the end of the logo cross-fading animation, if any.
     */
    public void endFadeAnimation() {
        if (mFadeAnimation != null) {
            mFadeAnimation.end();
            mFadeAnimation = null;
        }
    }

    /**
     * @return True after we receive an animated logo from the server.
     */
    private boolean isAnimatedLogoShowing() {
        return mAnimatedLogoDrawable != null;
    }

    /**
     * Starts playing the given animated GIF logo.
     */
    public void playAnimatedLogo(BaseGifImage gifImage) {
        mLoadingView.hideLoadingUI();
        mAnimatedLogoDrawable = new BaseGifDrawable(gifImage, Config.ARGB_8888);
        mAnimatedLogoMatrix = new Matrix();
        setMatrix(mAnimatedLogoDrawable.getIntrinsicWidth(),
                mAnimatedLogoDrawable.getIntrinsicHeight(), mAnimatedLogoMatrix, false);
        // Set callback here to ensure #invalidateDrawable() is called.
        mAnimatedLogoDrawable.setCallback(this);
        mAnimatedLogoDrawable.start();
    }

    /**
     * Show a spinning progressbar.
     */
    public void showLoadingView() {
        mLogo = null;
        invalidate();
        mLoadingView.showLoadingUI();
    }

    /**
     * Show a loading indicator or a baked-in default search provider logo, based on what is
     * available.
     */
    public void showSearchProviderInitialView() {
        if (maybeShowDefaultLogo()) return;

        showLoadingView();
    }

    /**
     * Fades in a new logo over the current logo.
     *
     * @param logo The new logo to fade in.
     */
    public void updateLogo(Logo logo) {
        if (logo == null) {
            if (maybeShowDefaultLogo()) return;

            mLogo = null;
            invalidate();
            return;
        }

        String contentDescription = TextUtils.isEmpty(logo.altText)
                ? null
                : getResources().getString(R.string.accessibility_google_doodle, logo.altText);
        updateLogo(logo.image, contentDescription, false);
    }

    private void updateLogo(Bitmap logo, final String contentDescription, boolean isDefaultLogo) {
        assert logo != null;

        if (mFadeAnimation != null) mFadeAnimation.end();

        mLoadingView.hideLoadingUI();

        // Don't crossfade if the new logo is the same as the old one.
        if (mLogo != null && mLogo.sameAs(logo)) return;

        mNewLogo = logo;
        mNewLogoMatrix = new Matrix();
        mNewLogoIsDefault = isDefaultLogo;
        setMatrix(mNewLogo.getWidth(), mNewLogo.getHeight(), mNewLogoMatrix, mNewLogoIsDefault);

        mFadeAnimation = ObjectAnimator.ofFloat(this, mTransitionProperty, 0f, 1f);
        mFadeAnimation.setDuration(LOGO_TRANSITION_TIME_MS);
        mFadeAnimation.addListener(new Animator.AnimatorListener() {
            @Override
            public void onAnimationStart(Animator animation) {
            }

            @Override
            public void onAnimationRepeat(Animator animation) {
            }

            @Override
            public void onAnimationEnd(Animator animation) {
                mLogo = mNewLogo;
                mLogoMatrix = mNewLogoMatrix;
                mLogoIsDefault = mNewLogoIsDefault;
                mNewLogo = null;
                mNewLogoMatrix = null;
                mTransitionAmount = 0f;
                mFadeAnimation = null;
                setContentDescription(contentDescription);
                setClickable(!mNewLogoIsDefault);
            }

            @Override
            public void onAnimationCancel(Animator animation) {
                onAnimationEnd(animation);
                invalidate();
            }
        });
        mFadeAnimation.start();
    }

    /**
     * Shows the default search engine logo if available.
     * @return Whether the default search engine logo is available.
     */
    private boolean maybeShowDefaultLogo() {
        Bitmap defaultLogo = getDefaultLogo();
        if (defaultLogo != null) {
            updateLogo(defaultLogo, null, true);
            return true;
        }
        return false;
    }

    /**
     * @return Whether a new logo is currently fading in over the old logo.
     */
    private boolean isTransitioning() {
        return mTransitionAmount != 0f;
    }

    /**
     * Sets the matrix to scale and translate the image so that it will be centered in the LogoView
     * and scaled to fit within the LogoView.
     *
     * @param preventUpscaling Whether the image should not be scaled up. If true, the image might
     *                         not fill the entire view but will still be centered.
     */
    private void setMatrix(int imageWidth, int imageHeight, Matrix matrix,
            boolean preventUpscaling) {
        int width = getWidth();
        int height = getHeight();

        float scale = Math.min((float) width / imageWidth, (float) height / imageHeight);
        if (preventUpscaling) scale = Math.min(1.0f, scale);

        int imageOffsetX = Math.round((width - imageWidth * scale) * 0.5f);

        float whitespace = height - imageHeight * scale;
        int imageOffsetY = Math.round(whitespace * 0.5f);

        matrix.setScale(scale, scale);
        matrix.postTranslate(imageOffsetX, imageOffsetY);
    }

    /**
     * @return The default logo.
     */
    private Bitmap getDefaultLogo() {
        if (!TemplateUrlServiceFactory.get().isDefaultSearchEngineGoogle()) return null;

        Bitmap defaultLogo = sDefaultLogo == null ? null : sDefaultLogo.get();
        final int tint = ApiCompatibilityUtils.getColor(getResources(), R.color.google_logo_tint);
        if (defaultLogo == null || sDefaultLogoTint != tint) {
            if (tint == Color.TRANSPARENT) {
                defaultLogo = BitmapFactory.decodeResource(getResources(), R.drawable.google_logo);
            } else {
                // Apply color filter on a bitmap, which will cause some performance overhead, but
                // it is worth the APK space savings by avoiding adding another large asset for the
                // logo in night mode. Not using vector drawable here because it is close to the
                // maximum recommended vector drawable size 200dpx200dp.
                BitmapFactory.Options options = new BitmapFactory.Options();
                options.inMutable = true;
                defaultLogo = BitmapFactory.decodeResource(
                        getResources(), R.drawable.google_logo, options);
                Paint paint = new Paint();
                paint.setColorFilter(new PorterDuffColorFilter(tint, PorterDuff.Mode.SRC_ATOP));
                Canvas canvas = new Canvas(defaultLogo);
                canvas.drawBitmap(defaultLogo, 0, 0, paint);
            }
            sDefaultLogo = new WeakReference<>(defaultLogo);
            sDefaultLogoTint = tint;
        }
        return defaultLogo;
    }

    @Override
    protected boolean verifyDrawable(Drawable who) {
        return (who == mAnimatedLogoDrawable) || super.verifyDrawable(who);
    }

    @Override
    public void invalidateDrawable(Drawable drawable) {
        // mAnimatedLogoDrawable doesn't actually know its bounds, so super.invalidateDrawable()
        // doesn't invalidate the right area. Instead invalidate the entire view; the drawable takes
        // up most of the view anyway so this is just as efficient.
        // @see ImageView#invalidateDrawable().
        if (drawable == mAnimatedLogoDrawable) invalidate();
        else super.invalidateDrawable(drawable);
    }

    @Override
    protected void onDraw(Canvas canvas) {
        if (isAnimatedLogoShowing()) {
            if (mFadeAnimation != null) mFadeAnimation.cancel();
            // Free the old bitmaps to allow them to be GC'd.
            mLogo = null;
            mNewLogo = null;

            canvas.save();
            canvas.concat(mAnimatedLogoMatrix);
            mAnimatedLogoDrawable.draw(canvas);
            canvas.restore();
        } else {
            if (mLogo != null && mTransitionAmount < 0.5f) {
                mPaint.setAlpha((int) (255 * 2 * (0.5f - mTransitionAmount)));
                canvas.save();
                canvas.concat(mLogoMatrix);
                canvas.drawBitmap(mLogo, 0, 0, mPaint);
                canvas.restore();
            }

            if (mNewLogo != null && mTransitionAmount > 0.5f) {
                mPaint.setAlpha((int) (255 * 2 * (mTransitionAmount - 0.5f)));
                canvas.save();
                canvas.concat(mNewLogoMatrix);
                canvas.drawBitmap(mNewLogo, 0, 0, mPaint);
                canvas.restore();
            }
        }
    }

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        if (w != oldw || h != oldh) {
            if (mAnimatedLogoDrawable != null) {
                setMatrix(mAnimatedLogoDrawable.getIntrinsicWidth(),
                        mAnimatedLogoDrawable.getIntrinsicHeight(), mAnimatedLogoMatrix, false);
            }
            if (mLogo != null) {
                setMatrix(mLogo.getWidth(), mLogo.getHeight(), mLogoMatrix, mLogoIsDefault);
            }
            if (mNewLogo != null) {
                setMatrix(mNewLogo.getWidth(), mNewLogo.getHeight(), mNewLogoMatrix,
                        mNewLogoIsDefault);
            }
        }
    }

    @Override
    public void onClick(View view) {
        if (view == this && mDelegate != null && !isTransitioning()) {
            mDelegate.onLogoClicked(isAnimatedLogoShowing());
        }
    }
}
