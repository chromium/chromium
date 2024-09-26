// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Configuration;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.tab.state.ShoppingPersistedTabData;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.widget.MaterialCardViewNoShadow;
import org.chromium.components.browser_ui.widget.textbubble.TextBubble;
import org.chromium.ui.widget.ButtonCompat;
import org.chromium.ui.widget.ChromeImageView;
import org.chromium.ui.widget.ViewRectProvider;

import java.lang.ref.WeakReference;

/**
 * Represents a large message card view in Grid Tab Switcher. The view contains a customized content
 * section, an action button for acceptance, and a close button for dismissal.
 */
class LargeMessageCardView extends FrameLayout {
    private static WeakReference<Bitmap> sCloseButtonBitmapWeakRef;

    private final Context mContext;
    private final int mLandscapeSidePadding;
    private MaterialCardViewNoShadow mMaterialCardViewNoShadow;
    private PriceCardView mPriceInfoBox;
    private ChromeImageView mIcon;
    private TextView mTitle;
    private TextView mDescription;
    private ButtonCompat mActionButton;
    private ButtonCompat mSecondaryActionButton;
    private ChromeImageView mCloseButton;

    public LargeMessageCardView(Context context, AttributeSet attrs) {
        super(context, attrs);
        mContext = context;
        mLandscapeSidePadding =
                (int)
                        context.getResources()
                                .getDimension(
                                        R.dimen.tab_grid_large_message_side_padding_landscape);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mMaterialCardViewNoShadow = findViewById(R.id.large_message_card_view);
        mPriceInfoBox = findViewById(R.id.price_info_box);
        mIcon = findViewById(R.id.icon);
        mTitle = findViewById(R.id.title);
        mDescription = findViewById(R.id.description);
        mActionButton = findViewById(R.id.action_button);
        mSecondaryActionButton = findViewById(R.id.secondary_action_button);
        mCloseButton = findViewById(R.id.close_button);

        if (sCloseButtonBitmapWeakRef == null || sCloseButtonBitmapWeakRef.get() == null) {
            int closeButtonSize =
                    (int) getResources().getDimension(R.dimen.tab_grid_close_button_size);
            Bitmap bitmap = BitmapFactory.decodeResource(getResources(), R.drawable.btn_close);
            sCloseButtonBitmapWeakRef =
                    new WeakReference<>(
                            Bitmap.createScaledBitmap(
                                    bitmap, closeButtonSize, closeButtonSize, true));
        }
        mCloseButton.setImageBitmap(sCloseButtonBitmapWeakRef.get());
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        updateWidthWithOrientation(mContext.getResources().getConfiguration().orientation);
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
    }

    /**
     * Set title text.
     * @param titleText Text to be displayed.
     */
    void setTitleText(String titleText) {
        mTitle.setText(titleText);
    }

    /**
     * Set description text.
     *
     * @param descriptionText Text to be displayed.
     */
    void setDescriptionText(CharSequence descriptionText) {
        mDescription.setText(descriptionText);
    }

    /**
     * Set action text for the action button.
     * @param actionText Text to be displayed.
     */
    void setActionText(String actionText) {
        mActionButton.setText(actionText);
    }

    /**
     * Set text for the secondary action button and toggles the visibility to VISIBLE if GONE.
     * @param secondaryActionText Text to be displayed.
     */
    void setSecondaryActionText(String secondaryActionText) {
        if (mSecondaryActionButton.getVisibility() == View.GONE) {
            mSecondaryActionButton.setVisibility(View.VISIBLE);
        }
        mSecondaryActionButton.setText(secondaryActionText);
    }

    /**
     * Set click listener for the action button.
     * @param listener {@link android.view.View.OnClickListener} for the action button.
     */
    void setActionButtonOnClickListener(OnClickListener listener) {
        mActionButton.setOnClickListener(listener);
    }

    /**
     * Set click listener for the secondary action button.
     * @param listener {@link android.view.View.OnClickListener} for the secondary action button.
     */
    void setSecondaryActionButtonOnClickListener(OnClickListener listener) {
        mSecondaryActionButton.setOnClickListener(listener);
    }

    /**
     * Set content description for dismiss button.
     * @param description The content description.
     */
    void setDismissButtonContentDescription(String description) {
        mCloseButton.setContentDescription(description);
    }

    /**
     * Set {@link android.view.View.OnClickListener} for dismiss button.
     * @param listener {@link android.view.View.OnClickListener} to set.
     */
    void setDismissButtonOnClickListener(OnClickListener listener) {
        mCloseButton.setOnClickListener(listener);
    }

