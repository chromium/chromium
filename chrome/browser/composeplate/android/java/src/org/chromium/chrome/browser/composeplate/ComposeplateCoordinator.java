// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.composeplate;

import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.DrawableRes;
import androidx.annotation.StyleRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ntp.NewTabPageUtils;
import org.chromium.chrome.browser.ntp.NewTabPageUtils.ActionChips;
import org.chromium.chrome.browser.util.BrowserUiUtils.ModuleTypeOnStartAndNtp;
import org.chromium.components.search_engines.AiModeButtonUiConfig;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Coordinator for the composeplate on the NTP. */
@NullMarked
public class ComposeplateCoordinator {
    private final PropertyModel mModel;
    private final ComposeplateView mView;
    private final boolean mIsLff;

    // The dimensions below have a single value across configurations, e.g., portrait and
    // landscape, so they are read once and cached.

    /** The default spacing of the buttons. */
    private final int mDefaultButtonSpacingPx;

    /** The lateral padding of the optional button with the large form factor layout. */
    private final int mOptionalButtonPaddingLffPx;

    /** The end margin of the optional button with the large form factor layout. */
    private final int mOptionalButtonMarginEndLffPx;

    /** The lateral padding reserved for the search box shadow when NTP Aurora is enabled. */
    private final int mSearchBoxShadowPaddingPx;

    /**
     * The min window width in dp on large form factor devices to use the large form factor layout.
     */
    private final int mLffLayoutMinWindowWidthDp;

    /** Whether the composeplate is shown in a narrow window, see {@link #setIsInNarrowWindow}. */
    private boolean mIsInNarrowWindow;

    /**
     * The {@link ActionChips} type of the optional button, used to record its impression metrics.
     * Null until the optional button's click listener is set.
     */
    private @Nullable @ActionChips Integer mOptionalButtonType;

    /**
     * Constructs a new ComposeplateCoordinator.
     *
     * @param parentView The parent {@link ViewGroup} for the composeplate.
     * @param isLff Whether the device is a large form factor.
     * @param horizontalInsetDp The horizontal inset of the window in dp, used to initialize whether
     *     the composeplate is shown in a narrow window. See {@link #updateIsInNarrowWindow}.
     */
    public ComposeplateCoordinator(ViewGroup parentView, boolean isLff, int horizontalInsetDp) {
        mIsLff = isLff;
        mModel = new PropertyModel.Builder(ComposeplateProperties.ALL_KEYS).build();
        mView = parentView.findViewById(R.id.composeplate_view);

        Resources res = mView.getResources();
        mDefaultButtonSpacingPx =
                res.getDimensionPixelSize(R.dimen.composeplate_view_button_margin);
        mOptionalButtonPaddingLffPx =
                res.getDimensionPixelSize(R.dimen.composeplate_view_optional_button_padding_lff);
        mOptionalButtonMarginEndLffPx =
                res.getDimensionPixelSize(R.dimen.composeplate_view_optional_button_margin_lff);
        mSearchBoxShadowPaddingPx =
                res.getDimensionPixelSize(R.dimen.search_box_padding_for_shadow_lateral);
        // Round to avoid float precision issues when comparing with the integer window width.
        mLffLayoutMinWindowWidthDp =
                Math.round(
                        res.getDimension(R.dimen.composeplate_view_lff_layout_min_window_width)
                                / res.getDisplayMetrics().density);

        PropertyModelChangeProcessor.create(mModel, mView, ComposeplateViewBinder::bind);
        maybeRevertToLegacyLayout();
        updateIsInNarrowWindow(horizontalInsetDp);
    }

