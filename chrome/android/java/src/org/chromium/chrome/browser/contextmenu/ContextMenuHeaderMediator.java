// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Matrix;
import android.graphics.Paint;
import android.graphics.PorterDuff;
import android.graphics.Shader;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;
import android.view.View;

import androidx.annotation.ColorInt;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.RoundedIconGenerator;
import org.chromium.components.embedder_support.contextmenu.ContextMenuNativeDelegate;
import org.chromium.components.embedder_support.contextmenu.ContextMenuParams;
import org.chromium.components.favicon.IconType;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

@NullMarked
class ContextMenuHeaderMediator implements View.OnClickListener {
    private final PropertyModel mModel;

    private final Context mContext;
    private final GURL mPlainUrl;

    ContextMenuHeaderMediator(
            Context context,
            PropertyModel model,
            ContextMenuParams params,
            Profile profile,
            ContextMenuNativeDelegate nativeDelegate) {
        mContext = context;
        mPlainUrl = params.getUrl();
        mModel = model;
        mModel.set(ContextMenuHeaderProperties.TITLE_AND_URL_CLICK_LISTENER, this);

        if (params.isImage()) {
            final Resources res = mContext.getResources();
            final int imageMaxSize =
                    res.getDimensionPixelSize(R.dimen.context_menu_header_image_max_size);
            nativeDelegate.retrieveImageForContextMenu(
                    imageMaxSize, imageMaxSize, this::onImageThumbnailRetrieved);
        } else if (!params.isImage() && !params.isVideo()) {
            LargeIconBridge iconBridge = new LargeIconBridge(profile);
            iconBridge.getLargeIconForUrl(
                    mPlainUrl,
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
        if (thumbnail != null) {
            setHeaderImage(getImageWithCheckerBackground(mContext.getResources(), thumbnail), true);
        }
        // TODO(sinansahin): Handle the case where the retrieval of the thumbnail fails.
    }

    /** See {@link org.chromium.components.favicon.LargeIconBridge#getLargeIconForUrl} */
    private void onFaviconAvailable(
            @Nullable Bitmap icon,
            @ColorInt int fallbackColor,
            boolean isColorDefault,
            @IconType int iconType) {
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

        final int size = mModel.get(ContextMenuHeaderProperties.MONOGRAM_SIZE_PIXEL);

        icon = Bitmap.createScaledBitmap(icon, size, size, true);

        setHeaderImage(icon, false);
    }

    /**
     * This is called when the header is clicked. It toggles between an expanded state (all text
     * fields show their full content) and a collapsed state (text fields are truncated to fit
     * within a set number of lines).
     *
     * @param v The url text view.
     */
    @Override
    public void onClick(View v) {
        // 1. Determine the current state and what properties are visible.
        final boolean isCurrentlyExpanded =
                mModel.get(ContextMenuHeaderProperties.URL_MAX_LINES) == Integer.MAX_VALUE;
        final boolean isTitleEmpty = TextUtils.isEmpty(mModel.get(ListMenuItemProperties.TITLE));
        final boolean isUrlEmpty = TextUtils.isEmpty(mModel.get(ContextMenuHeaderProperties.URL));
        final boolean isSecondaryUrlPresent =
                !TextUtils.isEmpty(mModel.get(ContextMenuHeaderProperties.SECONDARY_URL));
        final boolean isTertiaryUrlPresent =
                !TextUtils.isEmpty(mModel.get(ContextMenuHeaderProperties.TERTIARY_URL));

        // 2. Handle the "expand" action. This is the simple case.
        if (!isCurrentlyExpanded) {
            mModel.set(ContextMenuHeaderProperties.TITLE_MAX_LINES, Integer.MAX_VALUE);
            mModel.set(ContextMenuHeaderProperties.URL_MAX_LINES, Integer.MAX_VALUE);
            if (isSecondaryUrlPresent) {
                mModel.set(ContextMenuHeaderProperties.SECONDARY_URL_MAX_LINES, Integer.MAX_VALUE);
            }
            if (isTertiaryUrlPresent) {
                mModel.set(ContextMenuHeaderProperties.TERTIARY_URL_MAX_LINES, Integer.MAX_VALUE);
            }
            mModel.set(ContextMenuHeaderProperties.IS_EXPANDED, true);
            return;
        }

        // 3. Handle the "collapse" action. This logic distributes a total of 2 or 3 lines.
        mModel.set(ContextMenuHeaderProperties.IS_EXPANDED, false);

        // Case A: No secondary/tertiary URLs. Distribute 2 lines between Title and URL.
        if (!isSecondaryUrlPresent) {
            mModel.set(ContextMenuHeaderProperties.URL_MAX_LINES, isTitleEmpty ? 2 : 1);
            mModel.set(ContextMenuHeaderProperties.TITLE_MAX_LINES, isUrlEmpty ? 2 : 1);
            // Reset secondary/tertiary lines for consistency, though they are not visible.
            mModel.set(ContextMenuHeaderProperties.SECONDARY_URL_MAX_LINES, 1);
            mModel.set(ContextMenuHeaderProperties.TERTIARY_URL_MAX_LINES, 1);
            return;
        }

        // Case B: Secondary URL is present. Distribute 3 lines among all visible properties.
        int visibleProperties = 0;
        if (!isTitleEmpty) visibleProperties++;
        if (!isUrlEmpty) visibleProperties++;
        visibleProperties++; // Secondary is guaranteed present here.
        if (isTertiaryUrlPresent) visibleProperties++;

        int secondaryLines = 1;
        if (visibleProperties == 1) { // Only secondary URL is visible
            secondaryLines = 3;
        } else if (visibleProperties == 2) { // Secondary + one other property
            secondaryLines = 2;
        }
        // If 3 or more properties are visible, they all get 1 line.

        mModel.set(ContextMenuHeaderProperties.TITLE_MAX_LINES, 1);
        mModel.set(ContextMenuHeaderProperties.URL_MAX_LINES, 1);
        mModel.set(ContextMenuHeaderProperties.SECONDARY_URL_MAX_LINES, secondaryLines);
        mModel.set(ContextMenuHeaderProperties.TERTIARY_URL_MAX_LINES, 1);
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
        Bitmap tileBitmap =
                Bitmap.createBitmap(
                        drawable.getIntrinsicWidth(),
                        drawable.getIntrinsicHeight(),
                        Bitmap.Config.ARGB_8888);
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
        Drawable drawable =
                ApiCompatibilityUtils.getDrawable(
                        mContext.getResources(), R.drawable.gm_filled_videocam_24);
        drawable.setColorFilter(
                SemanticColorUtils.getDefaultIconColor(mContext), PorterDuff.Mode.SRC_IN);
        Bitmap bitmap =
                Bitmap.createBitmap(
                        drawable.getIntrinsicWidth(),
                        drawable.getIntrinsicHeight(),
                        Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(bitmap);
        drawable.setBounds(0, 0, canvas.getWidth(), canvas.getHeight());
        drawable.draw(canvas);
        setHeaderImage(bitmap, false);
    }

    private void setHeaderImage(Bitmap bitmap, boolean isThumbnail) {
        if (isThumbnail) {
            mModel.set(ContextMenuHeaderProperties.IMAGE, bitmap);
            return;
        }

        mModel.set(ContextMenuHeaderProperties.CIRCLE_BG_VISIBLE, true);
        mModel.set(ContextMenuHeaderProperties.IMAGE, bitmap);
    }

    private RoundedIconGenerator createRoundedIconGenerator(@ColorInt int iconColor) {
        final Resources resources = mContext.getResources();
        final int iconSize =
                resources.getDimensionPixelSize(R.dimen.context_menu_header_monogram_size);
        final int cornerRadius = iconSize / 2;
        final int textSize =
                resources.getDimensionPixelSize(R.dimen.context_menu_header_monogram_text_size);

        return new RoundedIconGenerator(iconSize, iconSize, cornerRadius, iconColor, textSize);
    }
}
