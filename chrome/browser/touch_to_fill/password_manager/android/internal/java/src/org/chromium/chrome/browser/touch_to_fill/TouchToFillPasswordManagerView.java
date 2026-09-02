// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.RelativeLayout;

import androidx.annotation.Px;
import androidx.annotation.StringRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.touch_to_fill.TouchToFillPasswordManagerProperties.ItemType;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetListViewBase;
import org.chromium.components.browser_ui.bottomsheet.ItemDividerBase;

import java.util.Set;

/**
 * This class is responsible for rendering the bottom sheet which displays the touch to fill
 * credentials. It is a View in this Model-View-Controller component and doesn't inherit but holds
 * Android Views.
 */
@NullMarked
class TouchToFillPasswordManagerView extends BottomSheetListViewBase {
    private static class HorizontalDividerItemDecoration extends ItemDividerBase {
        HorizontalDividerItemDecoration(Context context) {
            super(context);
        }

        @Override
        protected boolean shouldSkipItemType(@ItemType int type) {
            switch (type) {
                case ItemType.HEADER: // Fallthrough.
                case ItemType.FILL_BUTTON:
                case ItemType.FOOTER:
                    return true;
                case ItemType.CREDENTIAL: // Fallthrough.
                case ItemType.WEBAUTHN_CREDENTIAL:
                case ItemType.MORE_PASSKEYS:
                    return false;
            }
            assert false : "Undefined whether to skip setting background for item of type: " + type;
            return true; // Should never be reached. But if, skip to not change anything.
        }
    }

    /**
     * Constructs a TouchToFillPasswordManagerView which creates, modifies, and shows the bottom
     * sheet.
     *
     * @param context A {@link Context} used to load resources and inflate the sheet.
     * @param bottomSheetController The {@link BottomSheetController} used to show/hide the sheet.
     */
    TouchToFillPasswordManagerView(Context context, BottomSheetController bottomSheetController) {
        super(
                bottomSheetController,
                (RelativeLayout)
                        LayoutInflater.from(context)
                                .inflate(R.layout.touch_to_fill_password_manager_sheet, null),
                true);

        setSheetItemListView(getContentView().findViewById(R.id.sheet_item_list));
        getSheetItemListView().addItemDecoration(new HorizontalDividerItemDecoration(context));
    }

    @Override
    public int getVerticalScrollOffset() {
        return getSheetItemListView().computeVerticalScrollOffset();
    }

    @Override
    public String getSheetContentDescription(Context context) {
        return context.getString(R.string.touch_to_fill_content_description);
    }

    @Override
    public @StringRes int getSheetHalfHeightAccessibilityStringId() {
        return R.string.touch_to_fill_sheet_half_height;
    }

    @Override
    public @StringRes int getSheetFullHeightAccessibilityStringId() {
        return R.string.touch_to_fill_sheet_full_height;
    }

    @Override
    public @StringRes int getSheetClosedAccessibilityStringId() {
        return R.string.touch_to_fill_sheet_closed;
    }

    @Override
    protected View getHandlebar() {
        return getContentView().findViewById(R.id.drag_handlebar);
    }

    @Override
    protected @Nullable View getHeaderView() {
        // Credential filling bottom sheet doesn't have a static header view.
        return null;
    }

    @Override
    protected @Px int getConclusiveMarginHeightPx() {
        return getContentView()
                .getResources()
                .getDimensionPixelSize(R.dimen.touch_to_fill_sheet_bottom_padding_button);
    }

    @Override
    protected @Px int getSideMarginPx() {
        return getContentView().getResources().getDimensionPixelSize(R.dimen.ttf_sheet_margin);
    }

    @Override
    protected Set<Integer> listedItemTypes() {
        return Set.of(
                TouchToFillPasswordManagerProperties.ItemType.CREDENTIAL,
                TouchToFillPasswordManagerProperties.ItemType.WEBAUTHN_CREDENTIAL,
                TouchToFillPasswordManagerProperties.ItemType.MORE_PASSKEYS);
    }

    @Override
    protected Set<Integer> footerItemTypes() {
        return Set.of(TouchToFillPasswordManagerProperties.ItemType.FOOTER);
    }
}
