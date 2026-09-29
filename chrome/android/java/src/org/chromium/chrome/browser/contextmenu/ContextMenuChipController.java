// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import android.content.Context;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.components.browser_ui.widget.chips.ChipView;
import org.chromium.components.embedder_support.contextmenu.ChipRenderParams;
import org.chromium.ui.widget.AnchoredPopupWindow;
import org.chromium.ui.widget.ViewRectProvider;

/** A controller to handle chip construction and cross-app communication. */
@NullMarked
class ContextMenuChipController implements View.OnClickListener {
    private final View mAnchorView;
    private final Context mContext;
    private final Runnable mDismissContextMenuCallback;

    private @Nullable AnchoredPopupWindow mPopupWindow;
    private @Nullable ChipRenderParams mChipRenderParams;
    private @Nullable ChipView mChipView;

    ContextMenuChipController(
            Context context, View anchorView, final Runnable dismissContextMenuCallback) {
        mContext = context;
        mAnchorView = anchorView;
        mDismissContextMenuCallback = dismissContextMenuCallback;
    }

    /**
     * Returns the necessary px necessary to render the chip with enough margin space above and
     * below.
     */
    int getVerticalPxNeededForChip() {
        return 2
                        * mContext.getResources()
                                .getDimensionPixelSize(R.dimen.context_menu_chip_vertical_margin)
                + mContext.getResources().getDimensionPixelSize(R.dimen.chip_default_height);
    }

    /**
     * Derive max text view width by subtracting the width of other elements from the max chip
     * width.
     */
    @VisibleForTesting
    int getChipTextMaxWidthPx(boolean isRemoveIconHidden) {
        int maxWidthPx =
                mContext.getResources().getDimensionPixelSize(R.dimen.context_menu_chip_max_width)
                        // Padding before primary icon
                        - mContext.getResources()
                                .getDimensionPixelSize(R.dimen.chip_view_extended_start_padding)
                        // Padding after primary icon
                        - mContext.getResources()
                                .getDimensionPixelSize(R.dimen.chip_view_start_padding)
                        // Primary icon width.
                        - mContext.getResources()
                                .getDimensionPixelSize(R.dimen.context_menu_chip_icon_size);
        if (!isRemoveIconHidden) {
            maxWidthPx =
                    maxWidthPx
                            // Padding before close icon.
                            - mContext.getResources()
                                    .getDimensionPixelSize(
                                            R.dimen.chip_end_icon_extended_margin_start)
                            // End icon width.
                            - mContext.getResources()
                                    .getDimensionPixelSize(R.dimen.context_menu_chip_icon_size)
                            // Padding after close icon.
                            - mContext.getResources()
                                    .getDimensionPixelSize(
                                            R.dimen.chip_end_icon_extended_margin_end);
        }

        return maxWidthPx;
    }

    @Override
    public void onClick(View v) {
        if (v == mChipView) {
            // The onClick callback may result in a cross-app switch so dismiss the menu before
            // executing that logic. Also note that dismissing the menu will also dismiss the chip.
            mDismissContextMenuCallback.run();
            Runnable onClick = mChipRenderParams == null ? null : mChipRenderParams.onClickCallback;
            if (onClick != null) onClick.run();
        }
    }

    /**
     * Dismiss the lens chip. Needed for cases where a user dismisses
     * the context menu without closing the chip manually.
     */
    void dismissChipIfShowing() {
        if (mPopupWindow != null && mPopupWindow.isShowing()) {
            mPopupWindow.dismiss();
        }
    }

    private ChipView buildChipView(ChipRenderParams chipRenderParams) {
        ChipView chipView =
                (ChipView) LayoutInflater.from(mContext).inflate(R.layout.context_menu_chip, null);

        chipView.getPrimaryTextView().setText(mContext.getString(chipRenderParams.titleResourceId));
        // TODO(benwgold): Consult with Chrome UX owners to see if Chip UI hierarchy should be
        // refactored.
        chipView.getPrimaryTextView()
                .setMaxWidth(getChipTextMaxWidthPx(chipRenderParams.isRemoveIconHidden));

        if (chipRenderParams.iconResourceId != 0) {
            chipView.setIconWithTint(
                    chipRenderParams.iconResourceId, /* tintWithTextColor= */ false);
        }

        if (!chipRenderParams.isRemoveIconHidden) {
            chipView.addRemoveIcon();
            chipView.setRemoveIconClickListener(v -> dismissChipIfShowing());
        }

        chipView.setOnClickListener(this);
        return chipView;
    }

    /**
     * Inflate an anchored chip view, set it up, and show it to the user.
     *
     * @param chipRenderParams The data to construct the chip.
     */
    protected void showChip(ChipRenderParams chipRenderParams) {
        if (mPopupWindow != null) {
            // Chip has already been shown for this context menu.
            return;
        }

        mChipRenderParams = chipRenderParams;

        ChipView chipView = buildChipView(chipRenderParams);

        ViewRectProvider rectProvider = new ViewRectProvider(mAnchorView);
        // Draw a clear background to avoid blocking context menu items.
        mPopupWindow =
                new AnchoredPopupWindow.Builder(
                                mContext,
                                mAnchorView,
                                new ColorDrawable(Color.TRANSPARENT),
                                () -> chipView,
                                rectProvider)
                        .setAnimationStyle(R.style.ChipAnimation)
                        .setPreferredHorizontalOrientation(
                                AnchoredPopupWindow.HorizontalOrientation.CENTER)
                        // The bottom margin will determine the vertical placement of the chip, so
                        // ensure that this distance is computed from the anchor.
                        .setPreferredVerticalOrientation(
                                AnchoredPopupWindow.VerticalOrientation.ABOVE)
                        .setFocusable(false)
                        // Don't dismiss as a result of touches outside of the chip popup.
                        .setOutsideTouchable(false)
                        .setMaxWidth(
                                mContext.getResources()
                                        .getDimensionPixelSize(R.dimen.context_menu_chip_max_width))
                        .build();

        mChipView = chipView;

        mPopupWindow.show();
        if (mChipRenderParams.onShowCallback != null) {
            mChipRenderParams.onShowCallback.run();
        }
    }

    @Nullable AnchoredPopupWindow getCurrentPopupWindowForTesting() {
        return mPopupWindow;
    }

    void clickChipForTesting() {
        if (mChipView != null) onClick(mChipView);
    }
}
