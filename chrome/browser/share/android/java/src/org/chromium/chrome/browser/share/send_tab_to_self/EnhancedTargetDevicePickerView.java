// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.send_tab_to_self;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewParent;
import android.widget.RelativeLayout;

import androidx.annotation.NonNull;
import androidx.annotation.Px;
import androidx.annotation.StringRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetListViewBase;
import org.chromium.components.browser_ui.bottomsheet.ItemDividerBase;

import java.util.Objects;
import java.util.Set;

/** View for the Send Tab To Self Enhanced Bottom Sheet. */
@NullMarked
class EnhancedTargetDevicePickerView extends BottomSheetListViewBase {
    /** Item types used in the list. */
    @interface ItemType {
        int DEVICE = 0;
    }

    private static class HorizontalDividerItemDecoration extends ItemDividerBase {
        HorizontalDividerItemDecoration(Context context) {
            super(context);
        }

        @Override
        protected boolean shouldSkipItemType(int type) {
            return false;
        }
    }

    final View mSendButton;
    final View mBottomActionsBlock;
    final View mManageDevicesLink;

    EnhancedTargetDevicePickerView(Context context, BottomSheetController bottomSheetController) {
        super(
                bottomSheetController,
                (RelativeLayout)
                        LayoutInflater.from(context)
                                .inflate(R.layout.send_tab_to_self_enhanced_sheet, null),
                true);
        mSendButton =
                Objects.requireNonNull(
                        getContentView().findViewById(R.id.send_button), "send_button not found");
        mBottomActionsBlock =
                Objects.requireNonNull(
                        getContentView().findViewById(R.id.bottom_actions_block),
                        "bottom_actions_block not found");
        mManageDevicesLink =
                Objects.requireNonNull(
                        getContentView().findViewById(R.id.manage_devices_link),
                        "manage_devices_link not found");

        setSheetItemListView(getContentView().findViewById(R.id.sheet_item_list));
        getSheetItemListView().addItemDecoration(new HorizontalDividerItemDecoration(context));

        getContentView().addOnAttachStateChangeListener(new ClipLayoutHelper());
    }

    @Override
    public int getVerticalScrollOffset() {
        return getSheetItemListView().computeVerticalScrollOffset();
    }

    @Override
    public @NonNull String getSheetContentDescription(Context context) {
        return context.getString(R.string.send_tab_to_self_content_description);
    }

    @Override
    public @StringRes int getSheetHalfHeightAccessibilityStringId() {
        return R.string.send_tab_to_self_sheet_half_height;
    }

    @Override
    public @StringRes int getSheetFullHeightAccessibilityStringId() {
        return R.string.send_tab_to_self_sheet_full_height;
    }

    @Override
    public @StringRes int getSheetClosedAccessibilityStringId() {
        return R.string.send_tab_to_self_sheet_closed;
    }

    @Override
    protected View getHandlebar() {
        return getContentView().findViewById(R.id.drag_handlebar);
    }

    @Override
    protected @Nullable View getHeaderView() {
        return getContentView().findViewById(R.id.sheet_title);
    }

    @Override
    protected @Px int getDesiredSheetHeightPx() {
        if (mSendButton.getMeasuredHeight() == 0) {
            remeasure();
        }
        int sendButtonHeight = mSendButton.getMeasuredHeight();
        int paddingTop = mBottomActionsBlock.getPaddingTop();
        return super.getDesiredSheetHeightPx() + sendButtonHeight + paddingTop + getSideMarginPx();
    }

    @Override
    protected @Px int getMaximumSheetHeightPx() {
        int maxHeight = super.getMaximumSheetHeightPx();
        int bottomBlockHeight = mBottomActionsBlock.getMeasuredHeight();
        return maxHeight + bottomBlockHeight;
    }

    @Override
    protected @Px int getConclusiveMarginHeightPx() {
        int bottomBlockHeight = mBottomActionsBlock.getMeasuredHeight();
        return bottomBlockHeight
                + getContentView().getResources().getDimensionPixelSize(R.dimen.stts_sheet_padding);
    }

    @Override
    protected @Px int getSideMarginPx() {
        return getContentView().getResources().getDimensionPixelSize(R.dimen.stts_sheet_margin);
    }

    @Override
    protected Set<Integer> listedItemTypes() {
        return Set.of(ItemType.DEVICE);
    }

    @Override
    protected Set<Integer> footerItemTypes() {
        return Set.of();
    }

    @Override
    public boolean coversBottomControls() {
        return true;
    }

    private static class ClipLayoutHelper implements View.OnAttachStateChangeListener {
        private boolean mOriginalClipChildren;
        private boolean mOriginalClipToPadding;
        private @Nullable ViewGroup mParent;

        @Override
        public void onViewAttachedToWindow(View v) {
            if (mParent != null) return;
            ViewParent parent = v.getParent();
            if (parent instanceof ViewGroup) {
                mParent = (ViewGroup) parent;
                mOriginalClipChildren = mParent.getClipChildren();
                mOriginalClipToPadding = mParent.getClipToPadding();
                mParent.setClipChildren(false);
                mParent.setClipToPadding(false);
            }
        }

        @Override
        public void onViewDetachedFromWindow(View v) {
            if (mParent != null) {
                mParent.setClipChildren(mOriginalClipChildren);
                mParent.setClipToPadding(mOriginalClipToPadding);
                mParent = null;
            }
        }
    }
}
