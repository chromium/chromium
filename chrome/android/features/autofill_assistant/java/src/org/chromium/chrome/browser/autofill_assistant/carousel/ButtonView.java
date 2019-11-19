// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.autofill_assistant.carousel;

import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.drawable.Drawable;
import android.support.v4.view.ViewCompat;
import android.util.AttributeSet;
import android.view.ContextThemeWrapper;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.DrawableRes;
import androidx.annotation.IdRes;
import androidx.annotation.Px;
import androidx.annotation.StyleRes;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.util.ViewUtils;
import org.chromium.ui.widget.ChromeImageView;
import org.chromium.ui.widget.RippleBackgroundHelper;

/**
 * The view responsible for displaying rounded buttons. This class is a fork of {@link ChipView}
 * that allows to remove the border of the button and have buttons with an icon and no text.
 */
public class ButtonView extends LinearLayout {
    /** An id to use for {@link #setIcon(int, boolean)} when there is no icon on the chip. */
    public static final int INVALID_ICON_ID = -1;

    private final RippleBackgroundHelper mRippleBackgroundHelper;
    private final TextView mPrimaryText;
    private final ChromeImageView mIcon;
    private final @IdRes int mSecondaryTextAppearanceId;

    private TextView mSecondaryText;

    /**
     * Constructor for inflating from XML.
     */
    public ButtonView(Context context, @StyleRes int chipStyle) {
        this(context, null, chipStyle);
    }

    /**
     * Constructor for inflating from XML.
     */
    public ButtonView(Context context, AttributeSet attrs) {
        this(context, attrs, R.style.SuggestionChipThemeOverlay);
    }

    private ButtonView(Context context, AttributeSet attrs, @StyleRes int themeOverlay) {
        super(new ContextThemeWrapper(context, themeOverlay), attrs, R.attr.chipStyle);

        final @Px int leadingElementPadding =
                getResources().getDimensionPixelSize(R.dimen.chip_element_leading_padding);

        TypedArray a = getContext().obtainStyledAttributes(
                attrs, R.styleable.ButtonView, R.attr.chipStyle, 0);
        int chipColorId =
                a.getResourceId(R.styleable.ButtonView_chipColor, R.color.chip_background_color);
        int rippleColorId =
                a.getResourceId(R.styleable.ButtonView_rippleColor, R.color.chip_ripple_color);
        int cornerRadius = a.getDimensionPixelSize(R.styleable.ButtonView_cornerRadius,
                getContext().getResources().getDimensionPixelSize(R.dimen.chip_corner_radius));
        int iconWidth = a.getDimensionPixelSize(R.styleable.ButtonView_iconWidth,
                getResources().getDimensionPixelSize(R.dimen.chip_icon_size));
        int iconHeight = a.getDimensionPixelSize(R.styleable.ButtonView_iconHeight,
                getResources().getDimensionPixelSize(R.dimen.chip_icon_size));
        int primaryTextAppearance = a.getResourceId(
                R.styleable.ButtonView_primaryTextAppearance, R.style.TextAppearance_ChipText);
        mSecondaryTextAppearanceId = a.getResourceId(
                R.styleable.ButtonView_secondaryTextAppearance, R.style.TextAppearance_ChipText);
        int borderWidth =
                a.getResourceId(R.styleable.ButtonView_chipBorderWidth, R.dimen.chip_border_width);
        int verticalInset = a.getDimensionPixelSize(R.styleable.ButtonView_verticalInset,
                getResources().getDimensionPixelSize(
                        org.chromium.ui.R.dimen.chip_bg_vertical_inset));
        a.recycle();

        mIcon = new ChromeImageView(getContext());
        mIcon.setLayoutParams(new LayoutParams(iconWidth, iconHeight));
        addView(mIcon);

        ViewCompat.setPaddingRelative(this, leadingElementPadding, 0, leadingElementPadding, 0);

        mPrimaryText = new TextView(new ContextThemeWrapper(getContext(), R.style.ChipTextView));
        ApiCompatibilityUtils.setTextAppearance(mPrimaryText, primaryTextAppearance);
        ViewCompat.setPaddingRelative(mPrimaryText, ViewUtils.dpToPx(context, 4), 0, 0, 0);
        addView(mPrimaryText);
        setPrimaryTextMargins(4);

        // Reset icon and background:
        mRippleBackgroundHelper = new RippleBackgroundHelper(this, chipColorId, rippleColorId,
                cornerRadius, R.color.chip_stroke_color, borderWidth, verticalInset);
        setIcon(INVALID_ICON_ID, false);
    }

    private void setPrimaryTextMargins(int marginStart) {
        Context context = mPrimaryText.getContext();
        MarginLayoutParams layoutParams = (MarginLayoutParams) mPrimaryText.getLayoutParams();
        layoutParams.setMarginStart(ViewUtils.dpToPx(context, marginStart));
        layoutParams.setMarginEnd(ViewUtils.dpToPx(context, 8));
        mPrimaryText.setLayoutParams(layoutParams);
    }

    @Override
    protected void drawableStateChanged() {
        super.drawableStateChanged();
        if (mRippleBackgroundHelper != null) {
            mRippleBackgroundHelper.onDrawableStateChanged();
        }
    }

    /**
     * Sets the icon at the start of the chip view.
     * @param icon The resource id pointing to the icon.
     */
    public void setIcon(@DrawableRes int icon, boolean tintWithTextColor) {
        if (icon == INVALID_ICON_ID) {
            setPrimaryTextMargins(4);
            mIcon.setVisibility(ViewGroup.GONE);
            return;
        }

        setPrimaryTextMargins(0);
        mIcon.setVisibility(ViewGroup.VISIBLE);
        mIcon.setImageResource(icon);
        setTint(tintWithTextColor);
    }

    /**
     * Sets the icon at the start of the chip view.
     * @param drawable Drawable to display.
     */
    public void setIcon(Drawable drawable, boolean tintWithTextColor) {
        setPrimaryTextMargins(0);
        mIcon.setVisibility(ViewGroup.VISIBLE);
        mIcon.setImageDrawable(drawable);
        setTint(tintWithTextColor);
    }

    /**
     * Returns the {@link TextView} that contains the label of the chip.
     * @return A {@link TextView}.
     */
    public TextView getPrimaryTextView() {
        return mPrimaryText;
    }

    /**
     * Returns the {@link TextView} that contains the secondary label of the chip. If it wasn't used
     * until now, this creates the view.
     * @return A {@link TextView}.
     */
    public TextView getSecondaryTextView() {
        if (mSecondaryText == null) {
            mSecondaryText =
                    new TextView(new ContextThemeWrapper(getContext(), R.style.ChipTextView));
            ApiCompatibilityUtils.setTextAppearance(mSecondaryText, mSecondaryTextAppearanceId);
            addView(mSecondaryText);
        }
        return mSecondaryText;
    }

    /**
     * Sets the correct tinting on the Chip's image view.
     * @param tintWithTextColor If true then the image view will be tinted with the primary text
     *      color. If not, the tint will be cleared.
     */
    private void setTint(boolean tintWithTextColor) {
        if (mPrimaryText.getTextColors() != null && tintWithTextColor) {
            ApiCompatibilityUtils.setImageTintList(mIcon, mPrimaryText.getTextColors());
        } else {
            ApiCompatibilityUtils.setImageTintList(mIcon, null);
        }
    }
}
