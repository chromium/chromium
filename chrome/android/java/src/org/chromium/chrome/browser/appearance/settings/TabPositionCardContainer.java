// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.appearance.settings;

import android.content.Context;
import android.content.res.Resources;
import android.util.AttributeSet;
import android.view.View;
import android.widget.LinearLayout;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.containment.ContainmentViewStyler;

/**
 * A responsive container for tab position cards that switches between horizontal (side-by-side) and
 * vertical (stacked) layouts based on available width, styling each option as its own container.
 */
@NullMarked
public class TabPositionCardContainer extends LinearLayout {
    /**
     * Arrangement the children are currently configured for. Defaults to HORIZONTAL to match the
     * inflated XML layout.
     */
    private int mAppliedOrientation = HORIZONTAL;

    private @Nullable View mHorizontalOption;
    private @Nullable View mVerticalOption;

    /** Constructs a new {@link TabPositionCardContainer}. */
    public TabPositionCardContainer(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mHorizontalOption = findViewById(R.id.tab_position_horizontal_option);
        mVerticalOption = findViewById(R.id.tab_position_vertical_option);
        updateChildLayouts(/* sideBySide= */ true);
    }

    @Override
    public void onRtlPropertiesChanged(int layoutDirection) {
        super.onRtlPropertiesChanged(layoutDirection);
        applyCardBackgrounds(mAppliedOrientation == HORIZONTAL);
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        int widthMode = MeasureSpec.getMode(widthMeasureSpec);
        int widthPx = MeasureSpec.getSize(widthMeasureSpec);

        if (widthMode != MeasureSpec.UNSPECIFIED && widthPx > 0) {
            int breakpointPx =
                    getResources()
                            .getDimensionPixelSize(
                                    R.dimen.tab_position_card_container_width_breakpoint);

            int targetOrientation = (widthPx >= breakpointPx) ? HORIZONTAL : VERTICAL;
            if (mAppliedOrientation != targetOrientation) {
                mAppliedOrientation = targetOrientation;
                updateChildLayouts(targetOrientation == HORIZONTAL);
                setOrientation(targetOrientation);
            }
        }

        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
    }

    /**
     * Updates children layout parameters, margins, and container backgrounds for the given
     * arrangement.
     */
    private void updateChildLayouts(boolean sideBySide) {
        if (mHorizontalOption == null || mVerticalOption == null) {
            return;
        }

        int gapPx = getResources().getDimensionPixelSize(R.dimen.tab_position_card_gap);

        applyOptionParams(mHorizontalOption, sideBySide, /* isFirst= */ true, gapPx);
        applyOptionParams(mVerticalOption, sideBySide, /* isFirst= */ false, gapPx);
        applyCardBackgrounds(sideBySide);
    }

    /**
     * Sizes an option card for the current arrangement: weighted halves separated by a horizontal
     * gap when side by side, or full width separated by a vertical gap when stacked.
     */
    private void applyOptionParams(View option, boolean sideBySide, boolean isFirst, int gapPx) {
        LayoutParams params = (LayoutParams) option.getLayoutParams();
        params.width = sideBySide ? 0 : LayoutParams.MATCH_PARENT;
        params.weight = sideBySide ? 1.0f : 0f;
        params.setMarginEnd(isFirst && sideBySide ? gapPx : 0);
        params.bottomMargin = isFirst && !sideBySide ? gapPx : 0;
        option.setLayoutParams(params);
    }

    private void applyCardBackgrounds(boolean sideBySide) {
        if (mHorizontalOption == null || mVerticalOption == null) {
            return;
        }

        Resources res = getResources();
        float outerRadius =
                res.getDimensionPixelSize(R.dimen.settings_item_rounded_corner_radius_default);
        float innerRadius =
                res.getDimensionPixelSize(R.dimen.settings_item_rounded_corner_radius_inner);
        int backgroundColor = SemanticColorUtils.getSettingsContainerBackgroundColor(getContext());

        float[] firstRadii =
                sideBySide
                        ? ContainmentViewStyler.cornerRadii(
                                outerRadius, innerRadius, innerRadius, outerRadius)
                        : ContainmentViewStyler.cornerRadii(
                                outerRadius, outerRadius, innerRadius, innerRadius);
        float[] secondRadii =
                sideBySide
                        ? ContainmentViewStyler.cornerRadii(
                                innerRadius, outerRadius, outerRadius, innerRadius)
                        : ContainmentViewStyler.cornerRadii(
                                innerRadius, innerRadius, outerRadius, outerRadius);

        boolean swapForRtl = sideBySide && getLayoutDirection() == LAYOUT_DIRECTION_RTL;
        setCardBackground(
                swapForRtl ? mVerticalOption : mHorizontalOption, firstRadii, backgroundColor);
        setCardBackground(
                swapForRtl ? mHorizontalOption : mVerticalOption, secondRadii, backgroundColor);
    }

    /**
     * Applies the shared settings container background, including hover and ripple states, so the
     * cards match every other container in Settings.
     */
    private void setCardBackground(View card, float[] radii, int backgroundColor) {
        card.setBackground(
                ContainmentViewStyler.createInteractiveRoundedDrawable(
                        getContext(), radii, backgroundColor));
    }
}
