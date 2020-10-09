// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.suggestions.tile;

import android.view.View;
import android.view.ViewGroup.MarginLayoutParams;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Binder wiring for the TileView. */
public class TileViewBinder {
    /** @see PropertyModelChangeProcessor.ViewBinder#bind(Object, Object, Object) */
    public static void bind(PropertyModel model, TileView view, PropertyKey propertyKey) {
        if (propertyKey == TileViewProperties.TITLE) {
            final TextView textView = view.findViewById(R.id.tile_view_title);
            textView.setText(model.get(TileViewProperties.TITLE));
        } else if (propertyKey == TileViewProperties.TITLE_LINES) {
            final TextView textView = view.findViewById(R.id.tile_view_title);
            textView.setLines(model.get(TileViewProperties.TITLE_LINES));
        } else if (propertyKey == TileViewProperties.ICON) {
            final ImageView iconView = view.findViewById(R.id.tile_view_icon);
            iconView.setImageDrawable(model.get(TileViewProperties.ICON));
        } else if (propertyKey == TileViewProperties.BADGE_VISIBLE) {
            final View badgeView = view.findViewById(R.id.offline_badge);
            final boolean isVisible = model.get(TileViewProperties.BADGE_VISIBLE);
            badgeView.setVisibility(isVisible ? View.VISIBLE : View.GONE);
        } else if (propertyKey == TileViewProperties.SHOW_LARGE_ICON) {
            final boolean useLargeIcon = model.get(TileViewProperties.SHOW_LARGE_ICON);
            final int iconEdgeSize = view.getResources().getDimensionPixelSize(useLargeIcon
                            ? R.dimen.tile_view_icon_size
                            : R.dimen.tile_view_icon_size_modern);
            final int iconTopMarginSize = view.getResources().getDimensionPixelOffset(useLargeIcon
                            ? R.dimen.tile_view_icon_background_margin_top_modern
                            : R.dimen.tile_view_icon_margin_top_modern);
            final View iconView = view.findViewById(R.id.tile_view_icon);
            final MarginLayoutParams params = (MarginLayoutParams) iconView.getLayoutParams();
            params.width = iconEdgeSize;
            params.height = iconEdgeSize;
            params.topMargin = iconTopMarginSize;
            iconView.setLayoutParams(params);
        } else if (propertyKey == TileViewProperties.ON_FOCUS_VIA_SELECTION) {
            view.setOnFocusViaSelectionListener(
                    model.get(TileViewProperties.ON_FOCUS_VIA_SELECTION));
        } else if (propertyKey == TileViewProperties.ON_CLICK) {
            view.setOnClickListener(model.get(TileViewProperties.ON_CLICK));
        } else if (propertyKey == TileViewProperties.ON_LONG_CLICK) {
            view.setOnLongClickListener(model.get(TileViewProperties.ON_LONG_CLICK));
        } else if (propertyKey == TileViewProperties.ON_CREATE_CONTEXT_MENU) {
            view.setOnCreateContextMenuListener(
                    model.get(TileViewProperties.ON_CREATE_CONTEXT_MENU));
        } else if (propertyKey == TileViewProperties.CONTENT_DESCRIPTION) {
            view.setContentDescription(model.get(TileViewProperties.CONTENT_DESCRIPTION));
        }
    }
}
