// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.toolbar;

import static androidx.browser.customtabs.CustomTabsIntent.CLOSE_BUTTON_POSITION_END;

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
import static org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarButtonsProperties.SIDE_SHEET_MAXIMIZE_BUTTON;
import static org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarButtonsProperties.TITLE_VISIBLE;
import static org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarButtonsProperties.TOOLBAR_WIDTH;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.support.annotation.DrawableRes;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageButton;

import androidx.annotation.Px;

import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.customtabs.features.minimizedcustomtab.MinimizedFeatureUtils;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarButtonsProperties.SideSheetMaximizeButtonData;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.ui.UiUtils;
import org.chromium.ui.modelutil.ListModelChangeProcessor;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyListModel;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

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
        public int spacingFromLastEndAlignedButton;
    }

    @Override
    public void bind(PropertyModel model, CustomTabToolbar view, PropertyKey propertyKey) {
        inflateAndPositionToolbarElements(view, model);
    }

    @Override
    public void onItemsInserted(
            PropertyListModel<PropertyModel, PropertyKey> model,
            CustomTabToolbar view,
            int index,
            int count) {
        inflateAndPositionToolbarElements(view, (PropertyModel) view.getTag(R.id.view_model));
    }

    @Override
    public void onItemsRemoved(
            PropertyListModel<PropertyModel, PropertyKey> model,
            CustomTabToolbar view,
            int index,
            int count) {
        inflateAndPositionToolbarElements(view, (PropertyModel) view.getTag(R.id.view_model));
    }

    @Override
    public void onItemsChanged(
            PropertyListModel<PropertyModel, PropertyKey> model,
            CustomTabToolbar view,
            int index,
            int count,
            @Nullable PropertyKey payload) {
        for (int i = index; i < index + count; i++) {
            PropertyModel customButtonModel = model.get(i);
            view.updateCustomActionButton(
                    index, customButtonModel.get(ICON), customButtonModel.get(DESCRIPTION));
        }
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
     */
    private static void inflateAndPositionToolbarElements(
            CustomTabToolbar view, PropertyModel model) {
        var resources = view.getResources();
        int defaultButtonWidth = resources.getDimensionPixelSize(R.dimen.toolbar_button_width);
        int defaultIconWidth = resources.getDimensionPixelSize(R.dimen.toolbar_icon_default_width);
        int iconSpacing = resources.getDimensionPixelSize(R.dimen.custom_tabs_toolbar_icon_spacing);
        int availableWidth = model.get(TOOLBAR_WIDTH);
        int locationBarMinWidth =
                getLocationBarMinWidth(
                        view.getResources(), model.get(OMNIBOX_ENABLED), model.get(TITLE_VISIBLE));
        var posParams = new ButtonPositioningParams();
        posParams.availableWidth = availableWidth;

        if (model.get(IS_INCOGNITO)) {
            int incognitoIconWidth =
                    resources.getDimensionPixelSize(R.dimen.custom_tabs_incognito_icon_width);
            locationBarMinWidth += incognitoIconWidth;

            view.ensureIncognitoImageViewInflated();
        }

        posParams.availableWidth -= locationBarMinWidth;

        if (view.getCloseButton() == null && model.get(CLOSE_BUTTON).visible) {
            var closeButton = view.ensureCloseButtonInflated();
            closeButton.setImageDrawable(model.get(CLOSE_BUTTON).icon);
            closeButton.setOnLongClickListener(view);
        }

        var closeButton = view.getCloseButton();
        if (closeButton != null) {
            boolean isEndPosition = model.get(CLOSE_BUTTON).position == CLOSE_BUTTON_POSITION_END;
            positionButton(
                    closeButton,
                    posParams,
                    defaultButtonWidth,
                    iconSpacing,
                    defaultIconWidth,
                    isEndPosition);
        }

        if (view.getMenuButton() == null && model.get(MENU_BUTTON_VISIBLE)) {
            view.ensureMenuButtonInflated();
        }

        var menuButton = view.getMenuButton();
        if (menuButton != null) {
            boolean isEndPosition = model.get(CLOSE_BUTTON).position != CLOSE_BUTTON_POSITION_END;
            positionButton(
                    menuButton,
                    posParams,
                    defaultButtonWidth,
                    iconSpacing,
                    defaultIconWidth,
                    isEndPosition);
        }

        var minimizeButtonData = model.get(MINIMIZE_BUTTON);
        // Check if we have space for the minimize button and we should be showing it.
        if (posParams.availableWidth >= defaultButtonWidth && minimizeButtonData.visible) {
            var minimizeButton = view.ensureMinimizeButtonInflated();
            minimizeButton.setOnClickListener(minimizeButtonData.clickListener);
            Context context = view.getContext();
            var d =
                    UiUtils.getTintedDrawable(
                            context,
                            MinimizedFeatureUtils.getMinimizeIcon(),
                            ChromeColors.getPrimaryIconTint(context, model.get(IS_INCOGNITO)));
            minimizeButton.setTag(R.id.custom_tabs_toolbar_tintable, true);
            minimizeButton.setImageDrawable(d);
            minimizeButton.setOnLongClickListener(view);

            // The minimize button is always start aligned.
            positionButton(
                    minimizeButton,
                    posParams,
                    defaultButtonWidth,
                    iconSpacing,
                    defaultIconWidth,
                    /* isEndAligned= */ false);
        } else if (view.getMinimizeButton() != null) {
            view.getMinimizeButton().setVisibility(View.GONE);
        }

        // TODO(crbug.com/402213312): We need to think about how this should work with MTB.
        FrameLayout customActionButtons = view.getCustomActionButtonsParent();
        // TODO(crbug.com/402213312): Think of how we can optimize this so we don't reinflate all
        // buttons any time if we add/remove one.
        customActionButtons.removeAllViews();

        if (model.get(CUSTOM_ACTION_BUTTONS_VISIBLE)) {
            var models = model.get(CUSTOM_ACTION_BUTTONS);
            for (var actionButtonModel : models) {
                if (!maybeInflateAndPositionCustomButton(
                        view, actionButtonModel, posParams, defaultButtonWidth, iconSpacing)) {
                    break;
                }
            }
        }

        // Check if we have space for the side-sheet maximize button we should be showing it.
        if (posParams.availableWidth >= defaultButtonWidth
                && model.get(SIDE_SHEET_MAXIMIZE_BUTTON).visible) {
            view.ensureSideSheetMaximizeButtonInflated();
            var sideSheetMaximizeButtonData = model.get(SIDE_SHEET_MAXIMIZE_BUTTON);
            prepareSideSheetMaximizeButton(view, sideSheetMaximizeButtonData);

            // The maximize button is currently end aligned.
            positionButton(
                    view.getSideSheetMaximizeButton(),
                    posParams,
                    defaultButtonWidth,
                    iconSpacing,
                    defaultIconWidth,
                    /* isEndAligned= */ true);
        } else if (view.getSideSheetMaximizeButton() != null) {
            view.getSideSheetMaximizeButton().setVisibility(View.GONE);
        }

        positionLocationBar(view, model, posParams);
    }

    /**
     * Positions a button on the toolbar based on given arguments.
     *
     * @param button The button to be positioned.
     * @param posParams A {@link ButtonPositioningParams} tracking the current state of the
     *     positioning process. It will be modified by this method.
     * @param defaultButtonWidth The default width of a toolbar button.
     * @param iconSpacing The spacing between two adjacent icons.
     * @param iconWidth The width of the icon within the button.
     * @param isEndAligned True if the button is aligned to the end of the toolbar, false if aligned
     *     to the start.
     */
    private static void positionButton(
            View button,
            ButtonPositioningParams posParams,
            @Px int defaultButtonWidth,
            @Px int iconSpacing,
            int iconWidth,
            boolean isEndAligned) {
        int startPadding;
        int endPadding;
        // We calculate this button's padding based on the padding of the button that came before.
        if (isEndAligned) {
            assert posParams.spacingFromLastEndAlignedButton <= iconSpacing;
            // Remaining space to reach iconSpacing.
            endPadding = iconSpacing - posParams.spacingFromLastEndAlignedButton;
            // Remaining space to reach the default button width. If the button will be wider than
            // the default width because its icon is wider, make the start padding 0.
            startPadding = Math.max(0, defaultButtonWidth - iconWidth - endPadding);
            posParams.spacingFromLastEndAlignedButton = startPadding;
        } else {
            assert posParams.spacingFromLastStartAlignedButton <= iconSpacing;
            // Similar to the block above, just start and end padding are reversed.
            startPadding = iconSpacing - posParams.spacingFromLastStartAlignedButton;
            endPadding = Math.max(0, defaultButtonWidth - iconWidth - startPadding);
            posParams.spacingFromLastStartAlignedButton = endPadding;
        }
        int buttonWidth = iconWidth + startPadding + endPadding;
        setHorizontalPadding(button, startPadding, endPadding);
        // Adjust the layout gravity based on where the button is aligned, and offset it by
        // the total width of the buttons we've previously placed.
        setHorizontalLayoutParams(
                button,
                isEndAligned ? 0 : posParams.totalStartAlignedButtonWidth,
                isEndAligned ? posParams.totalEndAlignedButtonWidth : 0,
                isEndAligned);
        if (isEndAligned) {
            // We've placed a button at the end.
            posParams.totalEndAlignedButtonWidth += buttonWidth;
        } else {
            // We've placed a button at the start.
            posParams.totalStartAlignedButtonWidth += buttonWidth;
        }
        posParams.availableWidth -= buttonWidth;
    }

    /**
     * Positions a custom action button on the toolbar based on given arguments.
     *
     * @param view The {@link CustomTabToolbar} that hosts the buttons.
     * @param model The custom action button {@link PropertyModel} containing the needed properties.
     * @param posParams A {@link ButtonPositioningParams} tracking the current state of the
     *     positioning process. It will be modified by this method.
     * @param defaultButtonWidth The default width of a toolbar button.
     * @param iconSpacing The spacing between two adjacent icons.
     */
    private static boolean maybeInflateAndPositionCustomButton(
            CustomTabToolbar view,
            PropertyModel model,
            ButtonPositioningParams posParams,
            @Px int defaultButtonWidth,
            @Px int iconSpacing) {
        Drawable drawable = model.get(ICON);
        Resources resources = view.getResources();
        // The height will be scaled to match spec while keeping the aspect ratio, so get the scaled
        // width through that.
        int sourceHeight = drawable.getIntrinsicHeight();
        int sourceScaledHeight = resources.getDimensionPixelSize(R.dimen.toolbar_icon_height);
        int sourceWidth = drawable.getIntrinsicWidth();
        int sourceScaledWidth = sourceWidth * sourceScaledHeight / sourceHeight;

        // Remaining space to reach iconSpacing to make up to the required spacing.
        assert posParams.spacingFromLastEndAlignedButton <= iconSpacing;
        int endPadding = iconSpacing - posParams.spacingFromLastEndAlignedButton;
        // Remaining space to reach at least the default button width. If the button will be wider
        // than the default width because its icon is wider, make the start padding 0.
        int startPadding = Math.max(0, defaultButtonWidth - sourceScaledWidth - endPadding);
        int buttonWidth = sourceScaledWidth + startPadding + endPadding;

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

        int minPadding = resources.getDimensionPixelSize(R.dimen.min_toolbar_icon_side_padding);

        int sidePadding = Math.max((2 * sourceScaledHeight - sourceScaledWidth) / 2, minPadding);
        int topPadding = button.getPaddingTop();
        int bottomPadding = button.getPaddingBottom();
        button.setPadding(sidePadding, topPadding, sidePadding, bottomPadding);
        button.setImageDrawable(drawable);

        // Add the view at the beginning of the list. This isn't reflected in how the button is
        // positioned; it's only for keeping the index aligned with the params list.
        view.getCustomActionButtonsParent().addView(button, 0);
        positionButton(
                button,
                posParams,
                defaultButtonWidth,
                iconSpacing,
                sourceScaledWidth,
                /* isEndAligned= */ true);

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
        if (model.get(OMNIBOX_ENABLED)) {
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
        int desiredSpace =
                resources.getDimensionPixelSize(R.dimen.custom_tabs_location_bar_start_spacing);
        int remainingSpace =
                Math.max(0, desiredSpace - posParams.spacingFromLastStartAlignedButton);
        setHorizontalPadding(locationBar, remainingSpace, locationBar.getPaddingEnd());
    }

    private static void setHorizontalPadding(View view, @Px int startPadding, @Px int endPadding) {
        view.setPaddingRelative(
                startPadding, view.getPaddingTop(), endPadding, view.getPaddingBottom());
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

    @Px
    private static int getLocationBarMinWidth(
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

    private static void prepareSideSheetMaximizeButton(
            CustomTabToolbar view, SideSheetMaximizeButtonData data) {
        ImageButton button = view.findViewById(R.id.custom_tabs_sidepanel_maximize);
        if (button == null && data.visible) {
            LayoutInflater.from(view.getContext())
                    .inflate(R.layout.custom_tabs_sidepanel_maximize, view, true);
            button = view.findViewById(R.id.custom_tabs_sidepanel_maximize);
        }

        if (button == null) return;
        if (!data.visible) {
            button.setVisibility(View.GONE);
            return;
        }

        button.setVisibility(View.VISIBLE);
        boolean maximized = data.maximized;
        var callback = data.callback;
        button.setOnClickListener(
                v -> setSideSheetMaximizeButtonDrawable((ImageButton) v, callback.onClick()));
        setSideSheetMaximizeButtonDrawable(button, maximized);
    }

    private static void setSideSheetMaximizeButtonDrawable(ImageButton button, boolean maximized) {
        @DrawableRes
        int drawableId = maximized ? R.drawable.ic_fullscreen_exit : R.drawable.ic_fullscreen_enter;
        int buttonDescId =
                maximized
                        ? R.string.custom_tab_side_sheet_minimize
                        : R.string.custom_tab_side_sheet_maximize;
        var drawable =
                UiUtils.getTintedDrawable(
                        button.getContext(),
                        drawableId,
                        ChromeColors.getPrimaryIconTint(button.getContext(), false));
        button.setImageDrawable(drawable);
        button.setContentDescription(button.getContext().getString(buttonDescId));
    }
}
