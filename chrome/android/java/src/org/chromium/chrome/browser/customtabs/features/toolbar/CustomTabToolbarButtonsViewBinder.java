// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.toolbar;

import static androidx.browser.customtabs.CustomTabsIntent.CLOSE_BUTTON_POSITION_END;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarButtonsProperties.CLICK_LISTENER;
import static org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarButtonsProperties.CLOSE_BUTTON;
import static org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarButtonsProperties.CUSTOM_ACTION_BUTTONS;
import static org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarButtonsProperties.CUSTOM_ACTION_BUTTONS_VISIBLE;
import static org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarButtonsProperties.DESCRIPTION;
import static org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarButtonsProperties.ICON;
import static org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarButtonsProperties.IS_INCOGNITO;
import static org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarButtonsProperties.MENU_BUTTON_VISIBLE;
import static org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarButtonsProperties.MINIMIZE_BUTTON;
import static org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarButtonsProperties.OMNIBOX_ENABLED;
import static org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarButtonsProperties.OPTIONAL_BUTTON_VISIBLE;
import static org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarButtonsProperties.SIDE_SHEET_MAXIMIZE_BUTTON;
import static org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarButtonsProperties.TINT;
import static org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarButtonsProperties.TITLE_VISIBLE;
import static org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarButtonsProperties.TOOLBAR_WIDTH;
import static org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarButtonsProperties.TYPE;

import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.util.Pair;
import android.util.SparseBooleanArray;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageButton;

import androidx.annotation.DimenRes;
import androidx.annotation.DrawableRes;
import androidx.annotation.Px;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browserservices.intents.CustomButtonParams.ButtonType;
import org.chromium.components.browser_ui.widget.TintedDrawable;
import org.chromium.ui.UiUtils;
import org.chromium.ui.modelutil.ListModelChangeProcessor;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyListModel;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

