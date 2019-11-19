// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Matrix;
import android.graphics.Paint;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffColorFilter;
import android.graphics.Shader;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;
import android.view.View;

import androidx.annotation.ColorInt;
import androidx.annotation.Nullable;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.favicon.IconType;
import org.chromium.chrome.browser.favicon.LargeIconBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.widget.RoundedIconGenerator;
import org.chromium.ui.modelutil.PropertyModel;

class RevampedContextMenuHeaderMediator implements View.OnClickListener {
    private PropertyModel mModel;

    private Context mContext;
    private String mPlainUrl;

    RevampedContextMenuHeaderMediator(
            Context context, PropertyModel model, ContextMenuParams params) {
        mContext = context;
        mPlainUrl = params.getUrl();
        mModel = model;
        mModel.set(RevampedContextMenuHeaderProperties.TITLE_AND_URL_CLICK_LISTENER, this);

        if (!params.isImage() && !params.isVideo()) {
            LargeIconBridge iconBridge = new LargeIconBridge(Profile.getLastUsedProfile());
            iconBridge.getLargeIconForUrl(mPlainUrl,
                    context.getResources().getDimensionPixelSize(R.dimen.default_favicon_min_size),
                    this::onFaviconAvailable);
        } else if (params.isVideo()) {
            setVideoIcon();
        }
    }

    /**
     * This is called when the thumbnail is fetched and ready to display.
     * @param thumbnail The bitmap received that will be displayed as the header image.
     */
    void onImageThumbnailRetrieved(Bitmap thumbnail) {
        RecordHistogram.recordBooleanHistogram("ContextMenu.ThumbnailFetched", thumbnail != null);
        if (thumbnail != null) {
            setHeaderImage(getImageWithCheckerBackground(mContext.getResources(), thumbnail), true);
        }
        // TODO(sinansahin): Handle the case where the retrieval of the thumbnail fails.
    }

    /**
     * See {@link org.chromium.chrome.browser.favicon.LargeIconBridge#getLargeIconForUrl}
     */
    private void onFaviconAvailable(@Nullable Bitmap icon, @ColorInt int fallbackColor,
            boolean isColorDefault, @IconType int iconType) {
        // If we didn't get a favicon, generate a monogram instead
        if (icon == null) {
            final RoundedIconGenerator iconGenerator = createRoundedIconGenerator(fallbackColor);
            icon = iconGenerator.generateIconForUrl(mPlainUrl);
            // generateIconForUrl might return null if the URL is empty or the domain cannot be
            // resolved. See https://crbug.com/987101
            // TODO(sinansahin): Handle the case where generating an icon fails.
            if (icon == null) {
                return;
            }
        }

        final int size = mContext.getResources().getDimensionPixelSize(
                R.dimen.revamped_context_menu_header_monogram_size);

        icon = Bitmap.createScaledBitmap(icon, size, size, true);

        setHeaderImage(icon, false);
    }

    /**
     * This is called when the url text is clicked. So, we can expand or shrink the url here.
     * @param v The url text view.
     */
    @Override
    public void onClick(View v) {
        RecordHistogram.recordBooleanHistogram("ContextMenu.URLClicked", true);
        if (mModel.get(RevampedContextMenuHeaderProperties.URL_MAX_LINES) == Integer.MAX_VALUE) {
            // URL and title should both be expanded.
            assert mModel.get(RevampedContextMenuHeaderProperties.TITLE_MAX_LINES)
                    == Integer.MAX_VALUE;

            final boolean isTitleEmpty =
                    TextUtils.isEmpty(mModel.get(RevampedContextMenuHeaderProperties.TITLE));
            mModel.set(RevampedContextMenuHeaderProperties.URL_MAX_LINES, isTitleEmpty ? 2 : 1);
            final boolean isUrlEmpty =
                    TextUtils.isEmpty(mModel.get(RevampedContextMenuHeaderProperties.URL));
            mModel.set(RevampedContextMenuHeaderProperties.TITLE_MAX_LINES, isUrlEmpty ? 2 : 1);
        } else {
            mModel.set(RevampedContextMenuHeaderProperties.URL_MAX_LINES, Integer.MAX_VALUE);
            mModel.set(RevampedContextMenuHeaderProperties.TITLE_MAX_LINES, Integer.MAX_VALUE);
        }
    }

    /**
     * This adds a checkerboard style background to the image.
     * It is useful for the transparent PNGs.
     * @return The given image with the checkerboard pattern in the background.
     */
    private static Bitmap getImageWithCheckerBackground(Resources res, Bitmap image) {
        // 1. Create a bitmap for the checkerboard pattern.
        Drawable drawable =
                ApiCompatibilityUtils.getDrawable(res, R.drawable.checkerboard_background);
        Bitmap tileBitmap = Bitmap.createBitmap(drawable.getIntrinsicWidth(),
                drawable.getIntrinsicHeight(), Bitmap.Config.ARGB_8888);
        Canvas tileCanvas = new Canvas(tileBitmap);
        drawable.setBounds(0, 0, drawable.getIntrinsicWidth(), drawable.getIntrinsicHeight());
        drawable.draw(tileCanvas);

        // 2. Create a BitmapDrawable using the checkerboard pattern bitmap.
        BitmapDrawable bitmapDrawable = new BitmapDrawable(res, tileBitmap);
        bitmapDrawable.setTileModeXY(Shader.TileMode.REPEAT, Shader.TileMode.REPEAT);
        bitmapDrawable.setBounds(0, 0, image.getWidth(), image.getHeight());

        // 3. Create a bitmap-backed canvas for the final image.
        Bitmap bitmap =
                Bitmap.createBitmap(image.getWidth(), image.getHeight(), Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(bitmap);

        // 4. Paint the checkerboard background into the final canvas
        bitmapDrawable.draw(canvas);

        // 5. Draw the image on top.
        Paint paint = new Paint();
        paint.setAntiAlias(true);
        canvas.drawBitmap(image, new Matrix(), paint);

        return bitmap;
    }

    private void setVideoIcon() {
        BitmapFactory.Options options = new BitmapFactory.Options();
        options.inMutable = true;
        Bitmap bitmap = BitmapFactory.decodeResource(
                mContext.getResources(), R.drawable.ic_videocam_white_24dp, options);
        Canvas canvas = new Canvas(bitmap);
        Paint paint = new Paint();
        paint.setColorFilter(new PorterDuffColorFilter(
                ApiCompatibilityUtils.getColor(mContext.getResources(), R.color.default_icon_color),
                PorterDuff.Mode.SRC_IN));
        canvas.drawBitmap(bitmap, new Matrix(), paint);
        setHeaderImage(bitmap, false);
    }

    private void setHeaderImage(Bitmap bitmap, boolean isThumbnail) {
        if (isThumbnail) {
            mModel.set(RevampedContextMenuHeaderProperties.IMAGE, bitmap);
            return;
        }

        mModel.set(RevampedContextMenuHeaderProperties.CIRCLE_BG_VISIBLE, true);
        mModel.set(RevampedContextMenuHeaderProperties.IMAGE, bitmap);
    }

    private RoundedIconGenerator createRoundedIconGenerator(@ColorInt int iconColor) {
        final Resources resources = mContext.getResources();
        final int iconSize =
                resources.getDimensionPixelSize(R.dimen.revamped_context_menu_header_monogram_size);
        final int cornerRadius = iconSize / 2;
        final int textSize = resources.getDimensionPixelSize(
                R.dimen.revamped_context_menu_header_monogram_text_size);

        return new RoundedIconGenerator(iconSize, iconSize, cornerRadius, iconColor, textSize);
    }
}
