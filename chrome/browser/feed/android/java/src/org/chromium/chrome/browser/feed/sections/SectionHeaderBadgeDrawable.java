// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.sections;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.ValueAnimator;
import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Canvas;
import android.graphics.ColorFilter;
import android.graphics.Paint;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.appcompat.widget.AppCompatTextView;

import com.google.android.material.shape.MaterialShapeDrawable;

import org.chromium.chrome.browser.feed.R;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;

/**
 * Drawable representing the unread dot.
 *
 * Allows for setting of text inside the dot and animating the width when text changes.
 */
public class SectionHeaderBadgeDrawable extends Drawable {
    private static final int ANIMATION_DURATION_MS = 400;
    private static final int ANIMATION_START_DELAY_MS = 1500;

    private final Paint mPaint;
    private final MaterialShapeDrawable mShapeDrawable;
    private final Context mContext;
    // Default text size.
    private final float mTextSize;

    private String mText;
    private ValueAnimator mAnimator;
    private View mAnchor;
    private boolean mHasPendingAnimation;

    public SectionHeaderBadgeDrawable(Context context) {
        mContext = context;

        // This textview is only used to easily set the text parameters and calculate textual width.
        AppCompatTextView textView = new AppCompatTextView(context);
        textView.setTextAppearance(context, R.style.TextAppearance_Material3_LabelSmall);
        mPaint = textView.getPaint();
        mPaint.setTextAlign(Paint.Align.CENTER);
        mPaint.setColor(SemanticColorUtils.getDefaultTextColorOnAccent1(context));
        mTextSize = mPaint.getTextSize();

        mShapeDrawable = new MaterialShapeDrawable();
        mShapeDrawable.setCornerSize(
                mContext.getResources().getDimensionPixelSize(R.dimen.feed_badge_radius));
        mShapeDrawable.setFillColor(
                ColorStateList.valueOf(SemanticColorUtils.getDefaultTextColorAccent1(context)));
        mText = "";
    }

    /**
     * Sets the text showing inside the dot.
     *
     * If we are currently attached to an anchor and the text is not empty, we will show
     * the text and start the animation back into a text-less dot after 500ms. Otherwise, we will
     * perform the animation back into a text-less dot 500ms after attaching to an anchor.
     *
     * @param text The text to show inside the dot.
     */
    public void setText(String text) {
        // Cast null to empty string first.
        final String finalText = (text == null) ? "" : text;
        // Do nothing if no change.
        if (mText.equals(finalText)) return;
        mText = finalText;
        // If mText is empty, restore alpha and text sizes, in case we had an animation before.
        if (mText.isEmpty()) {
            mPaint.setAlpha(255);
            mPaint.setTextSize(mTextSize);
            mHasPendingAnimation = false; // Turn off any pending animation.
            // Recalculate bounds and redraw if we are attached.
            if (mAnchor != null) {
                setBounds(calculateBounds(mAnchor, mText));
                invalidateSelf();
            }
        }
    }

    public void startAnimation() {
        // Don't animate if nothing to animate.
        if (mText.isEmpty()) {
            return;
        }
        if (mAnimator != null && mAnimator.isStarted()) {
            mAnimator.pause();
        }
        if (mAnchor == null) {
            mHasPendingAnimation = true;
            return;
        }
        setUpAndRunAnimation();
    }

    @Override
    public void draw(Canvas canvas) {
        Rect bounds = getBounds();

        if (bounds.isEmpty() || getAlpha() == 0 || !isVisible()) {
            return;
        }

        mShapeDrawable.draw(canvas);

        // Draws the full text with the top left corner at (centerX, centerY-(halfheight of text)).
        // We define halfheight as the average of ascent and descent to ensure the text does not
        // appear lopsided even if the font changes.
        canvas.drawText(
                mText,
                bounds.centerX(),
                bounds.centerY() - ((mPaint.descent() + mPaint.ascent()) / 2),
                mPaint);
    }

    @Override
    public void setAlpha(int alpha) {
        mPaint.setAlpha(alpha);
        invalidateSelf();
    }

    @Override
    public void setColorFilter(@Nullable ColorFilter colorFilter) {
        mPaint.setColorFilter(colorFilter);
        invalidateSelf();
    }

    @Override
    public void setBounds(Rect bounds) {
        mShapeDrawable.setBounds(bounds);
        super.setBounds(bounds);
    }

    @Override
    public int getOpacity() {
        return mPaint.getAlpha();
    }

