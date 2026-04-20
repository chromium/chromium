// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import static org.chromium.chrome.browser.tab_bottom_sheet.TabBottomSheetProperties.PEEK_STATE_ALPHA;

import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.context_sharing.R;
import org.chromium.chrome.browser.tab_bottom_sheet.TabBottomSheetProperties.ResizingState;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** ViewBinder for the Tab Bottom Sheet. */
@NullMarked
public class TabBottomSheetViewBinder {
    /**
     * Binds the given model to the given view.
     *
     * @param model The {@link PropertyModel} to bind.
     * @param view The inflated Android {@link View} of the promo sheet.
     * @param propertyKey The {@link PropertyKey} that changed.
     */
    public static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey == TabBottomSheetProperties.RESIZING_STATE) {
            ResizingState resizingState = model.get(TabBottomSheetProperties.RESIZING_STATE);
            CoBrowseViews coBrowseViews = model.get(TabBottomSheetProperties.BOTTOM_SHEET_VIEWS);
            coBrowseViews.setResizingState(resizingState);
        } else if (propertyKey == TabBottomSheetProperties.IS_RESIZING) {
            CoBrowseViews coBrowseViews = model.get(TabBottomSheetProperties.BOTTOM_SHEET_VIEWS);
            ResizingState resizingState = model.get(TabBottomSheetProperties.RESIZING_STATE);
            if (coBrowseViews != null && resizingState != null) {
                coBrowseViews.setIsResizing(model.get(TabBottomSheetProperties.IS_RESIZING));
            }
        } else if (PEEK_STATE_ALPHA == propertyKey) {
            float alpha = model.get(PEEK_STATE_ALPHA);
            View peekContainer = view.findViewById(R.id.actor_control_container);
            View expandedContent = view.findViewById(R.id.expanded_content_group);
            peekContainer.setAlpha(alpha);
            peekContainer.setVisibility(alpha == 0.0f ? View.GONE : View.VISIBLE);
            expandedContent.setAlpha(1.0f - alpha);
            expandedContent.setVisibility(alpha == 1.0f ? View.GONE : View.VISIBLE);
        }
    }
}
