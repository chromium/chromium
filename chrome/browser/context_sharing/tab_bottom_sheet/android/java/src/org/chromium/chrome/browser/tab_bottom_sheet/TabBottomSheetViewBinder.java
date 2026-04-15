// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import static org.chromium.chrome.browser.tab_bottom_sheet.TabBottomSheetProperties.PEEK_VIEW_AND_EXPANDED_CONTENT_ALPHA;
import static org.chromium.chrome.browser.tab_bottom_sheet.TabBottomSheetProperties.PEEK_VIEW_AND_EXPANDED_CONTENT_VISIBILITY;

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
        } else if (propertyKey == TabBottomSheetProperties.PLACEHOLDER_BACKGROUND_COLOR) {
            CoBrowseViews coBrowseViews = model.get(TabBottomSheetProperties.BOTTOM_SHEET_VIEWS);
            if (coBrowseViews != null) {
                coBrowseViews.setPlaceholderBackgroundColor(
                        model.get(TabBottomSheetProperties.PLACEHOLDER_BACKGROUND_COLOR));
            }
        } else if (PEEK_VIEW_AND_EXPANDED_CONTENT_ALPHA == propertyKey) {
            View peekContainer = view.findViewById(R.id.actor_control_container);
            peekContainer.setAlpha(model.get(PEEK_VIEW_AND_EXPANDED_CONTENT_ALPHA));
            View expandedContent = view.findViewById(R.id.expanded_content_group);
            expandedContent.setAlpha(1.0f - model.get(PEEK_VIEW_AND_EXPANDED_CONTENT_ALPHA));
        } else if (PEEK_VIEW_AND_EXPANDED_CONTENT_VISIBILITY == propertyKey) {
            int webContainerVisibility =
                    model.get(PEEK_VIEW_AND_EXPANDED_CONTENT_VISIBILITY) == View.VISIBLE
                            ? View.GONE
                            : View.VISIBLE;
            View peekContainer = view.findViewById(R.id.actor_control_container);
            peekContainer.setVisibility(model.get(PEEK_VIEW_AND_EXPANDED_CONTENT_VISIBILITY));
            View expandedContent = view.findViewById(R.id.expanded_content_group);
            expandedContent.setVisibility(webContainerVisibility);
        }
    }
}
