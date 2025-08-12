// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.util.Pair;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageView;

import androidx.annotation.ColorInt;
import androidx.core.content.ContextCompat;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.RoundedCornerOutlineProvider;

/** Draws the icon for the Theme Collections section in the NTP Appearance bottom sheet. */
@NullMarked
public class NtpThemeListThemeCollectionItemIconView extends FrameLayout {
    private final RoundedCornerOutlineProvider mPrimaryImageOutline;
    private final RoundedCornerOutlineProvider mSecondaryImageOutline;
    private final RoundedCornerOutlineProvider mBottomRightOutline;

    private View mNoImagePlaceholder;
    private ImageView mPrimaryImage;
    private ViewGroup mSecondaryImageContainer;
    private ImageView mSecondaryImage;
    private View mBottomRightContainer;
    private View mBottomRightBackground;
    private ImageView mBottomRightIcon;

    /** Constructor for inflating from XML. */
    public NtpThemeListThemeCollectionItemIconView(Context context, AttributeSet attrs) {
        super(context, attrs);

        Resources resources = context.getResources();
        int outerRadius =
                resources.getDimensionPixelSize(
                        R.dimen.ntp_customization_theme_collections_icon_outer_radius);

        mPrimaryImageOutline = new RoundedCornerOutlineProvider(outerRadius);

        mSecondaryImageOutline = new RoundedCornerOutlineProvider(outerRadius);
        mSecondaryImageOutline.setRoundingEdges(false, true, true, false);

        mBottomRightOutline = new RoundedCornerOutlineProvider(outerRadius);
        mBottomRightOutline.setRoundingEdges(false, false, true, true);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        final Context context = getContext();
        final @ColorInt int surfaceColor =
                ContextCompat.getColor(context, R.color.sheet_on_scrim_bg_color);
        final @ColorInt int colorSurfaceContainer =
                SemanticColorUtils.getColorSurfaceContainerHigh(context);

        mNoImagePlaceholder = findViewById(R.id.no_image_placeholder_background);
        mNoImagePlaceholder.setOutlineProvider(mPrimaryImageOutline);
        mNoImagePlaceholder.setClipToOutline(true);

        mPrimaryImage = findViewById(R.id.primary_image);
        mPrimaryImage.setOutlineProvider(mPrimaryImageOutline);
        mPrimaryImage.setClipToOutline(true);

        mSecondaryImageContainer = findViewById(R.id.secondary_image_container);
        mSecondaryImageContainer.setBackgroundColor(surfaceColor);

        mSecondaryImage = findViewById(R.id.secondary_image);
        mSecondaryImage.setOutlineProvider(mSecondaryImageOutline);
        mSecondaryImage.setClipToOutline(true);

        mBottomRightContainer = findViewById(R.id.bottom_right_container);
        mBottomRightContainer.setBackgroundColor(surfaceColor);

        mBottomRightBackground = findViewById(R.id.bottom_right_background);
        mBottomRightBackground.setBackgroundColor(colorSurfaceContainer);
        mBottomRightBackground.setOutlineProvider(mBottomRightOutline);
        mBottomRightBackground.setClipToOutline(true);

        mBottomRightIcon = findViewById(R.id.bottom_right_icon);
    }

    void setImageDrawablePair(Pair<Drawable, Drawable> drawablePair) {
        setImageDrawables(drawablePair.first, drawablePair.second);
    }

    void setImageDrawables(
            @Nullable Drawable primaryDrawable, @Nullable Drawable secondaryDrawable) {
        mNoImagePlaceholder.setVisibility(View.GONE);
        mPrimaryImage.setVisibility(View.GONE);
        mSecondaryImageContainer.setVisibility(View.GONE);
        mBottomRightContainer.setVisibility(View.GONE);
        mBottomRightBackground.setVisibility(View.GONE);
        mBottomRightIcon.setVisibility(View.GONE);

        if (primaryDrawable == null && secondaryDrawable == null) {
            mNoImagePlaceholder.setVisibility(View.VISIBLE);
        } else {
            mPrimaryImage.setImageDrawable(primaryDrawable);
            mSecondaryImage.setImageDrawable(secondaryDrawable);

            mPrimaryImage.setVisibility(View.VISIBLE);
            mSecondaryImageContainer.setVisibility(View.VISIBLE);
            mBottomRightContainer.setVisibility(View.VISIBLE);
            mBottomRightBackground.setVisibility(View.VISIBLE);
            mBottomRightIcon.setVisibility(View.VISIBLE);
        }
    }
}
