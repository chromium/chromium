// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.chromium.chrome.browser.hub.HubAnimationConstants.PANE_COLOR_BLEND_ANIMATION_DURATION_MS;

import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.graphics.Rect;
import android.view.TouchDelegate;
import android.view.ViewGroup;
import android.widget.Button;

import androidx.annotation.ColorInt;
import androidx.annotation.Px;
import androidx.core.widget.TextViewCompat;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.actions.button.FullButtonData;

/** Helper class for Hub action button operations. */
@NullMarked
public class HubActionButtonHelper {

    /** Sets button data for the action button. */
    public static void setButtonData(Button button, @Nullable FullButtonData buttonData) {
        ApplyButtonData.apply(buttonData, button);
        button.setText(null);
        button.setCompoundDrawablePadding(0);
        if (HubUtils.isGtsUpdateEnabled()) {
            Resources resources = button.getResources();
            @Px
            int paddingLR =
                    resources.getDimensionPixelSize(R.dimen.hub_toolbar_action_button_padding_lr);
            button.setPadding(paddingLR, 0, paddingLR, 0);

            @Px
            int buttonSize =
                    resources.getDimensionPixelSize(R.dimen.hub_toolbar_action_button_size);

            ViewGroup.LayoutParams params = (ViewGroup.LayoutParams) button.getLayoutParams();
            params.width = buttonSize;
            params.height = buttonSize;

            button.setLayoutParams(params);
        }
    }

    /** Sets up color mixer for the action button. */
    public static void setColorMixer(Button button, @Nullable HubColorMixer mixer) {
        HubColorMixerRegistrationHelper helper =
                (HubColorMixerRegistrationHelper) button.getTag(R.id.hub_color_mixer_helper);
        if (helper == null) {
            helper = new HubColorMixerRegistrationHelper();
            button.setTag(R.id.hub_color_mixer_helper, helper);

            Context context = button.getContext();
            boolean isGtsUpdateEnabled = HubUtils.isGtsUpdateEnabled();
            if (isGtsUpdateEnabled) {
                helper.registerBlend(
                        new SingleHubViewColorBlend(
                                PANE_COLOR_BLEND_ANIMATION_DURATION_MS,
                                colorScheme ->
                                        HubColors.getToolbarActionButtonIconColor(
                                                context, colorScheme),
                                color ->
                                        updateActionButtonIconColorInternal(
                                                button, context, color)));

                helper.registerBlend(
                        new SingleHubViewColorBlend(
                                PANE_COLOR_BLEND_ANIMATION_DURATION_MS,
                                colorScheme ->
                                        HubColors.getToolbarActionButtonBackgroundColor(
                                                context, colorScheme),
                                color -> updateActionButtonColorInternal(button, context, color)));

                helper.registerBlend(
                        new SingleHubViewColorBlend(
                                PANE_COLOR_BLEND_ANIMATION_DURATION_MS,
                                colorScheme ->
                                        HubColors.getToolbarActionButtonFocusColor(
                                                context, colorScheme),
                                color -> updateActionButtonFocusColorInternal(button, color)));
            } else {
                helper.registerBlend(
                        new SingleHubViewColorBlend(
                                PANE_COLOR_BLEND_ANIMATION_DURATION_MS,
                                colorScheme -> HubColors.getIconColor(context, colorScheme),
                                interpolatedColor -> {
                                    updateActionButtonIconColorInternal(
                                            button, context, interpolatedColor);
                                }));
            }
        }

        helper.setColorMixer(mixer);
    }

    /** Updates action button icon color. */
    private static void updateActionButtonIconColorInternal(
            Button button, Context context, @ColorInt int color) {
        ColorStateList actionButtonColor =
                HubColors.getActionButtonColor(context, color, HubUtils.isGtsUpdateEnabled());
        TextViewCompat.setCompoundDrawableTintList(button, actionButtonColor);
    }

    /** Updates action button background color. */
    private static void updateActionButtonColorInternal(
            Button button, Context context, @ColorInt int color) {
        ColorStateList actionButtonBgColor = HubColors.getActionButtonBgColor(context, color);
        button.setBackgroundTintList(actionButtonBgColor);
    }

    /** Updates action button focus stroke color. */
    private static void updateActionButtonFocusColorInternal(Button button, @ColorInt int color) {
        ColorStateList colorStateList = HubColors.generateFocusStrokeColorStateList(color);
        button.setForegroundTintList(colorStateList);
    }

    /** Creates touch delegate for the action button. */
    public static TouchDelegate createTouchDelegate(Button button) {
        Rect rect = new Rect();
        button.getHitRect(rect);

        @Px
        int touchSize =
                button.getContext()
                        .getResources()
                        .getDimensionPixelSize(R.dimen.min_touch_target_size);
        int halfWidthDelta = Math.max((touchSize - button.getWidth()) / 2, 0);
        int halfHeightDelta = Math.max((touchSize - button.getHeight()) / 2, 0);

        rect.left -= halfWidthDelta;
        rect.right += halfWidthDelta;
        rect.top -= halfHeightDelta;
        rect.bottom += halfHeightDelta;

        return new TouchDelegate(rect, button);
    }
}
