// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.RectF;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.Icon;
import android.net.Uri;
import android.os.Build;
import android.util.AttributeSet;
import android.widget.ImageView;

import androidx.annotation.Nullable;
import androidx.core.view.ViewCompat;

import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.chrome.tab_ui.R;

/**
 * A specialized {@link ImageView} that clips a thumbnail to a card shape with varied corner
 * radii. Overlays a background drawable. The height is varied based on the width and the
 * aspect ratio of the image.
 *
 * Alternatively, this could be implemented using
 * * ShapeableImageView - however, this is inconsistent for hardware/software based draws.
 * * RoundedCornerImageView - however, this doesn't handle non-Bitmap Drawables well.
 */
public class TabGridThumbnailView extends ImageView {
    private static final boolean SUPPORTS_ANTI_ALIAS_CLIP =
            Build.VERSION.SDK_INT >= Build.VERSION_CODES.P;

    private final GradientDrawable mBackgroundDrawable;

    // Pre-allocate to avoid repeat calls during {@link onDraw}.
    private final Paint mPaint;
    private final Path mPath;
    private final RectF mRectF;

    // Realistically this will be set once and never again.
    private float[] mRadii;

    public TabGridThumbnailView(Context context, AttributeSet attrs) {
        this(context, attrs, 0);
    }

    public TabGridThumbnailView(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
        mPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        mPaint.setStyle(Paint.Style.STROKE);
        mPaint.setStrokeWidth(1);
        mPath = new Path();
        mRectF = new RectF();
        mBackgroundDrawable = new GradientDrawable();

        TypedArray a =
                getContext().obtainStyledAttributes(attrs, R.styleable.TabGridThumbnailView, 0, 0);
        int radiusTopStart =
                a.getDimensionPixelSize(R.styleable.TabGridThumbnailView_cornerRadiusTopStart, 0);
        int radiusTopEnd =
                a.getDimensionPixelSize(R.styleable.TabGridThumbnailView_cornerRadiusTopEnd, 0);
        int radiusBottomStart = a.getDimensionPixelSize(
                R.styleable.TabGridThumbnailView_cornerRadiusBottomStart, 0);
        int radiusBottomEnd =
                a.getDimensionPixelSize(R.styleable.TabGridThumbnailView_cornerRadiusBottomEnd, 0);
        a.recycle();

        setRoundedCorners(radiusTopStart, radiusTopEnd, radiusBottomStart, radiusBottomEnd);
        setBackground(mBackgroundDrawable);
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);

        int measuredWidth = getMeasuredWidth();
        int measureHeight = getMeasuredHeight();

        // TODO(crbug/1434775): Consider fixing the aspect ratio and cropping/resizing the Drawable
        // to fit.
        int expectedHeight =
                (int) (measuredWidth * 1.0 / TabUtils.getTabThumbnailAspectRatio(getContext()));
        if (isPlaceHolder()) {
            measureHeight = expectedHeight;
        }

        setMeasuredDimension(measuredWidth, measureHeight);
        mRectF.set(0, 0, getMeasuredWidth(), getMeasuredHeight());
    }

    @Override
    public void setImageBitmap(Bitmap bitmap) {
        super.setImageBitmap(bitmap);
        updateImage();
    }

    @Override
    public void setImageIcon(Icon icon) {
        super.setImageIcon(icon);
        updateImage();
    }

    @Override
    public void setImageDrawable(@Nullable Drawable drawable) {
        super.setImageDrawable(drawable);
        updateImage();
    }

    @Override
    public void setImageResource(int resId) {
        super.setImageResource(resId);
        updateImage();
    }

    @Override
    public void setImageURI(Uri uri) {
        super.setImageURI(uri);
        updateImage();
    }

    @Override
    public void onDraw(Canvas canvas) {
        mPath.reset();
        mPath.addRoundRect(mRectF, mRadii, Path.Direction.CW);
        canvas.save();
        canvas.clipPath(mPath);
        super.onDraw(canvas);
        canvas.restore();
        // clipPath did not anti-alias or have a method to do so until Android P. For earlier
        // versions draw a very thin stroke of the background color to anti-alias the edges.
        if (!SUPPORTS_ANTI_ALIAS_CLIP) {
            canvas.drawPath(mPath, mPaint);
        }
    }

    private void updateImage() {
        if (isPlaceHolder()) {
            // If the drawable is empty, display a placeholder.
            setBackground(mBackgroundDrawable);
            return;
        }
        // Remove the background as multi-thumbnails have transparency.
        setBackground(null);
    }

    /**
     * @return whether the image drawable is a placeholder.
     */
    boolean isPlaceHolder() {
        return getDrawable() == null;
    }

    /**
     * Set the thumbnail placeholder based on whether it is used for an incognito / selected tab.
     * @param isIncognito Whether the thumbnail is on an incognito tab.
     * @param isSelected Whether the thumbnail is on a selected tab.
     */
    void setColorThumbnailPlaceHolder(boolean isIncognito, boolean isSelected) {
        mBackgroundDrawable.setColor(TabUiThemeProvider.getMiniThumbnailPlaceHolderColor(
                getContext(), isIncognito, isSelected));
        int oldColor = mPaint.getColor();
        mPaint.setColor(TabUiThemeProvider.getCardViewBackgroundColor(
                getContext(), isIncognito, isSelected));
        if (!SUPPORTS_ANTI_ALIAS_CLIP && oldColor != mPaint.getColor()) {
            invalidate();
        }
    }

    /**
     * Sets the rounded corner radii.
     * @param cornerRadiusTopStart top start corner radius.
     * @param cornerRadiusTopEnd top end corner radius.
     * @param cornerRadiusBottomStart bottom start corner radius.
     * @param cornerRadiusBottomEnd bottom end corner radius.
     */
    void setRoundedCorners(int cornerRadiusTopStart, int cornerRadiusTopEnd,
            int cornerRadiusBottomStart, int cornerRadiusBottomEnd) {
        // This is borrowed from {@link RoundedCornerImageView}. It could be further simplified as
        // at present the only radii distinction is top vs bottom.
        if (ViewCompat.getLayoutDirection(this) == ViewCompat.LAYOUT_DIRECTION_LTR) {
            mRadii = new float[] {cornerRadiusTopStart, cornerRadiusTopStart, cornerRadiusTopEnd,
                    cornerRadiusTopEnd, cornerRadiusBottomEnd, cornerRadiusBottomEnd,
                    cornerRadiusBottomStart, cornerRadiusBottomStart};
        } else {
            mRadii = new float[] {cornerRadiusTopEnd, cornerRadiusTopEnd, cornerRadiusTopStart,
                    cornerRadiusTopStart, cornerRadiusBottomStart, cornerRadiusBottomStart,
                    cornerRadiusBottomEnd, cornerRadiusBottomEnd};
        }

        mBackgroundDrawable.setCornerRadii(mRadii);
    }
}
