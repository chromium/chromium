// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.drawable.ColorDrawable;
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

    private ImageView mPrimaryImage;
    private ImageView mSecondaryImage;

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
                ContextCompat.getColor(context, R.color.bottom_sheet_bg_color);
        final @ColorInt int colorSurfaceContainer =
                SemanticColorUtils.getColorSurfaceContainerHigh(context);

        mPrimaryImage = findViewById(R.id.primary_image);
        mPrimaryImage.setOutlineProvider(mPrimaryImageOutline);
        mPrimaryImage.setClipToOutline(true);

        ViewGroup secondaryImageContainer = findViewById(R.id.secondary_image_container);
        secondaryImageContainer.setBackgroundColor(surfaceColor);

        mSecondaryImage = findViewById(R.id.secondary_image);
        mSecondaryImage.setOutlineProvider(mSecondaryImageOutline);
        mSecondaryImage.setClipToOutline(true);

        View bottomRightContainer = findViewById(R.id.bottom_right_container);
        bottomRightContainer.setBackgroundColor(surfaceColor);

        View bottomRightBackground = findViewById(R.id.bottom_right_background);
        bottomRightBackground.setBackgroundColor(colorSurfaceContainer);
        bottomRightBackground.setOutlineProvider(mBottomRightOutline);
        bottomRightBackground.setClipToOutline(true);

        setImageDrawables(/* primaryDrawable= */ null, /* secondaryDrawable= */ null);
    }

    void setImageDrawablePair(Pair<Drawable, Drawable> drawablePair) {
        setImageDrawables(drawablePair.first, drawablePair.second);
    }

    void setImageDrawables(
            @Nullable Drawable primaryDrawable, @Nullable Drawable secondaryDrawable) {
        @ColorInt
        int colorSurfaceContainer = SemanticColorUtils.getColorSurfaceContainerHigh(getContext());

        if (primaryDrawable == null) {
            primaryDrawable = new ColorDrawable(colorSurfaceContainer);
        }
        mPrimaryImage.setImageDrawable(primaryDrawable);

        if (secondaryDrawable == null) {
            secondaryDrawable = new ColorDrawable(colorSurfaceContainer);
        }
        mSecondaryImage.setImageDrawable(secondaryDrawable);
    }
}
