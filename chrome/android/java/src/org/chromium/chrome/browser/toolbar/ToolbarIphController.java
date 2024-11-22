// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.content.Context;
import android.graphics.Rect;
import android.view.View;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.user_education.IphCommandBuilder;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightParams;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightShape;
import org.chromium.components.feature_engagement.FeatureConstants;

/** Controller manage when an IPH bubble for Toolbar is shown. */
public class ToolbarIphController {
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
        int yInset =
                mContext.getResources()
                        .getDimensionPixelOffset(
                                R.dimen.price_drop_spotted_iph_ntp_tabswitcher_y_inset);
        mEducationHelper.requestShowIph(
                new IphCommandBuilder(
                                mContext.getResources(),
                                FeatureConstants.PRICE_DROP_NTP_FEATURE,
                                R.string.price_drop_spotted_iph,
                                R.string.price_drop_spotted_iph)
                        .setInsetRect(new Rect(0, 0, 0, -yInset))
                        .setAnchorView(anchorView)
                        .setHighlightParams(params)
                        .setDismissOnTouch(true)
                        .build());
    }
}