    private void maybeRevertToLegacyLayout() {
        // Early return for isBesideMvtModuleEnabled to avoid modifying the AI Mode section beside
        // the MVT.
        if (NewTabPageUtils.isNtpAuroraButtonColorEnabled()
                || NewTabPageUtils.isBesideMvtModuleEnabled()) {
            return;
        }

        ViewGroup.MarginLayoutParams containerParams =
                (ViewGroup.MarginLayoutParams) mView.getLayoutParams();
        if (containerParams == null) {
            return;
        }

        Resources resources = mView.getResources();
        containerParams.topMargin =
                resources.getDimensionPixelSize(R.dimen.composeplate_view_legacy_margin_top);
        containerParams.bottomMargin =
                resources.getDimensionPixelSize(R.dimen.ntp_section_bottom_margin);
        containerParams.height =
                resources.getDimensionPixelSize(R.dimen.composeplate_view_legacy_height);
        mView.setLayoutParams(containerParams);

        mView.setTextStyle(R.style.TextAppearance_ComposeplateTextMedium);
    }

    /**
     * Sets the visibility of the composeplate.
     *
     * @param visible Whether the composeplate should be visible.
     * @param isCurrentPage whether the New Tab Page is the current page displayed to the user.
     */
    public void setVisibility(boolean visible, boolean isCurrentPage) {
        if (isCurrentPage && visible != mModel.get(ComposeplateProperties.IS_VISIBLE)) {
            ComposeplateMetricsUtils.recordComposeplateImpression(visible);
        }

        mModel.set(ComposeplateProperties.IS_VISIBLE, visible);
    }

    /**
     * Records an impression for each visible button on the composeplate: the AI Mode button, the
     * incognito button and, if shown, the optional button. Nothing is recorded if the composeplate
     * isn't visible. Should only be called when the New Tab Page is shown to the user.
     */
    public void recordButtonsImpression() {
        if (!mModel.get(ComposeplateProperties.IS_VISIBLE)) return;

        ComposeplateMetricsUtils.recordComposeplateButtonImpression(ActionChips.DEFAULT);
        ComposeplateMetricsUtils.recordComposeplateButtonImpression(ActionChips.INCOGNITO);
        if (mOptionalButtonType != null
                && mModel.get(ComposeplateProperties.IS_OPTIONAL_BUTTON_VISIBLE)) {
            ComposeplateMetricsUtils.recordComposeplateButtonImpression(mOptionalButtonType);
        }
    }

    /**
     * Sets the click listener for the incognito button.
     *
     * @param incognitoClickListener The click listener for the incognito button.
     */
    public void setIncognitoClickListener(View.OnClickListener incognitoClickListener) {
        mModel.set(
                ComposeplateProperties.INCOGNITO_CLICK_LISTENER,
                createEnhancedClickListener(
                        incognitoClickListener,
                        ModuleTypeOnStartAndNtp.COMPOSEPLATE_VIEW_INCOGNITO_BUTTON));
    }

    /**
     * Sets the click listener for the composeplate button.
     *
     * @param composeplateButtonClickListener The click listener for the composeplate button.
     */
    public void setComposeplateButtonClickListener(
            View.OnClickListener composeplateButtonClickListener) {
        mModel.set(
                ComposeplateProperties.COMPOSEPLATE_BUTTON_CLICK_LISTENER,
                createEnhancedClickListener(
                        composeplateButtonClickListener,
                        ModuleTypeOnStartAndNtp.COMPOSEPLATE_BUTTON));
    }

