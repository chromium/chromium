// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.LayerDrawable;
import android.util.AttributeSet;
import android.view.View;
import android.widget.LinearLayout;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.ui.widget.ButtonCompat;
import org.chromium.ui.widget.ChromeImageView;
import org.chromium.ui.widget.OutlineOverlayHelper;
import org.chromium.ui.widget.TextViewWithLeading;

import java.lang.ref.WeakReference;

/**
 * Represents a secondary card view in Grid Tab Switcher. The view contains an icon, a description,
 * an action button for acceptance, and a close button for dismissal.
 */
@NullMarked
class MessageCardView extends LinearLayout {
    private static @Nullable WeakReference<Bitmap> sCloseButtonBitmapWeakRef;

    /** An interface to get the icon to be shown inside the message card. */
    public interface IconProvider {
        void fetchIconDrawable(Callback<Drawable> drawable);
    }

    /** An interface to handle a message action. */
    public interface ActionProvider {
        void action();
    }

    /** An interface to handle the dismiss action. */
    public interface ServiceDismissActionProvider<T> {
        void dismiss(T messageType);
    }

    private ChromeImageView mIcon;
    private TextViewWithLeading mDescription;
    private ButtonCompat mActionButton;
    private ChromeImageView mCloseButton;
    private OutlineOverlayHelper mOutlineOverlayHelper;

    public MessageCardView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mIcon = findViewById(R.id.icon);
        mDescription = findViewById(R.id.description);
        mActionButton = findViewById(R.id.action_button);
        mCloseButton = findViewById(R.id.close_button);

        if (sCloseButtonBitmapWeakRef == null || sCloseButtonBitmapWeakRef.get() == null) {
            int closeButtonSize =
                    (int) getResources().getDimension(R.dimen.message_card_close_button_size);
            Bitmap bitmap = BitmapFactory.decodeResource(getResources(), R.drawable.btn_close);
            sCloseButtonBitmapWeakRef =
                    new WeakReference<>(
                            Bitmap.createScaledBitmap(
                                    bitmap, closeButtonSize, closeButtonSize, true));
        }
        mCloseButton.setImageBitmap(sCloseButtonBitmapWeakRef.get());
        mOutlineOverlayHelper = OutlineOverlayHelper.attach(this);
        mOutlineOverlayHelper.setStrokeColor(
                ChromeColors.getKeyboardFocusRingColor(getContext(), false));
    }

    /**
     * @see TextView#setText(CharSequence).
     */
    void setDescriptionText(CharSequence text) {
        mDescription.setText(text);
    }

    /**
     * Set action text for the action button.
     * @param actionText Text to be displayed.
     */
    void setActionText(String actionText) {
        mActionButton.setText(actionText);
    }

    /**
     * Set icon drawable.
     *
     * @param iconDrawable Drawable to be shown.
     */
    void setIcon(Drawable iconDrawable) {
        mIcon.setVisibility(View.VISIBLE);
        mIcon.setImageDrawable(iconDrawable);
    }

    /**
     * Set click listener for the action button.
     * @param listener {@link android.view.View.OnClickListener} for the action button.
     */
    void setActionButtonOnClickListener(OnClickListener listener) {
        mActionButton.setOnClickListener(listener);
    }

    /**
     * Sets the action button visibility.
     *
     * @param visible Whether the action button is visible.
     */
    void setActionButtonVisible(boolean visible) {
        mActionButton.setVisibility(visible ? View.VISIBLE : View.GONE);
    }

    /**
     * Set content description for dismiss button.
     *
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

    /**
     * Modify the view based on the visibility of the icon. For messages that doesn't have an icon,
     * remove the icon and update the margin of the description text field.
     *
     * @param visible Whether icon is visible.
     */
    void setIconVisibility(boolean visible) {
        Resources resources = getResources();
        int verticalMargin =
                resources.getDimensionPixelOffset(R.dimen.message_card_description_vertical_margin);
        MarginLayoutParams params = (MarginLayoutParams) mDescription.getLayoutParams();
        if (visible) {
            if (indexOfChild(mIcon) == -1) {
                addView(mIcon, 0);
                params.setMargins(0, verticalMargin, 0, verticalMargin);
            }
        } else {
            removeView(mIcon);
            int leftMargin =
                    resources.getDimensionPixelSize(R.dimen.tab_grid_iph_item_description_margin);
            params.setMargins(leftMargin, verticalMargin, 0, verticalMargin);
        }
    }

    /**
     * Set background resource.
     * @param isIncognito Whether the resource is used for incognito mode.
     */
    private void setBackground(boolean isIncognito) {
        setBackgroundResource(TabUiThemeProvider.getMessageCardBackgroundResourceId(isIncognito));
        // Incognito colors should follow baseline.
        // Use the color defined in drawable.
        if (isIncognito) {
            return;
        }
        // Set dynamic color.
        LayerDrawable drawable = (LayerDrawable) getBackground();
        drawable.findDrawableByLayerId(R.id.card_background_base)
                .setTint(SemanticColorUtils.getCardBackgroundColor(getContext()));
    }

    /**
     * Update Message Card when switching between normal mode and incognito mode.
     * @param isIncognito Whether it is in the incognito mode.
     */
    void updateMessageCardColor(boolean isIncognito) {
        setBackground(isIncognito);
        MessageCardViewUtils.setDescriptionTextAppearance(
                mDescription, isIncognito, /* isLargeMessageCard= */ false);
        MessageCardViewUtils.setActionButtonTextAppearance(
                mActionButton, isIncognito, /* isLargeMessageCard= */ false);
        MessageCardViewUtils.setCloseButtonTint(mCloseButton, isIncognito);

        mOutlineOverlayHelper.setStrokeColor(
                ChromeColors.getKeyboardFocusRingColor(getContext(), isIncognito));
    }

    /**
     * Set left margin of the message card.
     *
     * @param leftMarginPx Left margin of the card in px.
     */
    void setLeftMargin(int leftMarginPx) {
        MarginLayoutParams params = (MarginLayoutParams) getLayoutParams();
        params.leftMargin = leftMarginPx;
        setLayoutParams(params);
    }

    /**
     * Set top margin of the message card.
     *
     * @param topMarginPx Top margin of the card in px.
     */
    void setTopMargin(int topMarginPx) {
        MarginLayoutParams params = (MarginLayoutParams) getLayoutParams();
        params.topMargin = topMarginPx;
        setLayoutParams(params);
    }

    /**
     * Set right margin of the message card.
     *
     * @param rightMarginPx Right margin of the card in px.
     */
    void setRightMargin(int rightMarginPx) {
        MarginLayoutParams params = (MarginLayoutParams) getLayoutParams();
        params.rightMargin = rightMarginPx;
        setLayoutParams(params);
    }

    /**
     * Set bottom margin of the message card.
     *
     * @param bottomMarginPx Bottom margin of the card in px.
     */
    void setBottomMargin(int bottomMarginPx) {
        MarginLayoutParams params = (MarginLayoutParams) getLayoutParams();
        params.bottomMargin = bottomMarginPx;
        setLayoutParams(params);
    }
}
