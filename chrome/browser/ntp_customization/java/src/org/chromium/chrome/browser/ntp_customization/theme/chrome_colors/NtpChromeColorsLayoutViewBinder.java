// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.chrome_colors;

import static org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpChromeColorsProperties.BACKGROUND_COLOR_CIRCLE_VIEW_COLOR;
import static org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpChromeColorsProperties.BACKGROUND_COLOR_INPUT_TEXT_WATCHER;
import static org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpChromeColorsProperties.BACK_BUTTON_CLICK_LISTENER;
import static org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpChromeColorsProperties.CUSTOM_COLOR_PICKER_CONTAINER_VISIBILITY;
import static org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpChromeColorsProperties.DAILY_REFRESH_SWITCH_ON_CHECKED_CHANGE_LISTENER;
import static org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpChromeColorsProperties.IS_DAILY_REFRESH_SWITCH_CHECKED;
import static org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpChromeColorsProperties.LEARN_MORE_BUTTON_CLICK_LISTENER;
import static org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpChromeColorsProperties.PRIMARY_COLOR_CIRCLE_VIEW_COLOR;
import static org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpChromeColorsProperties.PRIMARY_COLOR_INPUT_TEXT_WATCHER;
import static org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpChromeColorsProperties.RECYCLER_VIEW_ADAPTER;
import static org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpChromeColorsProperties.RECYCLER_VIEW_ITEM_WIDTH;
import static org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpChromeColorsProperties.RECYCLER_VIEW_LAYOUT_MANAGER;
import static org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpChromeColorsProperties.RECYCLER_VIEW_MAX_WIDTH_PX;
import static org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpChromeColorsProperties.RECYCLER_VIEW_SPACING;
import static org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpChromeColorsProperties.SAVE_BUTTON_CLICK_LISTENER;

import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.view.View;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.ImageView;

import androidx.annotation.ColorInt;
import androidx.annotation.VisibleForTesting;
import androidx.constraintlayout.widget.ConstraintLayout;
import androidx.constraintlayout.widget.ConstraintSet;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.components.browser_ui.widget.MaterialSwitchWithText;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** View binder class for the NTP customization Chrome Colors bottom sheet. */
@NullMarked
public class NtpChromeColorsLayoutViewBinder {
    public static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        NtpChromeColorGridRecyclerView recyclerView =
                view.findViewById(R.id.chrome_colors_recycler_view);

