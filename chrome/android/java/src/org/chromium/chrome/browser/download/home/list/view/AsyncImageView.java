// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list.view;

import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.chrome.download.R;

/**
 * Helper class to handle asynchronously loading an image and displaying it when ready.  This class
 * supports both a 'waiting' drawable and an 'unavailable' drawable that will be used in the
 * foreground when the async image isn't present yet.
 */
public class AsyncImageView extends ForegroundRoundedCornerImageView {
    /** An interface that provides a way for this class to query for a {@link Drawable}. */
    @FunctionalInterface
    public interface Factory {
        /**
         * Called by {@link AsyncImageView} to start the process of asynchronously loading a
         * {@link Drawable}.
         *
         * @param consumer The {@link Callback} to notify with the result.
         * @param widthPx  The desired width of the {@link Drawable} if applicable (not required to
         * match).
         * @param heightPx The desired height of the {@link Drawable} if applicable (not required to
         * match).
         * @return         A {@link Runnable} that can be triggered to cancel the outstanding
         * request.
         */
        Runnable get(Callback<Drawable> consumer, int widthPx, int heightPx);
    }

    /** Provides the callers an opportunity to override the image size before it is drawn. */
    public interface ImageResizer {
        /**
         * Called by the {@link AsyncImageView} before drawing to the screen.
         * @param drawable The {@link Drawable} to be drawn.
         */
        void maybeResizeImage(Drawable drawable);
    }

    private Drawable mUnavailableDrawable;
    private Drawable mWaitingDrawable;

    private Factory mFactory;
    private ImageResizer mImageResizer;

    private Runnable mCancelable;
    private boolean mWaitingForResponse;

    private @Nullable Object mIdentifier;

    /** Creates an {@link AsyncImageDrawable instance. */
    public AsyncImageView(Context context) {
        this(context, null, 0);
    }

    /** Creates an {@link AsyncImageDrawable instance. */
    public AsyncImageView(Context context, AttributeSet attrs) {
        this(context, attrs, 0);
    }

    /** Creates an {@link AsyncImageDrawable instance. */
    public AsyncImageView(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);

        TypedArray types = attrs == null
                ? null
                : context.obtainStyledAttributes(attrs, R.styleable.AsyncImageView, 0, 0);

        mUnavailableDrawable = AutoAnimatorDrawable.wrap(
                UiUtils.getDrawable(context, types, R.styleable.AsyncImageView_unavailableSrc));
        mWaitingDrawable = AutoAnimatorDrawable.wrap(
                UiUtils.getDrawable(context, types, R.styleable.AsyncImageView_waitingSrc));

        if (types != null) types.recycle();
    }

    /**
     * Starts loading a {@link Drawable} from {@code factory}.  This will automatically clear out
     * any outstanding request state and start a new one.
     *
     * @param factory    The {@link Factory} to use that will provide the {@link Drawable}.
     * @param identifier An identification for this particular request. Subsequent calls with the
     *                   same {@link Object} will be ignored until either {@code null} or a
     *                   different {@link Object} are passed in.  This lets us ignore redundant
     *                   calls.
     */
    public void setAsyncImageDrawable(Factory factory, @Nullable Object identifier) {
        if (mIdentifier != null && identifier != null && mIdentifier.equals(identifier)) return;

        // This will clear out any outstanding request.
        setImageDrawable(null);
        setForegroundDrawableCompat(mWaitingDrawable);

        mIdentifier = identifier;
        mFactory = factory;
        retrieveDrawableIfNeeded();
    }

    /**
     * @param unavailableDrawable Sets the {@link Drawable} to use when there is no thumbnail
     *                            available.
     */
    public void setUnavailableDrawable(Drawable unavailableDrawable) {
        boolean showUnavailable =
                getForegroundDrawableCompat() == mUnavailableDrawable && !mWaitingForResponse;
        mUnavailableDrawable = AutoAnimatorDrawable.wrap(unavailableDrawable);
        if (showUnavailable) setForegroundDrawableCompat(mUnavailableDrawable);
    }

    /**
     * @param waitingDrawable Sets the {@link Drawable} to use when waiting for an outstanding
     *                        asynchronous thumbnail request.
     */
    public void setWaitingDrawable(Drawable waitingDrawable) {
        mWaitingDrawable = AutoAnimatorDrawable.wrap(waitingDrawable);
        if (mWaitingForResponse) setForegroundDrawableCompat(mWaitingDrawable);
    }

    /**
     * @param resizer Sets a {@link ImageResizer} to use when drawing the image to the screen.
     */
    public void setImageResizer(ImageResizer resizer) {
        mImageResizer = resizer;
    }

    // RoundedCornerImageView implementation.
    @Override
    public void setImageDrawable(Drawable drawable) {
        // If we had an outstanding async request, cancel it because we're now setting the drawable
        // to something else.
        cancelPreviousDrawableRequest();

        if (mImageResizer != null) mImageResizer.maybeResizeImage(drawable);
        setForegroundDrawableCompat(null);
        super.setImageDrawable(drawable);
    }

    // View implementation.
    @Override
    public void onLayout(boolean changed, int left, int top, int right, int bottom) {
        super.onLayout(changed, left, top, right, bottom);

        retrieveDrawableIfNeeded();
    }

    @Override
    protected void onSizeChanged(int width, int height, int oldWidth, int oldHeight) {
        super.onSizeChanged(width, height, oldWidth, oldHeight);
        if (width == oldWidth && height == oldHeight) return;
        if (mImageResizer != null) mImageResizer.maybeResizeImage(getDrawable());
    }

    private void setAsyncImageDrawableResponse(Drawable drawable, Object identifier) {
        // If we ended up swapping out the identifier and somehow this request didn't cancel ignore
        // the response.  This does a direct == comparison instead of .equals() because any new
        // request should have canceled this one (we'll leave null alone though).
        if (mIdentifier != identifier || !mWaitingForResponse) return;

        mCancelable = null;
        mWaitingForResponse = false;
        setImageDrawable(drawable);

        // Restore the identifier after calling setImageDrawable(), which will erase it.
        mIdentifier = identifier;

        setForegroundDrawableCompat(drawable == null ? mUnavailableDrawable : null);
    }

    private void cancelPreviousDrawableRequest() {
        mFactory = null;
        mIdentifier = null;

        if (mWaitingForResponse) {
            if (mCancelable != null) mCancelable.run();
            mCancelable = null;
            mWaitingForResponse = false;
        }
    }

    private void retrieveDrawableIfNeeded() {
        // If width or height are not valid, don't start to retrieve the drawable since the
        // thumbnail may be scaled down to 0.
        if (getWidth() <= 0 || getHeight() <= 0) return;
        if (mFactory == null) return;

        // Start to retrieve the drawable.
        mWaitingForResponse = true;

        Object localIdentifier = mIdentifier;
        mCancelable = mFactory.get(drawable
                -> setAsyncImageDrawableResponse(drawable, localIdentifier),
                getWidth(), getHeight());

        // If setAsyncImageDrawableResponse is called synchronously, clear mCancelable.
        if (!mWaitingForResponse) mCancelable = null;

        mFactory = null;
    }
}
