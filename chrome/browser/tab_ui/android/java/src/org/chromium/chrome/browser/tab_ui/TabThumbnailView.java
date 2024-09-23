// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_ui;

import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Matrix;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.PorterDuff;
import android.graphics.RectF;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.Icon;
import android.graphics.drawable.VectorDrawable;
import android.net.Uri;
import android.os.Build;
import android.util.AttributeSet;
import android.widget.ImageView;
import androidx.annotation.Nullable;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.view.ViewCompat;

/**
 * A specialized {@link ImageView} that clips a thumbnail to a card shape with varied corner
 * radii. Overlays a background drawable. The height is varied based on the width and the
 * aspect ratio of the image.
 *
 * Alternatively, this could be implemented using
 * * ShapeableImageView - however, this is inconsistent for hardware/software based draws.
 * * RoundedCornerImageView - however, this doesn't handle non-Bitmap Drawables well.
 */
public class TabThumbnailView extends ImageView {
    private static final boolean SUPPORTS_ANTI_ALIAS_CLIP =
            Build.VERSION.SDK_INT >= Build.VERSION_CODES.P;

    /** Placeholder drawable constants. */
    private static final float SIZE_PERCENTAGE = 0.42f;

    private static Integer sVerticalOffsetPx;

    /** To prevent {@link TabThumbnailView#updateImage()} from running during inflation. */
    private boolean mInitialized;

    /**
     * Placeholder icon drawable to use if there is no thumbnail. This is drawn on-top of the
     * {@link mBackgroundDrawable} which defines the shape of the thumbnail. There are two
     * separate layers because the background scales with the thumbnail size whereas the icon
     * will be the SIZE_PERCENTAGE of the minimum side length of the thumbnail size centered
     * and adjusted upwards.
     */
    private VectorDrawable mIconDrawable;

    private Matrix mIconMatrix;
    private int mIconColor;

    /**
     * Background drawable which is present while in placeholder mode.
     * Once a thumbnail is set this will be removed.
     */
    private final GradientDrawable mBackgroundDrawable;

    // Pre-allocate to avoid repeat calls during {@link onDraw}.
    private final Paint mPaint;
    private final Path mPath;
    private final RectF mRectF;

    // Realistically this will be set once and never again.
    private float[] mRadii;

    public TabThumbnailView(Context context, AttributeSet attrs) {
        this(context, attrs, 0);
    }

    public TabThumbnailView(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);

        if (sVerticalOffsetPx == null) {
            sVerticalOffsetPx =
                    context.getResources()
                            .getDimensionPixelSize(
                                    R.dimen.tab_thumbnail_placeholder_vertical_offset);
        }

        mPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        mPaint.setStyle(Paint.Style.STROKE);
        mPaint.setStrokeWidth(1);
        mPath = new Path();
        mRectF = new RectF();
        mIconMatrix = new Matrix();
        mBackgroundDrawable = new GradientDrawable();

        TypedArray a =
                getContext().obtainStyledAttributes(attrs, R.styleable.TabThumbnailView, 0, 0);
        int radiusTopStart =
                a.getDimensionPixelSize(R.styleable.TabThumbnailView_cornerRadiusTopStart, 0);
        int radiusTopEnd =
                a.getDimensionPixelSize(R.styleable.TabThumbnailView_cornerRadiusTopEnd, 0);
        int radiusBottomStart =
                a.getDimensionPixelSize(R.styleable.TabThumbnailView_cornerRadiusBottomStart, 0);
        int radiusBottomEnd =
                a.getDimensionPixelSize(R.styleable.TabThumbnailView_cornerRadiusBottomEnd, 0);
        a.recycle();

        setRoundedCorners(radiusTopStart, radiusTopEnd, radiusBottomStart, radiusBottomEnd);
        mInitialized = true;
        updateImage();
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
        if (!mInitialized) return;

        mRectF.set(0, 0, getMeasuredWidth(), getMeasuredHeight());
        resizeIconDrawable();
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
        if (!mInitialized) {
            super.onDraw(canvas);
            return;
        }
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
        if (!mInitialized) return;
        // If the drawable is empty, display a placeholder image.
        if (isPlaceholder()) {
            setBackground(mBackgroundDrawable);
            updateIconDrawable();
            return;
        }

