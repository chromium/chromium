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
import androidx.annotation.Nullable;
import androidx.annotation.StyleRes;

import org.chromium.chrome.R;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.widget.RoundedCornerOutlineProvider;

/**
 * Draws the image at the start of a bookmark folder row. This may contains elements from the
 * folder's children bookmarks, such as thumbnail or count.
 */
public class ImprovedBookmarkFolderView extends FrameLayout {
    private final RoundedCornerOutlineProvider mPrimaryImageOutline;
    private final RoundedCornerOutlineProvider mSecondaryImageOutline;

    private final RoundedCornerOutlineProvider mChildTextBackgroundOutlineOneImageTop;
    private final RoundedCornerOutlineProvider mChildTextBackgroundOutlineOneImageBot;
    private final RoundedCornerOutlineProvider mChildTextContainerOutlineOneImage;
    private final RoundedCornerOutlineProvider mChildTextContainerOutlineTwoImages;

    private ImageView mPrimaryImage;
    private View mNoImagePlaceholder;
    private ImageView mStartIcon;
    private ViewGroup mSecondaryImageContainer;
    private ImageView mSecondaryImage;

    private View mChildCountBackgroundOneImage;
    private View mChildCountBackgroundTwoImages;
    private View mChildCountContainer;
    private TextView mChildCount;

    /** Constructor for inflating from XML. */
    public ImprovedBookmarkFolderView(Context context, AttributeSet attrs) {
        super(context, attrs);

        Resources resources = context.getResources();
        int outerRadius =
                resources.getDimensionPixelSize(R.dimen.improved_bookmark_row_outer_corner_radius);
        int innerRadius =
                resources.getDimensionPixelSize(R.dimen.improved_bookmark_row_inner_corner_radius);

        mPrimaryImageOutline = new RoundedCornerOutlineProvider(outerRadius);

        mSecondaryImageOutline = new RoundedCornerOutlineProvider(outerRadius);
        mSecondaryImageOutline.setRoundingEdges(false, true, true, false);

        mChildTextBackgroundOutlineOneImageTop = new RoundedCornerOutlineProvider(innerRadius);
        mChildTextBackgroundOutlineOneImageTop.setRoundingEdges(true, true, false, false);

        mChildTextBackgroundOutlineOneImageBot = new RoundedCornerOutlineProvider(outerRadius);
        mChildTextBackgroundOutlineOneImageBot.setRoundingEdges(false, false, true, true);

        mChildTextContainerOutlineOneImage = new RoundedCornerOutlineProvider(innerRadius);
        mChildTextContainerOutlineOneImage.setRoundingEdges(true, true, false, false);

        mChildTextContainerOutlineTwoImages = new RoundedCornerOutlineProvider(outerRadius);
        mChildTextContainerOutlineTwoImages.setRoundingEdges(false, false, true, true);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        final Context context = getContext();
        final @ColorInt int surface0 =
                ChromeColors.getSurfaceColor(context, R.dimen.default_elevation_0);
        final @ColorInt int surface1 =
                ChromeColors.getSurfaceColor(context, R.dimen.default_elevation_1);

        mPrimaryImage = findViewById(R.id.primary_image);
        mPrimaryImage.setOutlineProvider(mPrimaryImageOutline);
        mPrimaryImage.setClipToOutline(true);

        mNoImagePlaceholder = findViewById(R.id.no_image_placeholder_background);
        mNoImagePlaceholder.setOutlineProvider(mPrimaryImageOutline);
        mNoImagePlaceholder.setClipToOutline(true);

        mStartIcon = findViewById(R.id.no_image_placeholder_image);

        mSecondaryImage = findViewById(R.id.secondary_image);
        mSecondaryImage.setOutlineProvider(mSecondaryImageOutline);
        mSecondaryImage.setClipToOutline(true);

        mSecondaryImageContainer = findViewById(R.id.secondary_image_container);
        mSecondaryImageContainer.setBackgroundColor(surface0);

        // Setup the background for the child count view when there's one image present.
        mChildCountBackgroundOneImage = findViewById(R.id.child_count_background_one_image);
        View childCountBackgroundOneImageTop =
                findViewById(R.id.child_count_background_one_image_top);
        childCountBackgroundOneImageTop.setBackgroundColor(surface1);
        childCountBackgroundOneImageTop.setOutlineProvider(mChildTextBackgroundOutlineOneImageTop);
        childCountBackgroundOneImageTop.setClipToOutline(true);
        View childCountBackgroundOneImageBot =
                findViewById(R.id.child_count_background_one_image_bot);
        childCountBackgroundOneImageBot.setBackgroundColor(surface1);
        childCountBackgroundOneImageBot.setOutlineProvider(mChildTextBackgroundOutlineOneImageBot);
        childCountBackgroundOneImageBot.setClipToOutline(true);

        // Setup the background for the child count view when there's two images present.
        mChildCountBackgroundTwoImages = findViewById(R.id.child_count_background_two_images);
        mChildCountBackgroundTwoImages.setBackgroundColor(surface1);
        mChildCountBackgroundTwoImages.setOutlineProvider(mChildTextContainerOutlineTwoImages);
        mChildCountBackgroundTwoImages.setClipToOutline(true);

        // The container which separates the child text from the images.
        mChildCountContainer = findViewById(R.id.child_count_container);
        mChildCountContainer.setBackgroundColor(surface0);
        mChildCountContainer.setClipToOutline(true);

        mChildCount = findViewById(R.id.child_count_text);
    }

    void setStartIconDrawable(Drawable drawable) {
        mStartIcon.setImageDrawable(drawable);
    }

    void setStartIconTint(ColorStateList tint) {
        mStartIcon.setImageTintList(tint);
    }

    void setStartAreaBackgroundColor(@ColorInt int color) {
        mNoImagePlaceholder.setBackgroundColor(color);
    }

    void setStartImageDrawablePair(Pair<Drawable, Drawable> drawablePair) {
        setStartImageDrawables(drawablePair.first, drawablePair.second);
    }

    void setStartImageDrawables(
            @Nullable Drawable primaryDrawable, @Nullable Drawable secondaryDrawable) {
        mNoImagePlaceholder.setVisibility(View.GONE);
        mPrimaryImage.setVisibility(View.GONE);
        mSecondaryImageContainer.setVisibility(View.GONE);
        mChildCountBackgroundOneImage.setVisibility(View.GONE);
        mChildCountBackgroundTwoImages.setVisibility(View.GONE);
        mChildCountContainer.setVisibility(View.GONE);

        if (primaryDrawable == null && secondaryDrawable == null) {
            // Placeholder folder image case.
            mNoImagePlaceholder.setVisibility(View.VISIBLE);
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
            mChildCountContainer.setOutlineProvider(mChildTextContainerOutlineOneImage);
        } else if (numberOfImages == 2) {
            mChildCountContainer.setOutlineProvider(mChildTextContainerOutlineTwoImages);
        }
    }
}
