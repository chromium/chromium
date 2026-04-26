// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.actions;

import android.content.res.Resources;
import android.view.View;

import androidx.annotation.DrawableRes;
import androidx.annotation.StringRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ui.actions.button.ButtonState;
import org.chromium.ui.modelutil.PropertyModel;

/** Helper methods for working with Action buttons. */
@NullMarked
public class ActionUtils {

    // Avoid instantiation.
    private ActionUtils() {}

    /**
     * Applies the given {@link ButtonState} to the view. This is currently unrelated to the view's
     * visibility state, and only affects the view's alpha, clickable, and enabled states.
     *
     * @param view The view to apply the state to.
     * @param state The {@link ButtonState} to apply.
     * @param canPress Whether the button can be pressed.
     */
    /* package */ static void applyButtonState(
            View view, @ButtonState int state, boolean canPress) {
        if (state == ButtonState.DEFAULT) {
            view.setAlpha(1.0f);
            view.setClickable(canPress);
            view.setEnabled(canPress);
        } else if (state == ButtonState.INVISIBLE_AND_CLICKABLE) {
            view.setAlpha(0.0f);
            view.setClickable(true);
            view.setEnabled(true);
        } else if (state == ButtonState.UNCLICKABLE) {
            view.setAlpha(1.0f);
            view.setClickable(false);
            view.setEnabled(true);
        }
    }

    /**
     * Registers bottom bar actions. The idea is to register all visual attributes of the button as
     * soon as the app starts, then update the behavioural properties at a later time.
     *
     * @param registry The {@link ActionRegistry} to register actions to.
     */
    public static void registerBottomBarActions(ActionRegistry registry) {
        // Register new tab.
        registerAction(
                registry,
                ActionId.NEW_TAB,
                R.drawable.new_tab_icon,
                R.string.button_new_tab,
                R.string.new_tab_title);
    }

    /**
     * Registers an action in the registry.
     *
     * @param registry The {@link ActionRegistry} to register to.
     * @param actionId The ID of the action.
     * @param iconResId The drawable resource ID for the icon.
     * @param contentDescriptionResId The string resource ID for the content description.
     * @param tooltipResId The string resource ID for the tooltip.
     */
    public static void registerAction(
            ActionRegistry registry,
            @ActionId int actionId,
            @DrawableRes int iconResId,
            @StringRes int contentDescriptionResId,
            @StringRes int tooltipResId) {
        PropertyModel model = createActionModel(iconResId, contentDescriptionResId, tooltipResId);
        registry.register(actionId, model);
    }

    /**
     * Creates a property model for an action.
     *
     * @param iconResId The drawable resource ID for the icon.
     * @param contentDescriptionResId The string resource ID for the content description.
     * @param tooltipResId The string resource ID for the tooltip.
     * @return The constructed {@link PropertyModel}.
     */
    public static PropertyModel createActionModel(
            @DrawableRes int iconResId,
            @StringRes int contentDescriptionResId,
            @StringRes int tooltipResId) {
        PropertyModel.Builder builder = new PropertyModel.Builder(ActionProperties.BASE_KEYS);
        if (iconResId != Resources.ID_NULL) {
            builder.with(ActionProperties.ICON_ID, iconResId);
        }
        if (contentDescriptionResId != Resources.ID_NULL) {
            builder.with(
                    ActionProperties.CONTENT_DESCRIPTION_RESOLVER,
                    new ResourceTextResolver(contentDescriptionResId));
        }
        if (tooltipResId != Resources.ID_NULL) {
            builder.with(
                    ActionProperties.TOOLTIP_TEXT_RESOLVER, new ResourceTextResolver(tooltipResId));
        }
        return builder.build();
    }
}
