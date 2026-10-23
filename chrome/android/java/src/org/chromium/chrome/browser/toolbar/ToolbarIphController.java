// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.content.Context;
import android.graphics.Rect;
import android.view.View;

import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ui.bottombar.BottomBarMetrics;
import org.chromium.chrome.browser.ui.bottombar.BottomBarMetrics.IphEvent;
import org.chromium.chrome.browser.ui.bottombar.BottomBarMetrics.IphFeature;
import org.chromium.chrome.browser.user_education.IphCommandBuilder;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightParams;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightShape;
import org.chromium.components.feature_engagement.FeatureConstants;

/** Controller manage when an IPH bubble for Toolbar is shown. */
@NullMarked
public class ToolbarIphController {

    // 2 seconds.
    private static final long MIN_DELAY_BEFORE_TOUCH_DISMISS_MS = 2000;
    // 5 seconds.
    @VisibleForTesting static final int GLIC_IPH_AUTO_DISMISS_TIMEOUT_MS = 5 * 1000;

    private final Context mContext;
    private final UserEducationHelper mEducationHelper;

    ToolbarIphController(Context context, UserEducationHelper educationHelper) {
        mContext = context;
        mEducationHelper = educationHelper;
    }

    /**
     * Attempts to show the price drop in-product help bubble.
     *
     * @param anchorView The view to anchor the IPH bubble on.
     */
    @VisibleForTesting
    public void showPriceDropIph(View anchorView) {
        HighlightParams params = new HighlightParams(HighlightShape.CIRCLE);
        params.setBoundsRespectPadding(true);
    }

    /**
     * Attempts to show the bottom toolbar in-product help bubble.
     *
     * @param anchorView The view to anchor the IPH bubble on.
     */
    @VisibleForTesting
    public void showBottomToolbarIph(View anchorView) {
        showAddressBarIph(
                anchorView,
                FeatureConstants.BOTTOM_TOOLBAR_FEATURE,
                R.string.toolbar_long_press_options_iph,
                /* onShowCallback= */ null,
                /* onDismissCallback= */ null);
    }

    /**
     * Attempts to show the bottom bar education address bar in-product help bubble.
     *
     * @param anchorView The view to anchor the IPH bubble on.
     * @param onShownCallback Callback invoked when the IPH bubble is shown.
     */
    public void showBottomBarAddressBarIph(View anchorView, Runnable onShownCallback) {
        showAddressBarIph(
                anchorView,
                FeatureConstants.ANDROID_BOTTOM_BAR_ADDRESS_BAR,
                R.string.bottom_bar_education_tooltip_address_bar,
                () -> {
                    BottomBarMetrics.recordIphEvent(IphEvent.SHOWN, IphFeature.ADDRESS_BAR);
                    onShownCallback.run();
                },
                () -> BottomBarMetrics.recordIphEvent(IphEvent.DISMISSED, IphFeature.ADDRESS_BAR));
    }

    private void showAddressBarIph(
            View anchorView,
            @FeatureConstants String featureName,
            @StringRes int stringId,
            @Nullable Runnable onShowCallback,
            @Nullable Runnable onDismissCallback) {
        int yInset = mContext.getResources().getDimensionPixelOffset(R.dimen.toolbar_iph_y_inset);
        IphCommandBuilder builder =
                new IphCommandBuilder(mContext.getResources(), featureName, stringId, stringId)
                        .setInsetRect(new Rect(0, 0, 0, yInset))
                        .setAnchorView(anchorView)
                        .setDelayedDismissOnTouch(MIN_DELAY_BEFORE_TOUCH_DISMISS_MS);
        if (onShowCallback != null) {
            builder.setOnShowCallback(onShowCallback);
        }
        if (onDismissCallback != null) {
            builder.setOnDismissCallback(onDismissCallback);
        }
        mEducationHelper.requestShowIph(builder.build());
    }

    /**
     * Attempts to show the Glic Promo in-product help bubble.
     *
     * @param anchorView The view to anchor the IPH bubble on.
     */
    public void showGlicIph(View anchorView) {
        mEducationHelper.requestShowIph(
                new IphCommandBuilder(
                                mContext.getResources(),
                                FeatureConstants.GLIC_PROMO_ANDROID_FEATURE,
                                R.string.iph_glic_promo_text,
                                R.string.iph_glic_promo_accessibility_text)
                        .setAnchorView(anchorView)
                        .setDismissOnTouch(true)
                        .setAutoDismissTimeout(GLIC_IPH_AUTO_DISMISS_TIMEOUT_MS)
                        .build());
    }
}
