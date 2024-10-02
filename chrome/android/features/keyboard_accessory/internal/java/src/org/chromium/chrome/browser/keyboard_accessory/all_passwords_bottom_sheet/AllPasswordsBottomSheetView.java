// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.all_passwords_bottom_sheet;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.SearchView;
import android.widget.SearchView.OnQueryTextListener;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.keyboard_accessory.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.ui.base.LocalizationUtils;

/**
 * This class is responsible for rendering the bottom sheet which displays all credentials from any
 * origin. It is a View in this Model-View-Controller component and doesn't inherit from a view but
 * holds Android Views.
 */
class AllPasswordsBottomSheetView implements BottomSheetContent {
    private final BottomSheetController mBottomSheetController;
    private Callback<Integer> mDismissHandler;
    private final RecyclerView mSheetItemListView;
    private final LinearLayout mContentView;

    private final BottomSheetObserver mBottomSheetObserver =
            new EmptyBottomSheetObserver() {
                @Override
                public void onSheetClosed(@BottomSheetController.StateChangeReason int reason) {
                    super.onSheetClosed(reason);
                    assert mDismissHandler != null;
                    mDismissHandler.onResult(reason);
                    mBottomSheetController.removeObserver(mBottomSheetObserver);
                }

                @Override
                public void onSheetStateChanged(int newState, int reason) {
                    super.onSheetStateChanged(newState, reason);
                    if (newState != BottomSheetController.SheetState.HIDDEN) return;
                    // This is a fail-safe for cases where onSheetClosed isn't triggered.
                    mDismissHandler.onResult(BottomSheetController.StateChangeReason.NONE);
                    mBottomSheetController.removeObserver(mBottomSheetObserver);
                }
            };

    /**
     * Constructs an AllPasswordsBottomSheetView which creates, modifies, and shows the bottom
     * sheet.
     * @param context A {@link Context} used to load resources and inflate the sheet.
     * @param bottomSheetController The {@link BottomSheetController} used to show/hide the sheet.
     */
    public AllPasswordsBottomSheetView(
            Context context, BottomSheetController bottomSheetController) {
        mBottomSheetController = bottomSheetController;
        mContentView =
                (LinearLayout)
                        LayoutInflater.from(context)
                                .inflate(R.layout.all_passwords_bottom_sheet, null);
        mSheetItemListView = mContentView.findViewById(R.id.sheet_item_list);
        mSheetItemListView.setLayoutManager(
                new LinearLayoutManager(
                        mSheetItemListView.getContext(), LinearLayoutManager.VERTICAL, false));
        mSheetItemListView.setItemAnimator(null);

        // Apply RTL layout changes.
        int layoutDirection =
                LocalizationUtils.isLayoutRtl()
                        ? View.LAYOUT_DIRECTION_RTL
                        : View.LAYOUT_DIRECTION_LTR;
        mContentView.setLayoutDirection(layoutDirection);

        mContentView.setOnGenericMotionListener((v, e) -> true); // Filter background interaction.
    }

    /**
     * Sets a new listener that reacts to events like item selection or dismissal.
     *
     * @param dismissHandler A {@link Callback<Integer>}.
     */
    void setDismissHandler(Callback<Integer> dismissHandler) {
        mDismissHandler = dismissHandler;
    }

    /**
     * If set to true, requests to show the bottom sheet. Otherwise, requests to hide the sheet.
     * @param isVisible A boolean describing whether to show or hide the sheet.
     */
    void setVisible(boolean isVisible) {
        if (isVisible) {
            mBottomSheetController.addObserver(mBottomSheetObserver);
            if (!mBottomSheetController.requestShowContent(this, true)) {
                assert (mDismissHandler != null);
                mDismissHandler.onResult(BottomSheetController.StateChangeReason.NONE);
                mBottomSheetController.removeObserver(mBottomSheetObserver);
            }
        } else {
            mBottomSheetController.hideContent(this, true);
        }
    }

    void setWarning(CharSequence warningMessage) {
        final TextView warningTextView = mContentView.findViewById(R.id.sheet_warning);
        warningTextView.setText(warningMessage);
    }

    void setSheetItemListAdapter(RecyclerView.Adapter adapter) {
        mSheetItemListView.setAdapter(adapter);
    }

    void setSearchQueryChangeHandler(Callback<String> callback) {
        SearchView searchView = getSearchView();
        searchView.setOnQueryTextListener(
                new OnQueryTextListener() {
                    @Override
                    public boolean onQueryTextSubmit(String s) {
                        return false;
                    }

                    @Override
                    public boolean onQueryTextChange(String newString) {
                        callback.onResult(newString);
                        return true;
                    }
                });
    }

    public SearchView getSearchView() {
        return mContentView.findViewById(R.id.all_passwords_search_view);
    }

    @Override
    public View getContentView() {
        return mContentView;
    }

    @Nullable
    @Override
    public View getToolbarView() {
        return null;
    }

    @Override
    public int getVerticalScrollOffset() {
        return mSheetItemListView.computeVerticalScrollOffset();
    }

    @Override
    public void destroy() {
        mBottomSheetController.removeObserver(mBottomSheetObserver);
    }

    @Override
    public int getPriority() {
        return BottomSheetContent.ContentPriority.HIGH;
    }

    @Override
    public boolean hasCustomScrimLifecycle() {
        return false;
    }

    @Override
    public boolean swipeToDismissEnabled() {
        return false;
    }

    @Override
    public boolean skipHalfStateOnScrollingDown() {
        return false;
    }

    @Override
    public int getPeekHeight() {
        return BottomSheetContent.HeightMode.DISABLED;
    }

    @Override
    public float getHalfHeightRatio() {
        return BottomSheetContent.HeightMode.DISABLED;
    }

    @Override
    public boolean hideOnScroll() {
        return false;
    }

    @Override
    public int getSheetContentDescriptionStringId() {
        return R.string.all_passwords_bottom_sheet_content_description;
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        return R.string.all_passwords_bottom_sheet_half_height;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        return R.string.all_passwords_bottom_sheet_full_height;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        return R.string.all_passwords_bottom_sheet_closed;
    }

    @VisibleForTesting
    CharSequence getWarningText() {
        final TextView warningTextView = mContentView.findViewById(R.id.sheet_warning);
        return warningTextView.getText();
    }
}