    /**
     * Attaches the drawable to an anchor.
     *
     * Does not do anything if we are already attached to this anchor. Otherwise,
     * triggers a draw loop which will put this drawable in the top right corner of the anchor.
     * @param anchor View to anchor this Drawable to.
     */
    public void attach(View anchor) {
        // Do not re-attach to same anchor view. Otherwise, this forces a layout
        // that messes up any ongoing animation.
        if (mAnchor != null && mAnchor.equals(anchor)) {
            invalidateSelf();
            return;
        }
        mAnchor = anchor;
        setBounds(calculateBounds(anchor, mText));
        anchor.getOverlay().add(this);
        invalidateSelf();
        // If we have a pending animation, set it up and run it..
        if (mHasPendingAnimation) {
            mHasPendingAnimation = false;
            setUpAndRunAnimation();
        }
    }

    /**
     * Detaches the drawable from the anchor.
     *
     * Does nothing if we are not attached to this anchor.
     * @param anchor View anchor that we previously called attach on.
     */
    public void detach(View anchor) {
        if (mAnchor == null || !mAnchor.equals(anchor)) {
            return;
        }
        mAnchor = null;
        anchor.getOverlay().remove(this);
        invalidateSelf();
    }

    /**
     * Calculates the bounds for this drawable if we were anchored to this view and
     * contains this text. Does not actually modify anything.
     *
     * @param anchor The view we are hypothetically anchored to.
     * @param text The text we hypothetically should display.
     * @return The bounds we should have in order to show in the top right corner.
     */
    private Rect calculateBounds(View anchor, String text) {
        Rect anchorBounds = new Rect();
        anchor.getDrawingRect(anchorBounds);

        Rect textBounds = new Rect();
        mPaint.getTextBounds(text, 0, text.length(), textBounds);

        int radius = mContext.getResources().getDimensionPixelSize(R.dimen.feed_badge_radius);
        // HalfHeight is radius if no text, or text height / 2.
        int halfHeight = Math.max(radius, (textBounds.bottom - textBounds.top) / 2 + radius);
        // HalfWidth is the radius if no text, or the text width/2.
        int halfWidth = Math.max(radius, (textBounds.right - textBounds.left) / 2 + radius);

        // CenterY is the top of the bounding box + a custom vertical offset.
        int centerY =
                anchorBounds.top
                        + mContext.getResources().getDimensionPixelSize(R.dimen.feed_badge_voffset)
                        + halfHeight;
        // CenterX is the right side of the bounding box + radius + offset.
        int centerX =
                anchorBounds.right
                        + halfWidth
                        - mContext.getResources().getDimensionPixelSize(R.dimen.feed_badge_hoffset);

        // The new bounds for the dot + any text to be rendered therein.
        return new Rect(
                centerX - halfWidth,
                centerY - halfHeight,
                centerX + halfWidth,
                centerY + halfHeight);
    }

    /**
     * Sets up and starts an animation which would turn us from a badge containing text
     * back into a dot not containing any text.
     *
     * The animation will have a 500ms delay before beginning.
     */
    private void setUpAndRunAnimation() {
        mAnimator = ValueAnimator.ofInt(0, 100);
        Rect bounds = getBounds();
        Rect toBounds = calculateBounds(mAnchor, "");
        mAnimator.addUpdateListener(
                (ValueAnimator animation) -> {
                    float fraction = animation.getAnimatedFraction();
                    Rect newBounds =
                            new Rect(
                                    (int) (bounds.left - (bounds.left - toBounds.left) * fraction),
                                    (int) (bounds.top - (bounds.top - toBounds.top) * fraction),
                                    (int)
                                            (bounds.right
                                                    - (bounds.right - toBounds.right) * fraction),
                                    (int)
                                            (bounds.bottom
                                                    - (bounds.bottom - toBounds.bottom)
                                                            * fraction));
                    mPaint.setAlpha((int) (255 - 255 * fraction));
                    mPaint.setTextSize(mTextSize - mTextSize * fraction);
                    setBounds(newBounds);
                    invalidateSelf();
                });
        mAnimator.addListener(
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationEnd(Animator animator) {
                        mText = "";
                        invalidateSelf();
                    }
                });
        mAnimator.setStartDelay(ANIMATION_START_DELAY_MS);
        mAnimator.setDuration(ANIMATION_DURATION_MS);
        mAnimator.start();
    }

    boolean getHasPendingAnimationForTest() {
        return mHasPendingAnimation;
    }

    ValueAnimator getAnimatorForTest() {
        return mAnimator;
    }
}