    /**
     * Sets the width of the composeplate view in LayoutParams and clears its margins. It keeps the
     * composeplate the same width as the fake search box, except when the Aurora button color
     * change feature is enabled, lateral margins are subtracted. Furthermore, if the Aurora feature
     * is enabled, it adds a shadow, so the shadow padding must be subtracted. This should be called
     * before the parent view's measure pass to avoid double measurement.
     *
     * @param searchBoxWidthPx The width of the fake search box.
     */
    public void setLayoutWidth(int searchBoxWidthPx) {
        ViewGroup.MarginLayoutParams layoutParams =
                (ViewGroup.MarginLayoutParams) mView.getLayoutParams();

        int targetWidth = searchBoxWidthPx;

        if (NewTabPageUtils.isNtpAuroraButtonColorEnabled()) {
            // If the aurora button color is enabled, adjust the margin.
            int margin =
                    mView.getResources()
                            .getDimensionPixelSize(R.dimen.composeplate_view_lateral_margin);
            targetWidth -= margin * 2;
        } else if (NewTabPageUtils.isNtpAuroraEnabled()) {
            // If aurora is enabled, consider the shadow.
            targetWidth -= mSearchBoxShadowPaddingPx * 2;
        }

        if (layoutParams.width == targetWidth) {
            return;
        }

        layoutParams.width = targetWidth;
    }

    /**
     * Wraps the given {@link View.OnClickListener} to record the click metric before invoking the
     * original listener.
     *
     * @param originalListener The original click listener to be wrapped.
     * @param sectionType The {@link ModuleTypeOnStartAndNtp} to record for the click.
     */
    private View.OnClickListener createEnhancedClickListener(
            View.OnClickListener originalListener, @ModuleTypeOnStartAndNtp int sectionType) {
        return v -> {
            if (originalListener != null) {
                originalListener.onClick(v);
            }
            ComposeplateMetricsUtils.recordComposeplateClick(sectionType);
        };
    }

    public void destroy() {
        mModel.set(ComposeplateProperties.INCOGNITO_CLICK_LISTENER, null);
        mModel.set(ComposeplateProperties.COMPOSEPLATE_BUTTON_CLICK_LISTENER, null);
        mModel.set(ComposeplateProperties.OPTIONAL_BUTTON_CLICK_LISTENER, null);
    }

    /**
     * Sets the visibility of the optional button.
     *
     * @param visible Whether the optional button should be visible.
     */
    public void setOptionalButtonVisibility(boolean visible) {
        mModel.set(ComposeplateProperties.IS_OPTIONAL_BUTTON_VISIBLE, visible);
        updateButtonLayout();
        updateButtonSpacing(visible);
    }

    /**
     * Updates whether the composeplate is shown in a narrow window on large form factor devices,
     * i.e., narrower than {@code R.dimen.composeplate_view_lff_layout_min_window_width}. No-op on
     * phones, which always use the phone layout. This threshold doesn't match any {@code
     * HorizontalDisplayStyle}, so this should be called whenever the window width or the horizontal
     * inset may have changed, e.g., on every {@code UiConfig} notification.
     *
     * @param horizontalInsetDp The horizontal inset of the window in dp, which is excluded from the
     *     window width, the same as the width that {@code UiConfig} uses to compute the display
     *     style. See {@code UiConfig#getHorizontalInset()}.
     */
    public void updateIsInNarrowWindow(int horizontalInsetDp) {
        if (!mIsLff) return;

        int windowWidthDp =
                mView.getResources().getConfiguration().screenWidthDp - horizontalInsetDp;
        setIsInNarrowWindow(windowWidthDp < mLffLayoutMinWindowWidthDp);
    }

    /**
     * Sets whether the composeplate is shown in a narrow window. On large form factor devices, the
     * buttons use the same layout and spacing as on phones in narrow windows, since there isn't
     * enough space to show the texts of all buttons.
     *
     * @param isInNarrowWindow Whether the composeplate is shown in a narrow window.
     */
    private void setIsInNarrowWindow(boolean isInNarrowWindow) {
        mIsInNarrowWindow = isInNarrowWindow;
        updateButtonLayout();
        updateButtonSpacing(mModel.get(ComposeplateProperties.IS_OPTIONAL_BUTTON_VISIBLE));
    }

    /** Returns whether the buttons use the large form factor layout. */
    private boolean useLffLayout() {
        return mIsLff && !mIsInNarrowWindow;
    }