        if (propertyKey == BACK_BUTTON_CLICK_LISTENER) {
            View backButton = view.findViewById(R.id.back_button);
            backButton.setOnClickListener(model.get(BACK_BUTTON_CLICK_LISTENER));
        } else if (propertyKey == LEARN_MORE_BUTTON_CLICK_LISTENER) {
            ImageView learnMoreButton = view.findViewById(R.id.learn_more_button);
            learnMoreButton.setOnClickListener(model.get(LEARN_MORE_BUTTON_CLICK_LISTENER));
        } else if (propertyKey == SAVE_BUTTON_CLICK_LISTENER) {
            ImageView saveButton = view.findViewById(R.id.save_button);
            if (saveButton != null) {
                saveButton.setOnClickListener(model.get(SAVE_BUTTON_CLICK_LISTENER));
            }
        } else if (propertyKey == BACKGROUND_COLOR_INPUT_TEXT_WATCHER) {
            EditText backgroundColorInput = view.findViewById(R.id.background_color_input);
            if (backgroundColorInput != null) {
                backgroundColorInput.addTextChangedListener(
                        model.get(BACKGROUND_COLOR_INPUT_TEXT_WATCHER));
            }
        } else if (propertyKey == PRIMARY_COLOR_INPUT_TEXT_WATCHER) {
            EditText primaryColorInput = view.findViewById(R.id.primary_color_input);
            if (primaryColorInput != null) {
                primaryColorInput.addTextChangedListener(
                        model.get(PRIMARY_COLOR_INPUT_TEXT_WATCHER));
            }
        } else if (propertyKey == BACKGROUND_COLOR_CIRCLE_VIEW_COLOR) {
            ImageView backgroundColorCircleView = view.findViewById(R.id.background_color_circle);
            updateColorCircle(
                    model.get(BACKGROUND_COLOR_CIRCLE_VIEW_COLOR), backgroundColorCircleView);
        } else if (propertyKey == PRIMARY_COLOR_CIRCLE_VIEW_COLOR) {
            ImageView primaryColorCircleView = view.findViewById(R.id.primary_color_circle);
            updateColorCircle(model.get(PRIMARY_COLOR_CIRCLE_VIEW_COLOR), primaryColorCircleView);
        } else if (propertyKey == CUSTOM_COLOR_PICKER_CONTAINER_VISIBILITY) {
            View containerView = view.findViewById(R.id.custom_color_picker_container);
            containerView.setVisibility(model.get(CUSTOM_COLOR_PICKER_CONTAINER_VISIBILITY));
        } else if (propertyKey == RECYCLER_VIEW_LAYOUT_MANAGER) {
            recyclerView.setLayoutManager(model.get(RECYCLER_VIEW_LAYOUT_MANAGER));
        } else if (propertyKey == RECYCLER_VIEW_ADAPTER) {
            recyclerView.setAdapter(model.get(RECYCLER_VIEW_ADAPTER));
        } else if (propertyKey == RECYCLER_VIEW_ITEM_WIDTH) {
            recyclerView.setItemWidth(model.get(RECYCLER_VIEW_ITEM_WIDTH));
        } else if (propertyKey == RECYCLER_VIEW_SPACING) {
            recyclerView.setSpacing(model.get(RECYCLER_VIEW_SPACING));
        } else if (propertyKey == RECYCLER_VIEW_MAX_WIDTH_PX) {
            if (view instanceof ConstraintLayout constraintLayout) {
                FrameLayout recyclerViewContainer =
                        view.findViewById(R.id.chrome_colors_recycler_view_container);
                setConstraintSet(
                        new ConstraintSet(),
                        constraintLayout,
                        recyclerViewContainer,
                        model.get(RECYCLER_VIEW_MAX_WIDTH_PX));
            }
        } else if (propertyKey == IS_DAILY_REFRESH_SWITCH_CHECKED) {
            MaterialSwitchWithText dailyRefreshSwitch =
                    view.findViewById(R.id.chrome_colors_switch_button);
            dailyRefreshSwitch.setChecked(model.get(IS_DAILY_REFRESH_SWITCH_CHECKED));
        } else if (propertyKey == DAILY_REFRESH_SWITCH_ON_CHECKED_CHANGE_LISTENER) {
            MaterialSwitchWithText dailyRefreshSwitch =
                    view.findViewById(R.id.chrome_colors_switch_button);
            dailyRefreshSwitch.setOnCheckedChangeListener(
                    model.get(DAILY_REFRESH_SWITCH_ON_CHECKED_CHANGE_LISTENER));
        }
    }

    private static void updateColorCircle(@ColorInt int color, ImageView circleImageView) {
        Drawable background = circleImageView.getBackground();
        if (background instanceof GradientDrawable) {
            ((GradientDrawable) background.mutate()).setColor(color);
            circleImageView.setVisibility(View.VISIBLE);
        }
    }

    @VisibleForTesting
    static void setConstraintSet(
            ConstraintSet constraintSet,
            ConstraintLayout constraintLayout,
            FrameLayout recyclerViewContainer,
            int maxWidthPx) {
        constraintSet.clone(constraintLayout);
        constraintSet.constrainedWidth(recyclerViewContainer.getId(), true);
        constraintSet.constrainMaxWidth(recyclerViewContainer.getId(), maxWidthPx);
        constraintSet.applyTo(constraintLayout);
    }
}
