// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.sections;

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
 * Allows for setting of text and animating the width when text changes.
 */
public class SectionHeaderBadgeDrawable extends Drawable {
    private Paint mPaint;
    // Only used for calculating text width in dps and to easily apply text appearances.
    private AppCompatTextView mTextView;
    private MaterialShapeDrawable mShapeDrawable;
    private String mText;
    private Context mContext;

    public SectionHeaderBadgeDrawable(Context context) {
        mContext = context;

        // This textview is only used to easily set the text parameters and calculate textual width.
        mTextView = new AppCompatTextView(context);
        mTextView.setTextAppearance(context, R.style.TextAppearance_Material3_LabelSmall);
        mPaint = mTextView.getPaint();
        mPaint.setTextAlign(Paint.Align.CENTER);

        mShapeDrawable = new MaterialShapeDrawable();
        mShapeDrawable.setCornerSize(
                mContext.getResources().getDimensionPixelSize(R.dimen.feed_badge_radius));
        mShapeDrawable.setFillColor(
                ColorStateList.valueOf(SemanticColorUtils.getDefaultTextColorAccent1(context)));
        mText = "";
    }

    public void setText(String text) {
        mText = text;
        mTextView.setText(text);
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
        canvas.drawText(mText, 0, mText.length(), bounds.centerX(),
                bounds.centerY() - ((mPaint.descent() + mPaint.ascent()) / 2), mPaint);
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

    /**
     * Called when we are attaching the drawable to a new overlay.
     *
     * @param anchorBounds the bounding box for the overlay.
     */
    @Override
    public void setBounds(Rect anchorBounds) {
        // CenterY is the top of the bounding box + a custom vertical offset.
        int centerY = anchorBounds.top
                + mContext.getResources().getDimensionPixelSize(R.dimen.feed_badge_voffset);
        int halfHeight = mContext.getResources().getDimensionPixelSize(R.dimen.feed_badge_radius);
        // HalfWidth is the radius if no text, or the text width/2.
        int halfWidth = Math.max(halfHeight, mTextView.getMeasuredWidth() / 2);
        // CenterX is the right side of the bounding box + radius + offset.
        int centerX = anchorBounds.right + halfWidth
                - mContext.getResources().getDimensionPixelSize(R.dimen.feed_badge_hoffset);
        // The new bounds for the dot + any text to be rendered therein.
        Rect newBounds = new Rect(centerX - halfWidth, centerY - halfHeight, centerX + halfWidth,
                centerY + halfHeight);
        // We don't set bounding box for the textview because
        // one does not set bounds for views, and it's not part of the drawing.
        mShapeDrawable.setBounds(newBounds);
        super.setBounds(newBounds);
    }

    @Override
    public int getOpacity() {
        return mPaint.getAlpha();
    }

    public void attach(View anchor) {
        Rect badgeBounds = new Rect();
        anchor.getDrawingRect(badgeBounds);
        setBounds(badgeBounds);
        anchor.getOverlay().add(this);
        invalidateSelf();
    }

    public void detach(View anchor) {
        anchor.getOverlay().remove(this);
        invalidateSelf();
    }
}
