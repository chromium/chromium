// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.chromium.chrome.browser.hub.HubAnimationConstants.PANE_COLOR_BLEND_ANIMATION_DURATION_MS;

import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.graphics.Rect;
import android.util.AttributeSet;
import android.view.Gravity;
import android.view.TouchDelegate;
import android.view.View;
import android.widget.Button;
import android.widget.FrameLayout;

import androidx.annotation.ColorInt;
import androidx.core.widget.TextViewCompat;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Action button view for the Hub toolbar. Displays the primary pane action button. */
@NullMarked
public class HubActionButtonView extends Button {
    /** Default {@link Button} constructor called by inflation. */
    public HubActionButtonView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        super.onLayout(changed, left, top, right, bottom);
        View parent = (View) getParent();
        if (parent != null) {
            parent.setTouchDelegate(getToolbarActionButtonDelegate());
        }
    }

    void setButtonData(@Nullable FullButtonData buttonData) {
        ApplyButtonData.apply(buttonData, this);
        setText(null);
        setCompoundDrawablePadding(0);

        if (HubUtils.isGtsUpdateEnabled()) {
            Resources resources = getResources();
            int paddingLR =
                    resources.getDimensionPixelSize(R.dimen.hub_toolbar_action_button_padding_lr);
            setPadding(paddingLR, 0, paddingLR, 0);

            int buttonSize =
                    resources.getDimensionPixelSize(R.dimen.hub_toolbar_action_button_size);
            int startMarginPx =
                    resources.getDimensionPixelSize(R.dimen.hub_toolbar_action_button_start_margin);

            FrameLayout.LayoutParams params = (FrameLayout.LayoutParams) getLayoutParams();
            params.setMarginStart(startMarginPx);
            params.width = buttonSize;
            params.height = buttonSize;
            params.gravity = Gravity.START | Gravity.CENTER_VERTICAL;
            setLayoutParams(params);
        }
    }

    void setColorMixer(HubColorMixer mixer) {
        Context context = getContext();
        boolean isGtsUpdateEnabled = HubUtils.isGtsUpdateEnabled();
        if (isGtsUpdateEnabled) {
            mixer.registerBlend(
                    new SingleHubViewColorBlend(
                            PANE_COLOR_BLEND_ANIMATION_DURATION_MS,
                            colorScheme ->
                                    HubColors.getToolbarActionButtonIconColor(context, colorScheme),
                            color -> updateActionButtonIconColorInternal(context, color)));

            mixer.registerBlend(
                    new SingleHubViewColorBlend(
                            PANE_COLOR_BLEND_ANIMATION_DURATION_MS,
                            colorScheme ->
                                    HubColors.getToolbarActionButtonBackgroundColor(
                                            context, colorScheme),
                            color -> updateActionButtonColorInternal(context, color)));
        } else {
            mixer.registerBlend(
                    new SingleHubViewColorBlend(
                            PANE_COLOR_BLEND_ANIMATION_DURATION_MS,
                            colorScheme -> HubColors.getIconColor(context, colorScheme),
                            interpolatedColor -> {
                                updateActionButtonIconColorInternal(context, interpolatedColor);
                            }));
        }
    }

    private void updateActionButtonIconColorInternal(Context context, @ColorInt int color) {
        ColorStateList actionButtonColor =
                HubColors.getActionButtonColor(context, color, HubUtils.isGtsUpdateEnabled());
        TextViewCompat.setCompoundDrawableTintList(this, actionButtonColor);
    }

    private void updateActionButtonColorInternal(Context context, @ColorInt int color) {
        ColorStateList actionButtonBgColor = HubColors.getActionButtonBgColor(context, color);
        setBackgroundTintList(actionButtonBgColor);
    }

    private TouchDelegate getToolbarActionButtonDelegate() {
        Rect rect = new Rect();
        getHitRect(rect);

        int touchSize =
                getContext().getResources().getDimensionPixelSize(R.dimen.min_touch_target_size);
        int halfWidthDelta = Math.max((touchSize - getWidth()) / 2, 0);
        int halfHeightDelta = Math.max((touchSize - getHeight()) / 2, 0);

        rect.left -= halfWidthDelta;
        rect.right += halfWidthDelta;
        rect.top -= halfHeightDelta;
        rect.bottom += halfHeightDelta;

        return new TouchDelegate(rect, this);
    }
}
