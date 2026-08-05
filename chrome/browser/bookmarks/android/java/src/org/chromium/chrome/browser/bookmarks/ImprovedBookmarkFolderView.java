// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.util.Pair;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.ColorInt;
import androidx.annotation.StyleRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.RoundedCornerImageView;

/**
 * Draws the image at the start of a bookmark folder row. This may contains elements from the
 * folder's children bookmarks, such as thumbnail or count.
 */
@NullMarked
public class ImprovedBookmarkFolderView extends FrameLayout {
    private final int mOuterRadius;
    private final int mInnerRadius;

    private RoundedCornerImageView mPrimaryImage;
    private RoundedCornerImageView mNoImagePlaceholder;
    private ImageView mStartIcon;
    private ViewGroup mSecondaryImageContainer;
    private RoundedCornerImageView mSecondaryImage;

    private View mChildCountBackgroundOneImage;
    private RoundedCornerImageView mChildCountBackgroundOneImageTop;
    private RoundedCornerImageView mChildCountBackgroundOneImageBot;
    private RoundedCornerImageView mChildCountBackgroundTwoImages;
    private RoundedCornerImageView mChildCountContainer;
    private TextView mChildCount;

    /** Constructor for inflating from XML. */
    public ImprovedBookmarkFolderView(Context context, AttributeSet attrs) {
        super(context, attrs);

        Resources resources = context.getResources();
        mOuterRadius =
                resources.getDimensionPixelSize(R.dimen.improved_bookmark_row_outer_corner_radius);
        mInnerRadius =
                resources.getDimensionPixelSize(R.dimen.improved_bookmark_row_inner_corner_radius);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        final Context context = getContext();
        Resources resources = context.getResources();
        int outerRadius =
                resources.getDimensionPixelSize(R.dimen.improved_bookmark_row_outer_corner_radius);
        final @ColorInt int surfaceColor = SemanticColorUtils.getColorSurface(context);
        final @ColorInt int colorSurfaceContainerLow =
                SemanticColorUtils.getColorSurfaceContainerLow(context);

        mPrimaryImage = findViewById(R.id.primary_image);
        mPrimaryImage.setRoundedCorners(outerRadius, outerRadius, outerRadius, outerRadius);

        mNoImagePlaceholder = findViewById(R.id.no_image_placeholder_background);
        mNoImagePlaceholder.setRoundedCorners(outerRadius, outerRadius, outerRadius, outerRadius);

        mStartIcon = findViewById(R.id.no_image_placeholder_image);

        mSecondaryImage = findViewById(R.id.secondary_image);
        mSecondaryImage.setRoundedCorners(0, outerRadius, 0, 0);

        mSecondaryImageContainer = findViewById(R.id.secondary_image_container);
        mSecondaryImageContainer.setBackgroundColor(surfaceColor);

        // Setup the background for the child count view when there's one image present.
        mChildCountBackgroundOneImage = findViewById(R.id.child_count_background_one_image);
        mChildCountBackgroundOneImageTop = findViewById(R.id.child_count_background_one_image_top);
        mChildCountBackgroundOneImageTop.setRoundedCorners(mInnerRadius, 0, 0, 0);
        mChildCountBackgroundOneImageTop.setRoundedFillColor(colorSurfaceContainerLow);
        mChildCountBackgroundOneImageBot = findViewById(R.id.child_count_background_one_image_bot);
        mChildCountBackgroundOneImageBot.setRoundedCorners(0, 0, 0, mOuterRadius);
        mChildCountBackgroundOneImageBot.setRoundedFillColor(colorSurfaceContainerLow);

        // Setup the background for the child count view when there's two images present.
        mChildCountBackgroundTwoImages = findViewById(R.id.child_count_background_two_images);
        mChildCountBackgroundTwoImages.setRoundedCorners(0, 0, 0, mOuterRadius);
        mChildCountBackgroundTwoImages.setRoundedFillColor(colorSurfaceContainerLow);

        // The container which separates the child text from the images.
        mChildCountContainer = findViewById(R.id.child_count_container);
        mChildCountContainer.setRoundedFillColor(surfaceColor);

        mChildCount = findViewById(R.id.child_count_text);
    }

    void setStartIconDrawable(Drawable drawable) {
        mStartIcon.setImageDrawable(drawable);
    }

    void setStartIconTint(ColorStateList tint) {
        mStartIcon.setImageTintList(tint);
    }

    void setStartAreaBackgroundColor(@ColorInt int color) {
        mNoImagePlaceholder.setRoundedFillColor(color);
    }

    void setStartImageDrawablePair(Pair<Drawable, Drawable> drawablePair) {
        setStartImageDrawables(drawablePair.first, drawablePair.second);
    }

    void setStartImageDrawables(
            @Nullable Drawable primaryDrawable, @Nullable Drawable secondaryDrawable) {
        mNoImagePlaceholder.setVisibility(View.GONE);
        mStartIcon.setVisibility(View.GONE);
        mPrimaryImage.setVisibility(View.GONE);
        mSecondaryImageContainer.setVisibility(View.GONE);
        mChildCountBackgroundOneImage.setVisibility(View.GONE);
        mChildCountBackgroundTwoImages.setVisibility(View.GONE);
        mChildCountContainer.setVisibility(View.GONE);

        if (primaryDrawable == null && secondaryDrawable == null) {
            // Placeholder folder image case.
            mNoImagePlaceholder.setVisibility(View.VISIBLE);
            mStartIcon.setVisibility(View.VISIBLE);
        } else if (primaryDrawable != null && secondaryDrawable == null) {
            // 1-image case.
            mPrimaryImage.setImageDrawable(primaryDrawable);

            mPrimaryImage.setVisibility(View.VISIBLE);
            mChildCountBackgroundOneImage.setVisibility(View.VISIBLE);
            updateChildCountContainer(1);
        } else {
            // 2-image case.
            mPrimaryImage.setImageDrawable(primaryDrawable);
            mSecondaryImage.setImageDrawable(secondaryDrawable);

            mPrimaryImage.setVisibility(View.VISIBLE);
            mSecondaryImageContainer.setVisibility(View.VISIBLE);
            mChildCountBackgroundTwoImages.setVisibility(View.VISIBLE);
            updateChildCountContainer(2);
        }
    }

    @SuppressWarnings("SetTextI18n")
    void setChildCount(int count) {
        mChildCount.setText(Integer.toString(count));
    }

    void setChildCountStyle(@StyleRes int styleRes) {
        mChildCount.setTextAppearance(styleRes);
    }

    private void updateChildCountContainer(int numberOfImages) {
        mChildCountContainer.setVisibility(numberOfImages == 0 ? View.GONE : View.VISIBLE);
        if (numberOfImages == 1) {
            mChildCountContainer.setRoundedCorners(mInnerRadius, 0, 0, 0);
        } else if (numberOfImages == 2) {
            mChildCountContainer.setRoundedCorners(0, 0, 0, mOuterRadius);
        }
    }
}