    /**
     * Updates the layout of the buttons, which depends on whether the optional button is visible
     * and whether to use the large form factor layout:
     *
     * <ul>
     *   <li>With the large form factor layout, i.e., on large form factor devices except in narrow
     *       windows, the optional button shows its text and all buttons share the width equally.
     *   <li>Otherwise, the optional button is icon-only. When it is visible, the incognito button
     *       is also icon-only, and both buttons wrap their content, so that the composeplate button
     *       takes the rest of the width.
     * </ul>
     */
    private void updateButtonLayout() {
        boolean isOptionalButtonVisible =
                mModel.get(ComposeplateProperties.IS_OPTIONAL_BUTTON_VISIBLE);
        boolean useLffLayout = useLffLayout();
        mModel.set(ComposeplateProperties.IS_OPTIONAL_BUTTON_TEXT_VISIBLE, useLffLayout);
        mModel.set(ComposeplateProperties.IS_OPTIONAL_BUTTON_WRAP_CONTENT, !useLffLayout);
        mModel.set(
                ComposeplateProperties.IS_INCOGNITO_BUTTON_TEXT_VISIBLE,
                !isOptionalButtonVisible || useLffLayout);
        mModel.set(
                ComposeplateProperties.IS_INCOGNITO_BUTTON_WRAP_CONTENT,
                isOptionalButtonVisible && !useLffLayout);
    }

    /**
     * Called when the display style changes, e.g. on screen rotation. Re-applies the button
     * spacing, since the dimensions may differ between configurations.
     */
    public void onDisplayStyleChanged() {
        boolean isOptionalButtonVisible =
                mModel.get(ComposeplateProperties.IS_OPTIONAL_BUTTON_VISIBLE);
        if (!isOptionalButtonVisible) return;

        updateButtonSpacing(isOptionalButtonVisible);
    }

    /**
     * Updates the lateral paddings and end margins of the buttons based on whether the optional
     * button is visible and whether to use the large form factor layout.
     *
     * @param isOptionalButtonVisible Whether the optional button is visible.
     */
    private void updateButtonSpacing(boolean isOptionalButtonVisible) {
        if (NewTabPageUtils.isBesideMvtModuleEnabled()) return;

        int optionalButtonPadding = mDefaultButtonSpacingPx;
        int optionalButtonMarginEnd = mDefaultButtonSpacingPx;
        if (isOptionalButtonVisible) {
            if (useLffLayout()) {
                optionalButtonPadding = mOptionalButtonPaddingLffPx;
                optionalButtonMarginEnd = mOptionalButtonMarginEndLffPx;
            } else {
                // Always read the phone layout dimensions from the current resources instead of
                // caching them, since the activity isn't recreated on configuration changes and
                // narrow and wide windows have different values (see values-w360dp/dimens.xml and
                // values-w600dp/dimens.xml). This ensures fresh values are applied whenever the
                // NTP notifies a display style change, e.g., after the screen rotates or the
                // window is resized.
                Resources res = mView.getResources();
                optionalButtonPadding =
                        res.getDimensionPixelSize(
                                R.dimen.composeplate_view_optional_button_padding);
                optionalButtonMarginEnd =
                        res.getDimensionPixelSize(R.dimen.composeplate_view_optional_button_margin);
            }
        }

        mModel.set(ComposeplateProperties.OPTIONAL_BUTTON_LATERAL_PADDING, optionalButtonPadding);
        mModel.set(ComposeplateProperties.OPTIONAL_BUTTON_MARGIN_END, optionalButtonMarginEnd);
    }

    /**
     * Sets the icon of the optional button.
     *
     * @param iconResId The resource id of the icon drawable to show on the optional button.
     */
    public void setOptionalButtonIcon(@DrawableRes int iconResId) {
        mModel.set(ComposeplateProperties.OPTIONAL_BUTTON_ICON_RES_ID, iconResId);
    }

