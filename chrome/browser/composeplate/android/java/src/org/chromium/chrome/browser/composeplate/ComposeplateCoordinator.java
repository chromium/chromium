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

    /**
     * Constructs a new ComposeplateCoordinator.
     *
     * @param parentView The parent {@link ViewGroup} for the composeplate.
     * @param isLff Whether the device is a large form factor.
     */
    public ComposeplateCoordinator(ViewGroup parentView, boolean isLff) {
        mIsLff = isLff;
        mModel =
                new PropertyModel.Builder(ComposeplateProperties.ALL_KEYS)
                        .with(ComposeplateProperties.IS_LFF, isLff)
                        .build();
        mView = parentView.findViewById(R.id.composeplate_view);
        PropertyModelChangeProcessor.create(mModel, mView, ComposeplateViewBinder::bind);
        maybeRevertToLegacyLayout();
    }

    private void maybeRevertToLegacyLayout() {
        if (NewTabPageUtils.isNtpAuroraButtonColorEnabled()) {
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

        View searchIcon = mView.findViewById(R.id.composeplate_button_icon);
        if (searchIcon != null) {
            ViewGroup.LayoutParams searchIconParams = searchIcon.getLayoutParams();
            int iconSize =
                    mView.getResources().getDimensionPixelSize(R.dimen.composeplate_view_icon_size);
            searchIconParams.width = iconSize;
            searchIconParams.height = iconSize;
            searchIcon.setLayoutParams(searchIconParams);
        }

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
            int paddingForShadow =
                    mView.getResources()
                            .getDimensionPixelSize(R.dimen.search_box_padding_for_shadow_lateral);
            targetWidth -= paddingForShadow * 2;
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
        // On mobiles, the incognito button is icon-only when the optional button is visible.
        mModel.set(ComposeplateProperties.IS_INCOGNITO_BUTTON_TEXT_VISIBLE, !visible || mIsLff);
        updateButtonSpacing(visible);
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
     * button is visible.
     *
     * @param isOptionalButtonVisible Whether the optional button is visible.
     */
    private void updateButtonSpacing(boolean isOptionalButtonVisible) {
        // Always read the dimensions from the current resources instead of caching them, since
        // the activity isn't recreated on screen rotation and the landscape mode has different
        // values. This ensures fresh values are applied after the screen rotates.
        Resources res = mView.getResources();
        int defaultSpacing = res.getDimensionPixelSize(R.dimen.composeplate_view_button_margin);
        int optionalButtonPadding = defaultSpacing;
        int optionalButtonMarginEnd = defaultSpacing;
        if (isOptionalButtonVisible) {
            optionalButtonPadding =
                    res.getDimensionPixelSize(R.dimen.composeplate_view_optional_button_padding);
            optionalButtonMarginEnd =
                    res.getDimensionPixelSize(R.dimen.composeplate_view_optional_button_margin);
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
     *     ActionChips#CANVAS}.
     */
    public void setOptionalButtonClickListener(
            View.OnClickListener optionalButtonClickListener, @ActionChips int actionChipsType) {
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
            case ActionChips.CANVAS -> ModuleTypeOnStartAndNtp.COMPOSEPLATE_VIEW_CANVAS_BUTTON;
            default ->
                    throw new IllegalArgumentException(
                            "Unsupported action chips type: " + actionChipsType);
        };
    }

    /**
     * Sets the text of the optional button. It is shown on large form factors, and is always used
     * as the content description of the button.
     *
     * @param text The text of the optional button.
     */
    public void setOptionalButtonText(@Nullable String text) {
        mModel.set(ComposeplateProperties.OPTIONAL_BUTTON_TEXT, text);
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
