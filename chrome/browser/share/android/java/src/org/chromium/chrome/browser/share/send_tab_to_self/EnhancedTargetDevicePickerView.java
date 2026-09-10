// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.send_tab_to_self;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewParent;
import android.view.WindowInsets;

import androidx.annotation.Px;
import androidx.annotation.StringRes;
import androidx.core.view.WindowInsetsCompat;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetListViewBase;
import org.chromium.components.browser_ui.bottomsheet.ItemDividerBase;

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
    final View mManageDevicesBlock;
    final View mManageDevicesLink;

    EnhancedTargetDevicePickerView(Context context, BottomSheetController bottomSheetController) {
        super(
                bottomSheetController,
                LayoutInflater.from(context)
                        .inflate(R.layout.send_tab_to_self_enhanced_sheet, null),
                true);
        mSendButton = getContentView().findViewById(R.id.send_button);
        // TODO(crbug.com/532092798): Explore methods to avoid jank during state transitions.
        mBottomActionsBlock = getContentView().findViewById(R.id.bottom_actions_block);
        mManageDevicesBlock = getContentView().findViewById(R.id.manage_devices_block);
        mManageDevicesLink = getContentView().findViewById(R.id.manage_devices_link);
        if (bottomSheetController.isLargeFormFactorUiEnabled(this)) {
            updateManageDevicesVisibility(true);
        }

        setSheetItemListView(getContentView().findViewById(R.id.sheet_item_list));
        getSheetItemListView().addItemDecoration(new HorizontalDividerItemDecoration(context));

        getContentView().addOnAttachStateChangeListener(new ClipLayoutHelper());
        if (bottomSheetController.isLargeFormFactorUiEnabled(this)) {
            getContentView()
                    .addOnLayoutChangeListener(
                            (v, left, top, right, bottom, oldLeft, oldTop, oldRight, oldBottom) -> {
                                if (bottom - top != oldBottom - oldTop) {
                                    int state = getBottomSheetController().getSheetState();
                                    if (state == SheetState.HALF) {
                                        handleHalfStateOverflow(getSheetItemListView());
                                    } else if (state == SheetState.FULL) {
                                        handleFullStateOverflow(getSheetItemListView());
                                    }
                                }
                            });
        }
    }

    @Override
    public int getVerticalScrollOffset() {
        return getSheetItemListView().computeVerticalScrollOffset();
    }

    @Override
    public String getSheetContentDescription(Context context) {
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
        return getNonListHeightForState(/* forHalfState= */ true) + getInitialListHeight();
    }

    @Override
    protected @Px int getMaximumSheetHeightPx() {
        return getNonListHeightForState(/* forHalfState= */ false) + calculateListHeight();
    }

    private @Px int getNonListHeightForState(boolean forHalfState) {
        int height = getHeaderAndHandlebarHeightPx();
        int paddingTop = mBottomActionsBlock.getPaddingTop();
        int paddingBottom = mBottomActionsBlock.getPaddingBottom();
        int sendButtonHeight = getHeightWithMarginsPx(mSendButton);
        height += paddingTop + sendButtonHeight + paddingBottom;
        boolean isDesktop = getBottomSheetController().isLargeFormFactorUiEnabled(this);
        if (!forHalfState || isDesktop) {
            height += getHeightWithMarginsPx(mManageDevicesBlock);
        }
        return height;
    }

    private @Px int getHeaderAndHandlebarHeightPx() {
        return getHeightWithMarginsPx(getHandlebar()) + getHeightWithMarginsPx(getHeaderView());
    }

    private @Px int getInitialListHeight() {
        int initialHeight = super.getDesiredSheetHeightPx() - getHeaderAndHandlebarHeightPx();
        if (initialHeight <= 0 && getBottomSheetController().isLargeFormFactorUiEnabled(this)) {
            RecyclerView listView = getSheetItemListView();
            if (listView.getAdapter() == null || listView.getAdapter().getItemCount() == 0) {
                return 0;
            }
            int itemCount = listView.getAdapter().getItemCount();
            int defaultItemHeight = getDefaultListHeightPx(listView, 1);
            float visibleItemCount = Math.min(itemCount, MAX_FULLY_VISIBLE_LIST_ITEM_COUNT + 0.5f);
            initialHeight = (int) (defaultItemHeight * visibleItemCount);
        }
        return Math.max(0, initialHeight);
    }

    private @Px int calculateListHeight() {
        RecyclerView sheetItemListView = getSheetItemListView();
        if (sheetItemListView.getAdapter() == null) {
            return 0;
        }

        // itemCount is the total number of items in the adapter.
        int itemCount = sheetItemListView.getAdapter().getItemCount();
        if (itemCount == 0) {
            return 0;
        }

        // childCount is the number of child views currently layouted in the RecyclerView.
        int childCount = sheetItemListView.getChildCount();
        View firstChild = childCount > 0 ? sheetItemListView.getChildAt(0) : null;
        int itemHeight =
                firstChild != null
                        ? getHeightWithMarginsPx(firstChild)
                        : getDefaultListHeightPx(sheetItemListView, 1);
        return itemHeight * itemCount;
    }

    private @Px int getDefaultListHeightPx(RecyclerView listView, int itemCount) {
        int defaultItemHeightPx =
                listView.getContext()
                        .getResources()
                        .getDimensionPixelSize(R.dimen.stts_enhanced_list_item_height);
        return itemCount * defaultItemHeightPx;
    }

    @Override
    protected @Px int getConclusiveMarginHeightPx() {
        return getContentView().getResources().getDimensionPixelSize(R.dimen.stts_sheet_padding);
    }

    @Override
    protected @Px int getSideMarginPx() {
        return 0;
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
    protected void onSheetStateChanged(@SheetState int newState, @StateChangeReason int reason) {
        super.onSheetStateChanged(newState, reason);
        onSheetStateChange(newState);
    }

    private void onSheetStateChange(@SheetState int newState) {
        boolean inHalfState = newState == SheetState.HALF;
        boolean isDesktop = getBottomSheetController().isLargeFormFactorUiEnabled(this);
        updateManageDevicesVisibility(!inHalfState || isDesktop);
        if (!isDesktop) {
            remeasure();
        }
        RecyclerView sheetItemListView = getSheetItemListView();
        if (inHalfState) {
            handleHalfStateOverflow(sheetItemListView);
        } else {
            handleFullStateOverflow(sheetItemListView);
        }
    }

    private void handleHalfStateOverflow(RecyclerView sheetItemListView) {
        updateOverflowState(
                sheetItemListView,
                getInitialListHeight() < calculateListHeight(),
                () -> limitListHeightForHalfState());
    }

    private void handleFullStateOverflow(RecyclerView sheetItemListView) {
        int maxContainerHeight = getMaxAvailableHeightPx();
        int nonListHeight = getNonListHeightForState(/* forHalfState= */ false);
        int totalMaxHeight = nonListHeight + calculateListHeight();
        boolean overflows = maxContainerHeight > 0 && totalMaxHeight > maxContainerHeight;
        updateOverflowState(
                sheetItemListView,
                overflows,
                () -> limitListHeightForFullState(maxContainerHeight, nonListHeight));
    }

    private void updateOverflowState(
            RecyclerView sheetItemListView, boolean overflows, Runnable limitHeightAction) {
        if (overflows) {
            enableFadingEdge(sheetItemListView);
            limitHeightAction.run();
        } else {
            disableFadingEdge(sheetItemListView);
            resetListHeightToWrapContent(sheetItemListView);
        }
    }

    private void limitListHeightForFullState(int maxContainerHeight, int nonListHeight) {
        setSheetItemListHeightPx(Math.max(0, maxContainerHeight - nonListHeight));
    }

    private void updateManageDevicesVisibility(boolean visible) {
        int visibility = visible ? View.VISIBLE : View.GONE;
        if (mManageDevicesBlock.getVisibility() == visibility) return;
        mManageDevicesBlock.setVisibility(visibility);
    }

    private @Px int getMaxAvailableHeightPx() {
        if (getBottomSheetController().isLargeFormFactorUiEnabled(this)) {
            int maxHeight = getBottomSheetController().getMaxSheetHeight();
            return maxHeight > 0 ? maxHeight : getBottomSheetController().getContainerHeight();
        }
        return getBottomSheetController().getContainerHeight() - getSystemWindowInsetBottomPx();
    }

    private @Px int getSystemWindowInsetBottomPx() {
        WindowInsets insets = getContentView().getRootWindowInsets();
        if (insets == null) return 0;
        return WindowInsetsCompat.toWindowInsetsCompat(insets, getContentView())
                .getInsets(WindowInsetsCompat.Type.systemBars())
                .bottom;
    }

    private void enableFadingEdge(RecyclerView listView) {
        listView.setVerticalFadingEdgeEnabled(true);
        int fadeLength =
                listView.getResources().getDimensionPixelSize(R.dimen.stts_fading_edge_length);
        listView.setFadingEdgeLength(fadeLength);
    }

    private void disableFadingEdge(RecyclerView listView) {
        listView.setVerticalFadingEdgeEnabled(false);
    }

    private void limitListHeightForHalfState() {
        int maxListHeight = getInitialListHeight();
        int availableListHeight =
                getMaxAvailableHeightPx() - getNonListHeightForState(/* forHalfState= */ true);
        if (availableListHeight > 0 && maxListHeight > availableListHeight) {
            maxListHeight = availableListHeight;
        }
        if (maxListHeight > 0) {
            setSheetItemListHeightPx(maxListHeight);
        }
    }

    private void resetListHeightToWrapContent(RecyclerView listView) {
        ViewGroup.LayoutParams params = listView.getLayoutParams();
        if (params != null && params.height != ViewGroup.LayoutParams.WRAP_CONTENT) {
            params.height = ViewGroup.LayoutParams.WRAP_CONTENT;
            listView.setLayoutParams(params);
        }
    }

    private void setSheetItemListHeightPx(int targetHeight) {
        RecyclerView sheetItemListView = getSheetItemListView();
        ViewGroup.LayoutParams params = sheetItemListView.getLayoutParams();
        if (params == null || params.height == targetHeight) return;

        params.height = targetHeight;
        sheetItemListView.setLayoutParams(params);
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
