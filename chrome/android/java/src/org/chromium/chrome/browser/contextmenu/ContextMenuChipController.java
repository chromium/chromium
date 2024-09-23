// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import android.content.Context;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.R;
import org.chromium.components.browser_ui.widget.chips.ChipView;
import org.chromium.components.embedder_support.contextmenu.ChipRenderParams;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;
import org.chromium.ui.widget.AnchoredPopupWindow;
import org.chromium.ui.widget.ViewRectProvider;

/** A controller to handle chip construction and cross-app communication. */
class ContextMenuChipController implements View.OnClickListener {
    private boolean mFakeLensQueryResultForTesting;
    private View mAnchorView;
    private ChipView mChipView;
    private AnchoredPopupWindow mPopupWindow;
    private Context mContext;
    private ChipRenderParams mChipRenderParams;
    private final Runnable mDismissContextMenuCallback;

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
                                .getDimensionPixelSize(
                                        R.dimen.chip_element_extended_leading_padding)
                        // Padding after primary icon
                        - mContext.getResources()
                                .getDimensionPixelSize(R.dimen.chip_element_leading_padding)
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
                                            R.dimen.chip_extended_end_padding_with_end_icon);
        }

        return maxWidthPx;
    }

    // This method should only be used in test files.  It is not marked
    // @VisibleForTesting to allow the Coordinator to reference it in its
    // own testing methods.
    void setFakeLensQueryResultForTesting() {
        mFakeLensQueryResultForTesting = true;
    }

    @Override
    public void onClick(View v) {
        if (v == mChipView) {
            // The onClick callback may result in a cross-app switch so dismiss the menu before
            // executing that logic. Also note that dismissing the menu will also dismiss the chip.
            mDismissContextMenuCallback.run();
            mChipRenderParams.onClickCallback.run();
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

    /**
     * Inflate an anchored chip view, set it up, and show it to the user.
     * @param chipRenderParams The data to construct the chip.
     */
    protected void showChip(@NonNull ChipRenderParams chipRenderParams) {
        if (mPopupWindow != null) {
            // Chip has already been shown for this context menu.
            return;
        }

        mChipRenderParams = chipRenderParams;

        mChipView =
                (ChipView) LayoutInflater.from(mContext).inflate(R.layout.context_menu_chip, null);

        ViewRectProvider rectProvider = new ViewRectProvider(mAnchorView);
        // Draw a clear background to avoid blocking context menu items.
        mPopupWindow =
                new AnchoredPopupWindow(
                        mContext,
                        mAnchorView,
                        new ColorDrawable(Color.TRANSPARENT),
                        mChipView,
                        rectProvider);
        mPopupWindow.setAnimationStyle(R.style.ChipAnimation);
        mPopupWindow.setPreferredHorizontalOrientation(
                AnchoredPopupWindow.HorizontalOrientation.CENTER);
        // The bottom margin will determine the vertical placement of the chip, so
        // ensure that this distance is computed from the anchor.
        mPopupWindow.setPreferredVerticalOrientation(AnchoredPopupWindow.VerticalOrientation.ABOVE);
        mPopupWindow.setFocusable(false);
        // Don't dismiss as a result of touches outside of the chip popup.
        mPopupWindow.setOutsideTouchable(false);
        mPopupWindow.setMaxWidth(
                mContext.getResources().getDimensionPixelSize(R.dimen.context_menu_chip_max_width));

        mChipView
                .getPrimaryTextView()
                .setText(
                        SpanApplier.removeSpanText(
                                mContext.getString(chipRenderParams.titleResourceId),
                                new SpanInfo("<new>", "</new>")));
        // TODO(benwgold): Consult with Chrome UX owners to see if Chip UI hierarchy should be
        // refactored.
        mChipView
                .getPrimaryTextView()
                .setMaxWidth(getChipTextMaxWidthPx(chipRenderParams.isRemoveIconHidden));

        if (chipRenderParams.iconResourceId != 0) {
            mChipView.setIcon(chipRenderParams.iconResourceId, false);
        }

        if (!chipRenderParams.isRemoveIconHidden) {
            mChipView.addRemoveIcon();
            mChipView.setRemoveIconClickListener(
                    v -> {
                        dismissChipIfShowing();
                    });
        }

        mChipView.setOnClickListener(this);

        mPopupWindow.show();
        if (mChipRenderParams.onShowCallback != null) {
            mChipRenderParams.onShowCallback.run();
        }
    }

    // This method should only be used in test files.  It is not marked
    // @VisibleForTesting to allow the Coordinator to reference it in its
    // own testing methods.
    AnchoredPopupWindow getCurrentPopupWindowForTesting() {
        return mPopupWindow;
    }

    // This method should only be used in test files.  It is not marked
    // @VisibleForTesting to allow the Coordinator to reference it in its
    // own testing methods.
    void clickChipForTesting() {
        onClick(mChipView);
    }
}
