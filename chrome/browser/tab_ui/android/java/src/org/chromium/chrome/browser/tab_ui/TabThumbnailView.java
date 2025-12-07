// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_ui;

import static org.chromium.build.NullUtil.assumeNonNull;

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
import android.util.AttributeSet;
import android.widget.ImageView;

import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.view.ViewCompat;

import org.chromium.build.annotations.EnsuresNonNull;
import org.chromium.build.annotations.MonotonicNonNull;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.tab_groups.TabGroupColorId;

/**
 * A specialized {@link ImageView} that clips a thumbnail to a card shape with varied corner radii.
 * Overlays a background drawable. The height is varied based on the width and the aspect ratio of
 * the image.
 *
 * <p>Alternatively, this could be implemented using * ShapeableImageView - however, this is
 * inconsistent for hardware/software based draws. * RoundedCornerImageView - however, this doesn't
 * handle non-Bitmap Drawables well.
 */
@NullMarked
public class TabThumbnailView extends ImageView {
    /** Placeholder drawable constants. */
    private static final float WIDTH_PERCENTAGE = 0.80f;

    private static final float HEIGHT_PERCENTAGE = 0.45f;

    private static @MonotonicNonNull Integer sVerticalOffsetPx;

    /** To prevent {@link TabThumbnailView#updateImage()} from running during inflation. */
    private final boolean mInitialized;

    /**
     * Placeholder icon drawable to use if there is no thumbnail. This is drawn on-top of the {@link
     * mBackgroundDrawable} which defines the shape of the thumbnail. There are two separate layers
     * because the background scales with the thumbnail size whereas the icon will be the
     * SIZE_PERCENTAGE of the minimum side length of the thumbnail size centered and adjusted
     * upwards.
     */
    private @Nullable VectorDrawable mIconDrawable;

    private final Matrix mIconMatrix;
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

    private @MonotonicNonNull float[] mRadii;

    public TabThumbnailView(Context context, AttributeSet attrs) {
        this(context, attrs, 0);
    }

    public TabThumbnailView(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);

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
    public void setImageIcon(@Nullable Icon icon) {
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
    public void setImageURI(@Nullable Uri uri) {
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
        assumeNonNull(mRadii);
        mPath.addRoundRect(mRectF, mRadii, Path.Direction.CW);
        canvas.save();
        canvas.clipPath(mPath);
        super.onDraw(canvas);
        canvas.restore();
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
                                    getContext(), R.drawable.empty_thumbnail_background);
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
     *
     * @param isIncognito Whether the thumbnail is on an incognito tab.
     * @param isSelected Whether the thumbnail is on a selected tab.
     */
    public void updateThumbnailPlaceholder(
            boolean isIncognito, boolean isSelected, @Nullable @TabGroupColorId Integer colorId) {
        // Step 1: Background color.
        mBackgroundDrawable.setColor(
                TabCardThemeUtil.getMiniThumbnailPlaceholderColor(
                        getContext(), isIncognito, isSelected, colorId));
        final int newColor =
                TabCardThemeUtil.getCardViewBackgroundColor(
                        getContext(), isIncognito, isSelected, colorId);
        mPaint.setColor(newColor);

        // Step 2: Placeholder icon.
        // Make property changes outside the flag intentionally in the event the flag flips status
        // these will have no material effect on the UI and are safe.
        mIconColor = newColor;
        if (mIconDrawable != null) {
            setColorFilter(mIconColor, PorterDuff.Mode.SRC_IN);
        }
    }

    /**
     * Sets the rounded corner radii.
     *
     * @param cornerRadiusTopStart top start corner radius.
     * @param cornerRadiusTopEnd top end corner radius.
     * @param cornerRadiusBottomStart bottom start corner radius.
     * @param cornerRadiusBottomEnd bottom end corner radius.
     */
    @EnsuresNonNull("mRadii")
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

            final int vectorEdgeHeight = mIconDrawable.getIntrinsicHeight();
            final int vectorEdgeWidth = mIconDrawable.getIntrinsicWidth();

            final float effectiveWidth = WIDTH_PERCENTAGE * width;
            final float effectiveHeight = HEIGHT_PERCENTAGE * height;

            final float scaleX = effectiveWidth / vectorEdgeWidth;
            final float scaleY = effectiveHeight / vectorEdgeHeight;

            mIconMatrix.reset();
            mIconMatrix.postScale(scaleX, scaleY);

            // Center and offset vertically by sVerticalOffsetPx to account for optical illusion of
            // centering.
            assumeNonNull(sVerticalOffsetPx);
            mIconMatrix.postTranslate(
                    (width - effectiveWidth) / 2f,
                    (height - effectiveHeight) / 2f - sVerticalOffsetPx);
            setImageMatrix(mIconMatrix);
        }
    }

    public @Nullable VectorDrawable getIconDrawableForTesting() {
        return mIconDrawable;
    }
}
