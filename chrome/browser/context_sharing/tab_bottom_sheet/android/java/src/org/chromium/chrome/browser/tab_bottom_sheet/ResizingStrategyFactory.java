// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/** Factory for creating {@link ResizingStrategy} instances. */
@NullMarked
public class ResizingStrategyFactory {
    public static final String RESIZING_STRATEGY_PARAM = "resizing_strategy";
    public static final String STRATEGY_DRAG_DIRECTION = "drag_direction";

    /**
     * Creates a strategy for managing resizing mode on {@link WebViewResizingHelper}.
     *
     * @param helper The {@link WebViewResizingHelper} to control resizing mode on.
     */
    public static ResizingStrategy create(WebViewResizingHelper helper) {
        String strategyParam =
                ChromeFeatureList.getFieldTrialParamByFeature(
                        ChromeFeatureList.TAB_BOTTOM_SHEET_RESIZE_WEBVIEW, RESIZING_STRATEGY_PARAM);
        if (STRATEGY_DRAG_DIRECTION.equals(strategyParam)) {
            return new DragDirectionResizingStrategy(helper);
        }
        return new DefaultResizingStrategy(helper);
    }
}
