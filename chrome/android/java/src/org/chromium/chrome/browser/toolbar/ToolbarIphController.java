// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.content.Context;
import android.graphics.Rect;
import android.view.View;

import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
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

    private final Context mContext;
    private final UserEducationHelper mEducationHelper;

    ToolbarIphController(Context context, UserEducationHelper educationHelper) {
        mContext = context;
        mEducationHelper = educationHelper;
    }

    @VisibleForTesting
    public void showPriceDropIph(View anchorView) {
        HighlightParams params = new HighlightParams(HighlightShape.CIRCLE);
        params.setBoundsRespectPadding(true);
    }

    @VisibleForTesting
    public void showBottomToolbarIph(View anchorView) {
        int yInset = mContext.getResources().getDimensionPixelOffset(R.dimen.toolbar_iph_y_inset);
        mEducationHelper.requestShowIph(
                new IphCommandBuilder(
                                mContext.getResources(),
                                FeatureConstants.BOTTOM_TOOLBAR_FEATURE,
                                R.string.toolbar_long_press_options_iph,
                                R.string.toolbar_long_press_options_iph)
                        .setInsetRect(new Rect(0, 0, 0, yInset))
                        .setAnchorView(anchorView)
                        .setDelayedDismissOnTouch(MIN_DELAY_BEFORE_TOUCH_DISMISS_MS)
                        .build());
    }
}