    /** Setup the price info box. */
    void setupPriceInfoBox(@Nullable ShoppingPersistedTabData.PriceDrop priceDrop) {
        if (priceDrop != null) {
            mPriceInfoBox.setPriceStrings(priceDrop.price, priceDrop.previousPrice);
            mPriceInfoBox.setVisibility(View.VISIBLE);
        } else {
            mPriceInfoBox.setVisibility(View.GONE);
        }
    }

    /**
     * Set icon drawable.
     * @param iconDrawable Drawable to be shown.
     */
    void setIconDrawable(Drawable iconDrawable) {
        mIcon.setImageDrawable(iconDrawable);
    }

    /**
     * Set icon visibility.
     * @param visible Whether icon is visible.
     */
    void setIconVisibility(boolean visible) {
        if (visible) {
            mIcon.setVisibility(View.VISIBLE);
        } else {
            mIcon.setVisibility(View.GONE);
        }
    }

    /**
     * Set close button visibility.
     * @param visible Whether icon is visible.
     */
    void setCloseButtonVisibility(boolean visible) {
        if (visible) {
            mCloseButton.setVisibility(View.VISIBLE);
        } else {
            mCloseButton.setVisibility(View.GONE);
        }
    }

    @VisibleForTesting
    void updateWidthWithOrientation(int orientation) {
        if (orientation == Configuration.ORIENTATION_PORTRAIT) {
            setPadding(0, 0, 0, 0);
        } else {
            setPadding(mLandscapeSidePadding, 0, mLandscapeSidePadding, 0);
        }
    }

    // TODO(crbug.com/40164330): This method has little to do with this view. Move this function to
    // a
    // price tracking UI utility class.
    /**
     * When user taps on "Show me" on PriceWelcomeMessage, we scroll them to the binding tab, then a
     * blue tooltip appears and points to the price drop indicator.
     */
    public static void showPriceDropTooltip(View view) {
        ViewRectProvider rectProvider = new ViewRectProvider(view);
        TextBubble textBubble =
                new TextBubble(
                        view.getContext(),
                        view,
                        R.string.price_drop_spotted_lower_price,
                        R.string.price_drop_spotted_lower_price,
                        true,
                        rectProvider,
                        ChromeAccessibilityUtil.get().isAccessibilityEnabled());
        textBubble.setFocusable(true);
        textBubble.setDismissOnTouchInteraction(true);
        textBubble.show();
    }

    /**
     * Update Message Card when switching between normal mode and incognito mode.
     *
     * @param isIncognito Whether it is in the incognito mode.
     */
    void updateMessageCardColor(boolean isIncognito) {
        setBackground(isIncognito);
        MessageCardViewUtils.setTitleTextAppearance(
                mTitle, isIncognito, /* isLargeMessageCard= */ true);
        MessageCardViewUtils.setDescriptionTextAppearance(
                mDescription, isIncognito, /* isLargeMessageCard= */ true);
        MessageCardViewUtils.setActionButtonTextAppearance(
                mActionButton, isIncognito, /* isLargeMessageCard= */ true);
        MessageCardViewUtils.setActionButtonBackgroundColor(
                mActionButton, isIncognito, /* isLargeMessageCard= */ true);
        MessageCardViewUtils.setSecondaryActionButtonColor(mSecondaryActionButton, isIncognito);
        MessageCardViewUtils.setCloseButtonTint(mCloseButton, isIncognito);
    }

    /**
     * Update the icon's width.
     *
     * <p>TODO(crbug.com/40056462): Confirm with UX, whether large message card can follow a general
     * icon size for all clients. If so, then remove this method.
     *
     * @param widthInPixels The desired width in pixels to set.
     */
    void updateIconWidth(int widthInPixels) {
        mIcon.getLayoutParams().width = widthInPixels;
    }

    /**
     * Update the icon's height.
     *
     * <p>TODO(crbug.com/40056462): Confirm with UX, whether large message card can follow a general
     * icon size for all clients. If so, then remove this method.
     *
     * @param heightInPixels The desired height in pixels to set.
     */
    void updateIconHeight(int heightInPixels) {
        mIcon.getLayoutParams().height = heightInPixels;
    }

    /**
     * Set background resource.
     *
     * @param isIncognito Whether the resource is used for incognito mode.
     */
    private void setBackground(boolean isIncognito) {
        ColorStateList backgroundTint =
                ColorStateList.valueOf(
                        isIncognito
                                ? mContext.getColor(R.color.incognito_card_bg_color)
                                : ChromeColors.getSurfaceColor(
                                        mContext, R.dimen.default_elevation_2));
        mMaterialCardViewNoShadow.setBackgroundTintList(backgroundTint);
    }
}