    /**
     * Sets the click listener for the optional button.
     *
     * @param optionalButtonClickListener The click listener for the optional button.
     * @param actionChipsType The {@link ActionChips} type shown on the optional button, used to
     *     record the click metric. Must be {@link ActionChips#CREATE_IMAGE} or {@link
     *     ActionChips#GALLERY}.
     */
    public void setOptionalButtonClickListener(
            View.OnClickListener optionalButtonClickListener, @ActionChips int actionChipsType) {
        mOptionalButtonType = actionChipsType;
        mModel.set(
                ComposeplateProperties.OPTIONAL_BUTTON_CLICK_LISTENER,
                createEnhancedClickListener(
                        optionalButtonClickListener, getModuleTypeForActionChips(actionChipsType)));
    }

    private static @ModuleTypeOnStartAndNtp int getModuleTypeForActionChips(
            @ActionChips int actionChipsType) {
        return switch (actionChipsType) {
            case ActionChips.CREATE_IMAGE ->
                    ModuleTypeOnStartAndNtp.COMPOSEPLATE_VIEW_CREATE_IMAGE_BUTTON;
            case ActionChips.GALLERY -> ModuleTypeOnStartAndNtp.COMPOSEPLATE_VIEW_GALLERY_BUTTON;
            default ->
                    throw new IllegalArgumentException(
                            "Unsupported action chips type: " + actionChipsType);
        };
    }

    /**
     * Sets the text of the optional button. It is only shown with the large form factor layout; see
     * {@link #setOptionalButtonContentDescription(String)} for the accessible name.
     *
     * @param text The text of the optional button.
     */
    public void setOptionalButtonText(@Nullable String text) {
        mModel.set(ComposeplateProperties.OPTIONAL_BUTTON_TEXT, text);
    }

    /**
     * Sets the content description of the optional button, which is also used as its tooltip. It
     * should be more descriptive than the text, since the button is icon-only on mobiles.
     *
     * @param contentDescription The content description of the optional button.
     */
    public void setOptionalButtonContentDescription(@Nullable String contentDescription) {
        mModel.set(ComposeplateProperties.OPTIONAL_BUTTON_CONTENT_DESCRIPTION, contentDescription);
    }

    public void applyWhiteBackground(boolean apply) {
        mModel.set(ComposeplateProperties.APPLY_WHITE_BACKGROUND, apply);

        ColorStateList colorStateList =
                ComposeplateUtils.getSearchBoxIconColorTint(mView.getContext(), apply);
        @StyleRes int textStyleResId = ComposeplateUtils.getSearchBoxTextStyleResId(apply);
        mModel.set(ComposeplateProperties.COLOR_STATE_LIST, colorStateList);
        mModel.set(ComposeplateProperties.TEXT_STYLE_RES_ID, textStyleResId);
    }

    /**
     * Updates the AI Mode button with the UI config of the default search engine. The config
     * carries the strings to show, which vary per search engine.
     *
     * @param aiModeButtonUiConfig The {@link AiModeButtonUiConfig} of the default search engine.
     */
    public void updateAiModeButtonUiConfig(AiModeButtonUiConfig aiModeButtonUiConfig) {
        mModel.set(ComposeplateProperties.AI_MODE_BUTTON_UI_CONFIG, aiModeButtonUiConfig);
    }

    /**
     * Updates the icon of the AI Mode button.
     *
     * @param iconDrawable The icon {@link Drawable} to show on the AI Mode button.
     * @param shouldTint Whether the icon should be tinted to match the other composeplate icons.
     *     Pass false for full color icons, e.g. a favicon.
     */
    public void updateAiModeButtonIcon(@Nullable Drawable iconDrawable, boolean shouldTint) {
        mModel.set(
                ComposeplateProperties.AI_MODE_BUTTON_ICON,
                new ComposeplateProperties.AiModeButtonIcon(iconDrawable, shouldTint));
    }

    public PropertyModel getModelForTesting() {
        return mModel;
    }
}