        clearColorFilter();
        mIconDrawable = null;
        // Remove the background drawable as mini group thumbnails have a transparent space between
        // them and normal thumbnails are opaque.
        setBackground(null);
    }

    private void updateIconDrawable() {
        if (mIconDrawable == null) {
            mIconDrawable =
                    (VectorDrawable)
                            AppCompatResources.getDrawable(
                                    getContext(), R.drawable.ic_tab_placeholder);
        }
        setColorFilter(mIconColor, PorterDuff.Mode.SRC_IN);
        // External callers either change this or use MATRIX there is no need to reset this.
        // See {@link TabGridViewBinder#updateThumbnail()}.
        setScaleType(ImageView.ScaleType.MATRIX);
        resizeIconDrawable();
        super.setImageDrawable(mIconDrawable);
    }

    /**
     * @return whether the image drawable is a placeholder.
     */
    public boolean isPlaceholder() {
        // The drawable can only be null if we just removed the drawable and need to set the
        // mIconDrawable.
        if (getDrawable() == null) return true;

        // Otherwise there should always be a thumbnail or placeholder drawable.
        if (mIconDrawable == null) return false;
        return getDrawable() == mIconDrawable;
    }

    /**
     * Set the thumbnail placeholder based on whether it is used for an incognito / selected tab.
     * @param isIncognito Whether the thumbnail is on an incognito tab.
     * @param isSelected Whether the thumbnail is on a selected tab.
     */
    public void updateThumbnailPlaceholder(boolean isIncognito, boolean isSelected) {
        // Step 1: Background color.
        mBackgroundDrawable.setColor(
                TabUiThemeUtils.getMiniThumbnailPlaceholderColor(
                        getContext(), isIncognito, isSelected));
        final int oldColor = mPaint.getColor();
        final int newColor =
                TabUiThemeUtils.getCardViewBackgroundColor(
                        getContext(), isIncognito, isSelected);
        mPaint.setColor(newColor);

        // Step 2: Placeholder icon.
        // Make property changes outside the flag intentionally in the event the flag flips status
        // these will have no material effect on the UI and are safe.
        mIconColor = newColor;
        if (mIconDrawable != null) {
            setColorFilter(mIconColor, PorterDuff.Mode.SRC_IN);
        }

        // Step 3: Invalidate for versions earlier than Android P.
        if (!SUPPORTS_ANTI_ALIAS_CLIP && !isPlaceholder() && oldColor != newColor) {
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
    void setRoundedCorners(
            int cornerRadiusTopStart,
            int cornerRadiusTopEnd,
            int cornerRadiusBottomStart,
            int cornerRadiusBottomEnd) {
        // This is borrowed from {@link RoundedCornerImageView}. It could be further simplified as
        // at present the only radii distinction is top vs bottom.
        if (ViewCompat.getLayoutDirection(this) == ViewCompat.LAYOUT_DIRECTION_LTR) {
            mRadii =
                    new float[] {
                        cornerRadiusTopStart,
                        cornerRadiusTopStart,
                        cornerRadiusTopEnd,
                        cornerRadiusTopEnd,
                        cornerRadiusBottomEnd,
                        cornerRadiusBottomEnd,
                        cornerRadiusBottomStart,
                        cornerRadiusBottomStart
                    };
        } else {
            mRadii =
                    new float[] {
                        cornerRadiusTopEnd,
                        cornerRadiusTopEnd,
                        cornerRadiusTopStart,
                        cornerRadiusTopStart,
                        cornerRadiusBottomStart,
                        cornerRadiusBottomStart,
                        cornerRadiusBottomEnd,
                        cornerRadiusBottomEnd
                    };
        }

        mBackgroundDrawable.setCornerRadii(mRadii);
    }

    private void resizeIconDrawable() {
        if (mIconDrawable != null) {
            // Called in onMeasure() so getWidth() and getHeight() may not be ready yet.
            final int width = getMeasuredWidth();
            final int height = getMeasuredHeight();

            // Vector graphic is square so width or height doesn't matter.
            final int vectorEdgeLength = mIconDrawable.getIntrinsicWidth();

            // Shortest edge of thumbnail region * SIZE_PERCENTAGE.
            final int edgeLength = Math.round(SIZE_PERCENTAGE * Math.min(width, height));
            final float scale = (float) edgeLength / (float) vectorEdgeLength;
            mIconMatrix.reset();
            mIconMatrix.postScale(scale, scale);

            // Center and offset vertically by sVerticalOffsetPx to account for optical illusion of
            // centering.
            mIconMatrix.postTranslate(
                    (float) (width - edgeLength) / 2f,
                    (float) (height - edgeLength) / 2f - sVerticalOffsetPx);
            setImageMatrix(mIconMatrix);
        }
    }

    public VectorDrawable getIconDrawableForTesting() {
        return mIconDrawable;
    }
}