@NullMarked
public class CustomTabToolbarButtonsViewBinder
        implements PropertyModelChangeProcessor.ViewBinder<
                        PropertyModel, CustomTabToolbar, PropertyKey>,
                ListModelChangeProcessor.ViewBinder<
                        PropertyListModel<PropertyModel, PropertyKey>,
                        CustomTabToolbar,
                        PropertyKey> {
    private static final class ButtonPositioningParams {
        public int availableWidth;
        public int totalStartAlignedButtonWidth;
        public int spacingFromLastStartAlignedButton;
        public int totalEndAlignedButtonWidth;
    }

    /**
     * Adjusts button visibility priority between minimize and Custom/Chrome action buttons. Chrome
     * action buttons (Share, Open-in-Browser) of state DEFAULT has a priority lower than minimize
     * button i.e. MINIMIZE > SHARE > OPEN-IN-CHROME > EXPAND. If MINIMIZE was hidden and either
     * SHARE or OPEN-IN-CHROME is visible, flip their state.
     */
    private static class ButtonVisibilityFlipper {
        private boolean mActive; // Visibility needs flipping if true.
        private @ButtonType int mButtonToHide; // The type of Chrome action button to hide.
        private final SparseBooleanArray mVisibleButtons =
                new SparseBooleanArray(2); // For OPEN_IN_BROWSER, SHARE

        private boolean canShowMinimizeButton() {
            return mActive;
        }

        private boolean isCustomButtonToHide(@ButtonType int buttonType) {
            return mActive && mButtonToHide == buttonType;
        }

        private void addVisibleButtonType(@ButtonType int buttonType) {
            if (!mActive) mVisibleButtons.put(buttonType, true);
        }

        private boolean maybeFlipVisibility() {
            if (mActive) return false;

            // If minimize button is hidden and chrome action (either share or open-in-browser) is
            // shown, set |mActive| to true to enable minimize button, mark the chrome action (in
            // the order of open-in-browser, share) to hide. This take effect in the next round of
            // positioning job.
            if (mVisibleButtons.get(ButtonType.CCT_OPEN_IN_BROWSER_BUTTON)) {
                mActive = true;
                mButtonToHide = ButtonType.CCT_OPEN_IN_BROWSER_BUTTON;
            } else if (mVisibleButtons.get(ButtonType.CCT_SHARE_BUTTON)) {
                mActive = true;
                mButtonToHide = ButtonType.CCT_SHARE_BUTTON;
            }
            return mActive;
        }

        // Reset the flip state whenever the toolbar width is altered.
        private void reset() {
            mActive = false;
            mButtonToHide = ButtonType.OTHER;
            mVisibleButtons.clear();
        }
    }

    private final ButtonVisibilityFlipper mVisFlipper = new ButtonVisibilityFlipper();

    @Override
    public void bind(PropertyModel model, CustomTabToolbar view, PropertyKey propertyKey) {
        // Changing the TINT won't require a relayout.
        if (propertyKey == TINT) {
            updateAllButtonsTint(view, model.get(TINT));
        } else {
            mVisFlipper.reset();
            inflateAndPositionToolbarElements(view, model, mVisFlipper);
        }
    }

    @Override
    public void onItemsInserted(
            PropertyListModel<PropertyModel, PropertyKey> model,
            CustomTabToolbar view,
            int index,
            int count) {
        inflateAndPositionToolbarElements(
                view, (PropertyModel) view.getTag(R.id.view_model), mVisFlipper);
    }

    @Override
    public void onItemsRemoved(
            PropertyListModel<PropertyModel, PropertyKey> model,
            CustomTabToolbar view,
            int index,
            int count) {
        inflateAndPositionToolbarElements(
                view, (PropertyModel) view.getTag(R.id.view_model), mVisFlipper);
    }

    @Override
    public void onItemsChanged(
            PropertyListModel<PropertyModel, PropertyKey> model,
            CustomTabToolbar view,
            int index,
            int count,
            @Nullable PropertyKey payload) {
        inflateAndPositionToolbarElements(
                view, (PropertyModel) view.getTag(R.id.view_model), mVisFlipper);
    }

    /**
     * Inflates and positions the buttons and the location bar within the toolbar based on the
     * provided available width and the current button model. If there isn't enough space for all
     * buttons, some buttons may be omitted based on their priority.
     *
     * <p>This method should be called every time the toolbar width changes.
     *
     * @param view The {@link CustomTabToolbar} that hosts the buttons.
     * @param model The {@link PropertyModel} containing the needed properties.
     * @param visFlipper {@link ButtonVisibilityFlipper} used to adjust button priority.
     */
    private static void inflateAndPositionToolbarElements(
            CustomTabToolbar view, PropertyModel model, ButtonVisibilityFlipper visFlipper) {
        var resources = view.getResources();
        int defaultButtonWidth = resources.getDimensionPixelSize(R.dimen.toolbar_button_width);
        int defaultButtonHorizontalPadding =
                resources.getDimensionPixelSize(
                        R.dimen.custom_tabs_toolbar_button_horizontal_padding);
        int toolbarHorizontalPadding =
                resources.getDimensionPixelSize(R.dimen.custom_tabs_toolbar_horizontal_padding);
        int locationBarMinWidth =
                getLocationBarMinWidth(
                        view.getResources(), model.get(OMNIBOX_ENABLED), model.get(TITLE_VISIBLE));
        var posParams = new ButtonPositioningParams();
        posParams.availableWidth = model.get(TOOLBAR_WIDTH);
        posParams.availableWidth -= 2 * toolbarHorizontalPadding;
        posParams.totalStartAlignedButtonWidth = toolbarHorizontalPadding;
        posParams.totalEndAlignedButtonWidth = toolbarHorizontalPadding;

        if (model.get(IS_INCOGNITO)) {
            int incognitoIconWidth =
                    resources.getDimensionPixelSize(R.dimen.custom_tabs_incognito_icon_width);
            locationBarMinWidth += incognitoIconWidth;

            view.ensureIncognitoImageViewInflated();
        }

        posParams.availableWidth -= locationBarMinWidth;

        if (model.get(CLOSE_BUTTON).visible) {
            var closeButton = view.ensureCloseButtonInflated();
            closeButton.setImageDrawable(model.get(CLOSE_BUTTON).icon);
            closeButton.setOnLongClickListener(view);
            closeButton.setOnClickListener(model.get(CLOSE_BUTTON).clickListener);

            boolean isEndPosition = model.get(CLOSE_BUTTON).position == CLOSE_BUTTON_POSITION_END;
            positionButton(
                    closeButton,
                    posParams,
                    defaultButtonWidth,
                    defaultButtonHorizontalPadding,
                    isEndPosition);
        } else if (view.getCloseButton() != null) {
            view.getCloseButton().setVisibility(View.GONE);
        }

        if (model.get(MENU_BUTTON_VISIBLE)) {
            var menuButton = view.ensureMenuButtonInflated();
            boolean isEndPosition = model.get(CLOSE_BUTTON).position != CLOSE_BUTTON_POSITION_END;
            positionButton(
                    menuButton,
                    posParams,
                    defaultButtonWidth,
                    defaultButtonHorizontalPadding,
                    isEndPosition);
        } else if (view.getMenuButton() != null) {
            view.getMenuButton().setVisibility(View.GONE);
        }

        FrameLayout customActionButtons = assumeNonNull(view.getCustomActionButtonsParent());
        // TODO(crbug.com/402213312): Think of how we can optimize this so we don't reinflate all
        // buttons any time if we add/remove one.
        customActionButtons.removeAllViews();

        if (model.get(CUSTOM_ACTION_BUTTONS_VISIBLE)) {
            var models = model.get(CUSTOM_ACTION_BUTTONS);
            for (var actionButtonModel : models) {
                if (visFlipper.isCustomButtonToHide(actionButtonModel.get(TYPE))) continue;
                if (!maybeInflateAndPositionCustomButton(
                        view, actionButtonModel, posParams, defaultButtonWidth)) {
                    break;
                }
                visFlipper.addVisibleButtonType(actionButtonModel.get(TYPE));
            }
        }

        var minimizeButtonData = model.get(MINIMIZE_BUTTON);
        boolean minimizeButtonHidden = false;
        // Check if we have space for the minimize button and we should be showing it.
        if ((posParams.availableWidth >= defaultButtonWidth || visFlipper.canShowMinimizeButton())
                && minimizeButtonData.visible) {
            var minimizeButton = view.ensureMinimizeButtonInflated();

            if (minimizeButton.getDrawable() == null) {
                Context context = view.getContext();
                var d = UiUtils.getTintedDrawable(context, R.drawable.ic_minimize, model.get(TINT));
                minimizeButton.setTag(R.id.custom_tabs_toolbar_tintable, true);
                minimizeButton.setImageDrawable(d);
            }

            minimizeButton.setOnClickListener(minimizeButtonData.clickListener);
            minimizeButton.setOnLongClickListener(view);

            // The minimize button is always start aligned.
            positionButton(
                    minimizeButton,
                    posParams,
                    defaultButtonWidth,
                    defaultButtonHorizontalPadding,
                    /* isEndAligned= */ false);
        } else {
            // Set to true only when hidden due to width constraint.
            minimizeButtonHidden = !(posParams.availableWidth >= defaultButtonWidth);
            if (view.getMinimizeButton() != null) view.getMinimizeButton().setVisibility(View.GONE);
        }

        var optionalButton = view.getOptionalButton();
        if (optionalButton != null) {
            // TODO(https://crbug.com/455076202): Figure out when this happen.
            var parent = optionalButton.getParent();
            if (parent != null) {
                ((ViewGroup) parent).removeView(optionalButton);
            }
        }
        // Check if we have space for the optional button and we should be showing it. The optional
        // button is handled by its own MVC component, so it will have been inflated elsewhere.
        if (posParams.availableWidth >= defaultButtonWidth && model.get(OPTIONAL_BUTTON_VISIBLE)) {
            assertNonNull(optionalButton);
            optionalButton.setVisibility(View.VISIBLE);
            positionOptionalButton(
                    optionalButton, posParams, defaultButtonWidth, defaultButtonHorizontalPadding);
            customActionButtons.addView(optionalButton);
        } else if (optionalButton != null) {
            optionalButton.setVisibility(View.GONE);
        }

        // Check if we have space for the side-sheet maximize button we should be showing it.
        if (posParams.availableWidth >= defaultButtonWidth
                && model.get(SIDE_SHEET_MAXIMIZE_BUTTON).visible) {
            ImageButton sideSheetMaximizeButton = view.ensureSideSheetMaximizeButtonInflated();
            prepareSideSheetMaximizeButton(view, model);

            // The maximize button is currently end aligned.
            positionButton(
                    sideSheetMaximizeButton,
                    posParams,
                    defaultButtonWidth,
                    defaultButtonHorizontalPadding,
                    /* isEndAligned= */ true);
        } else if (view.getSideSheetMaximizeButton() != null) {
            view.getSideSheetMaximizeButton().setVisibility(View.GONE);
        }

        positionLocationBar(view, model, posParams);

        // Swap the position/padding of custom action / optional button. They were processed in
        // the order of priority above but their positions don't match, therefore should be swapped.
        maybeSwapCustomActionAndOptionalButtonPosition(view);

        if (minimizeButtonHidden && visFlipper.maybeFlipVisibility()) {
            // If button visibility got flipped, run this method again to reflect the change.
            inflateAndPositionToolbarElements(view, model, visFlipper);
        }
    }

    /**
     * Positions a button on the toolbar based on given arguments.
     *
     * @param button The button to be positioned.
     * @param posParams A {@link ButtonPositioningParams} tracking the current state of the
     *     positioning process. It will be modified by this method.
     * @param defaultButtonWidth The default width of a toolbar button.
     * @param defaultHorizontalPadding The default horizontal padding for a toolbar button.
     * @param isEndAligned True if the button is aligned to the end of the toolbar, false if aligned
     *     to the start.
     */
    private static void positionButton(
            View button,
            ButtonPositioningParams posParams,
            @Px int defaultButtonWidth,
            @Px int defaultHorizontalPadding,
            boolean isEndAligned) {
        // Adjust the layout gravity based on where the button is aligned, and offset it by
        // the total width of the buttons we've previously placed.
        setHorizontalLayoutParams(
                button,
                isEndAligned ? 0 : posParams.totalStartAlignedButtonWidth,
                isEndAligned ? posParams.totalEndAlignedButtonWidth : 0,
                isEndAligned);
        if (isEndAligned) {
            // We've placed a button at the end.
            posParams.totalEndAlignedButtonWidth += defaultButtonWidth;
        } else {
            // We've placed a button at the start.
            posParams.totalStartAlignedButtonWidth += defaultButtonWidth;
            posParams.spacingFromLastStartAlignedButton = defaultHorizontalPadding;
        }
        posParams.availableWidth -= defaultButtonWidth;
    }

    /**
     * Positions a custom action button on the toolbar based on given arguments.
     *
     * @param view The {@link CustomTabToolbar} that hosts the buttons.
     * @param model The custom action button {@link PropertyModel} containing the needed properties.
     * @param posParams A {@link ButtonPositioningParams} tracking the current state of the
     *     positioning process. It will be modified by this method.
     * @param defaultButtonWidth The default width of a toolbar button.
     */
    private static boolean maybeInflateAndPositionCustomButton(
            CustomTabToolbar view,
            PropertyModel model,
            ButtonPositioningParams posParams,
            @Px int defaultButtonWidth) {
        Drawable drawable = model.get(ICON);
        Resources resources = view.getResources();
        // The height will be scaled to match spec while keeping the aspect ratio, so get the scaled
        // width through that.
        int sourceHeight = drawable.getIntrinsicHeight();
        int sourceScaledHeight = resources.getDimensionPixelSize(R.dimen.toolbar_icon_height);
        int sourceWidth = drawable.getIntrinsicWidth();
        int sourceScaledWidth = sourceWidth * sourceScaledHeight / sourceHeight;
        int minPadding = resources.getDimensionPixelSize(R.dimen.min_toolbar_icon_side_padding);
        int horizontalPadding = Math.max((defaultButtonWidth - sourceScaledWidth) / 2, minPadding);
        int buttonWidth = sourceScaledWidth + 2 * horizontalPadding;

        if (buttonWidth > posParams.availableWidth) return false;

        ImageButton button =
                (ImageButton)
                        LayoutInflater.from(view.getContext())
                                .inflate(
                                        R.layout.custom_tabs_toolbar_button,
                                        view.getCustomActionButtonsParent(),
                                        false);
        button.setOnLongClickListener(view);
        button.setOnClickListener(model.get(CLICK_LISTENER));
        button.setContentDescription(model.get(DESCRIPTION));

        int topPadding = button.getPaddingTop();
        int bottomPadding = button.getPaddingBottom();
        button.setPadding(horizontalPadding, topPadding, horizontalPadding, bottomPadding);
        button.setImageDrawable(drawable);

        // Add the view at the beginning of the list. This isn't reflected in how the button is
        // positioned; it's only for keeping the index aligned with the params list.
        assumeNonNull(view.getCustomActionButtonsParent()).addView(button, 0);

        // Adjust the layout gravity based on where the button is aligned, and offset it by
        // the total width of the buttons we've previously placed.
        setHorizontalLayoutParams(
                button,
                /* startMargin= */ 0,
                posParams.totalEndAlignedButtonWidth,
                /* isEndAligned= */ true);
        // We've placed a button at the end.
        posParams.totalEndAlignedButtonWidth += buttonWidth;
        posParams.availableWidth -= buttonWidth;

        return true;
    }

    /**
     * Positions the location bar on the toolbar based on given arguments.
     *
     * @param view The {@link CustomTabToolbar} that hosts the buttons.
     * @param model The custom action button {@link PropertyModel} containing the needed properties.
     * @param posParams A {@link ButtonPositioningParams} tracking the current state of the
     *     positioning process. It will be modified by this method.
     */
    private static void positionLocationBar(
            CustomTabToolbar view, PropertyModel model, ButtonPositioningParams posParams) {
        var locationBar = view.findViewById(R.id.location_bar_frame_layout);
        Resources resources = view.getResources();
        var locationBarLp = ((ViewGroup.MarginLayoutParams) locationBar.getLayoutParams());
        locationBarLp.setMarginStart(posParams.totalStartAlignedButtonWidth);
        locationBarLp.setMarginEnd(posParams.totalEndAlignedButtonWidth);
        locationBar.setLayoutParams(locationBarLp);

        var titleUrlContainer = view.findViewById(R.id.title_url_container);
        var titleUrlLp = ((ViewGroup.MarginLayoutParams) titleUrlContainer.getLayoutParams());
        titleUrlLp.leftMargin = 0;
        boolean omniboxEnabled = model.get(OMNIBOX_ENABLED);
        if (omniboxEnabled) {
            // TODO(crbug.com/402213312): Revisit this when cleaning up CCTNestedSecurityIcon.
            // The security button is static when omnibox is enabled, so offset the url bar for it.
            int buttonWidth = resources.getDimensionPixelSize(R.dimen.toolbar_button_width);
            titleUrlLp.leftMargin += buttonWidth;
        }
        if (model.get(IS_INCOGNITO)) {
            int incognitoIconWidth =
                    resources.getDimensionPixelSize(R.dimen.custom_tabs_incognito_icon_width);
            titleUrlLp.leftMargin += incognitoIconWidth;
        }
        titleUrlContainer.setLayoutParams(titleUrlLp);

        // Ensure correct spacing between the last start aligned button and the location bar.
        int remainingSpace;
        if (omniboxEnabled) {
            remainingSpace =
                    resources.getDimensionPixelSize(
                            R.dimen.custom_tabs_url_bar_bg_horizontal_padding);
        } else {
            int desiredSpace =
                    resources.getDimensionPixelSize(R.dimen.custom_tabs_location_bar_start_spacing);
            remainingSpace =
                    Math.max(0, desiredSpace - posParams.spacingFromLastStartAlignedButton);
        }
        setHorizontalPadding(locationBar, remainingSpace, locationBar.getPaddingEnd());
    }

    /**
     * Positions the optional button on the toolbar based on given arguments.
     *
     * @param button The root of the optional button.
     * @param posParams A {@link ButtonPositioningParams} tracking the current state of the
     *     positioning process. It will be modified by this method.
     * @param defaultButtonWidth The default width of a toolbar button.
     * @param defaultHorizontalPadding The default horizontal padding for a toolbar button.
     */
    private static void positionOptionalButton(
            View button,
            ButtonPositioningParams posParams,
            @Px int defaultButtonWidth,
            @Px int defaultHorizontalPadding) {
        setOptionalButtonHorizontalPadding(
                button, defaultHorizontalPadding, defaultHorizontalPadding);

        // Adjust background padding to align it with the menu button.
        int paddingHori =
                getDimensionPx(button, R.dimen.custom_tabs_adaptive_button_bg_horizontal_padding);
        int paddingVert =
                getDimensionPx(button, R.dimen.custom_tabs_adaptive_button_bg_padding_vert);
        View background = button.findViewById(R.id.swappable_icon_secondary_background);
        background.setPaddingRelative(paddingHori, paddingVert, paddingHori, paddingVert);

        // Optional button is end aligned. Offset it by the total width of the buttons we've
        // previously placed.
        setHorizontalLayoutParams(button, 0, posParams.totalEndAlignedButtonWidth, true);
        // We've placed a button at the end.
        posParams.totalEndAlignedButtonWidth += defaultButtonWidth;
        posParams.availableWidth -= defaultButtonWidth;
    }

    private static void setHorizontalPadding(View view, @Px int startPadding, @Px int endPadding) {
        view.setPaddingRelative(
                startPadding, view.getPaddingTop(), endPadding, view.getPaddingBottom());
    }

    private static void setOptionalButtonHorizontalPadding(
            View button, @Px int startPadding, @Px int endPadding) {
        // Set the padding for the icon.
        View icon = button.findViewById(R.id.swappable_icon_animation_image);
        setHorizontalPadding(icon, startPadding, endPadding);

        // Set the padding for the menu button.
        View menu = button.findViewById(R.id.optional_toolbar_button);
        setHorizontalPadding(menu, startPadding, endPadding);
    }

    private static Pair<Integer, Integer> getOptionalButtonHorizontalPadding(View button) {
        View icon = button.findViewById(R.id.swappable_icon_animation_image);
        return Pair.create(icon.getPaddingStart(), icon.getPaddingEnd());
    }

    private static @Px int getDimensionPx(View v, @DimenRes int resId) {
        return v.getResources().getDimensionPixelSize(resId);
    }

    private static void setHorizontalLayoutParams(
            View view, @Px int startMargin, @Px int endMargin, boolean isEndAligned) {
        var lp = (FrameLayout.LayoutParams) view.getLayoutParams();
        lp.setMarginStart(startMargin);
        lp.setMarginEnd(endMargin);
        int horizontalGravity = isEndAligned ? Gravity.END : Gravity.START;
        lp.gravity = Gravity.CENTER_VERTICAL | horizontalGravity;
        view.setLayoutParams(lp);
    }

    private static void maybeSwapCustomActionAndOptionalButtonPosition(CustomTabToolbar view) {
        View optionalButton = view.getOptionalButton();
        FrameLayout customActionButtons = view.getCustomActionButtonsParent();
        if (optionalButton == null
                || optionalButton.getVisibility() != View.VISIBLE
                || assumeNonNull(customActionButtons).getChildCount() != 2) {
            return;
        }

        View customButton = customActionButtons.getChildAt(0);
        var padding = getOptionalButtonHorizontalPadding(optionalButton);
        int optionalStartPadding = padding.first;
        int optionalEndPadding = padding.second;
        int customStartPadding = customButton.getPaddingStart();
        int customEndPadding = customButton.getPaddingEnd();
        setOptionalButtonHorizontalPadding(optionalButton, customStartPadding, customEndPadding);
        setHorizontalPadding(customButton, optionalStartPadding, optionalEndPadding);

        var olp = (ViewGroup.MarginLayoutParams) optionalButton.getLayoutParams();
        var clp = (ViewGroup.MarginLayoutParams) customButton.getLayoutParams();
        int optionalEndMargin = olp.getMarginEnd();
        int customEndMargin = clp.getMarginEnd();
        setHorizontalLayoutParams(optionalButton, 0, customEndMargin, /* isEndAligned= */ true);
        setHorizontalLayoutParams(customButton, 0, optionalEndMargin, /* isEndAligned= */ true);
    }

    @Px
    static int getLocationBarMinWidth(
            Resources resources, boolean omniboxEnabled, boolean titleVisible) {
        int locationBarMinWidth =
                resources.getDimensionPixelSize(R.dimen.location_bar_min_url_width);
        if (omniboxEnabled) {
            locationBarMinWidth += resources.getDimensionPixelSize(R.dimen.toolbar_button_width);
        } else if (!titleVisible) {
            locationBarMinWidth +=
                    resources.getDimensionPixelSize(R.dimen.custom_tabs_security_icon_width);
        }
        return locationBarMinWidth;
    }

    private static void prepareSideSheetMaximizeButton(CustomTabToolbar view, PropertyModel model) {
        var data = model.get(SIDE_SHEET_MAXIMIZE_BUTTON);
        ImageButton button = view.findViewById(R.id.custom_tabs_sidepanel_maximize);
        assert button != null;
        button.setVisibility(View.VISIBLE);
        setSideSheetMaximizeButtonDrawable(button, data.maximized, model.get(TINT));
        button.setOnClickListener(v -> sideSheetMaximizeButtonCallback(model));
    }

    private static void sideSheetMaximizeButtonCallback(PropertyModel model) {
        var data = model.get(SIDE_SHEET_MAXIMIZE_BUTTON);
        var newData =
                new CustomTabToolbarButtonsProperties.SideSheetMaximizeButtonData(
                        data.visible, !data.maximized, data.callback);
        model.set(SIDE_SHEET_MAXIMIZE_BUTTON, newData);
        data.callback.onClick();
    }

    private static void setSideSheetMaximizeButtonDrawable(
            ImageButton button, boolean maximized, ColorStateList tint) {
        @DrawableRes
        int drawableId = maximized ? R.drawable.ic_fullscreen_exit : R.drawable.ic_fullscreen_enter;
        int buttonDescId =
                maximized
                        ? R.string.custom_tab_side_sheet_minimize
                        : R.string.custom_tab_side_sheet_maximize;

        var drawable = UiUtils.getTintedDrawable(button.getContext(), drawableId, tint);
        button.setImageDrawable(drawable);
        button.setContentDescription(button.getContext().getString(buttonDescId));
    }

    private static void updateAllButtonsTint(CustomTabToolbar view, ColorStateList tint) {
        // The menu button's tint is handled by its own MVC component.
        updateButtonTint(view.getCloseButton(), tint);
        updateButtonTint(view.getMinimizeButton(), tint);
        updateButtonTint(view.getSideSheetMaximizeButton(), tint);

        var actionButtons = view.getCustomActionButtonsParent();
        if (actionButtons != null) {
            for (int i = 0; i < actionButtons.getChildCount(); i++) {
                View actionButton = actionButtons.getChildAt(i);
                if (actionButton instanceof ImageButton button) {
                    updateButtonTint(button, tint);
                }
            }
        }
    }

    private static void updateButtonTint(@Nullable ImageButton button, ColorStateList tint) {
        if (button == null) return;

        Drawable drawable = button.getDrawable();
        if (drawable instanceof TintedDrawable tintedDrawable) {
            tintedDrawable.setTint(tint);
        } else if (button.getTag(R.id.custom_tabs_toolbar_tintable) != null) {
            drawable.setTintList(tint);
        }
    }
}
