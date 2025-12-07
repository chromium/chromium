// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;

import androidx.annotation.StringRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.ui.widget.ButtonCompat;

/**
 * A view for the Shared Tab Group Notice Bottom Sheet. Inform the user that changes made to a
 * shared tab group will be visible to everyone in the group.
 */
@NullMarked
public class TabGroupShareNoticeBottomSheetView extends LinearLayout implements BottomSheetContent {
    private final ViewGroup mContentView;
    private final ButtonCompat mConfirmButton;
    private @Nullable Runnable mCompletionHandler;

    TabGroupShareNoticeBottomSheetView(Context context) {
        super(context);
        mContentView =
                (ViewGroup)
                        LayoutInflater.from(context)
                                .inflate(
                                        R.layout.tab_group_share_notice_bottom_sheet,
                                        /* root= */ null);
        mConfirmButton =
                mContentView.findViewById(R.id.tab_group_share_notice_bottom_sheet_confirm_button);

        initConfirmButton();
    }

    private void initConfirmButton() {
        mConfirmButton.setOnClickListener(
                ignored -> {
                    if (mCompletionHandler != null) {
                        mCompletionHandler.run();
                    }
                });
    }

    // BottomSheetContent implementation follows:
    @Override
    public View getContentView() {
        return mContentView;
    }

    @Override
    public @Nullable View getToolbarView() {
        return null;
    }

    @Override
    public int getVerticalScrollOffset() {
        return mContentView.getScrollY();
    }

    @Override
    public void destroy() {}

    @Override
    public boolean swipeToDismissEnabled() {
        return true;
    }

    @Override
    public int getPriority() {
        return ContentPriority.LOW;
    }

    @Override
    public float getFullHeightRatio() {
        return HeightMode.WRAP_CONTENT;
    }

    @Override
    public boolean hideOnScroll() {
        return true;
    }

    @Override
    public String getSheetContentDescription(Context context) {
        return context.getString(R.string.tab_group_share_notice_bottom_sheet_content_description);
    }

    @Override
    public @StringRes int getSheetHalfHeightAccessibilityStringId() {
        return R.string.tab_group_share_notice_bottom_sheet_full_height;
    }

    @Override
    public @StringRes int getSheetFullHeightAccessibilityStringId() {
        return R.string.tab_group_share_notice_bottom_sheet_half_height;
    }

    @Override
    public @StringRes int getSheetClosedAccessibilityStringId() {
        return R.string.tab_group_share_notice_bottom_sheet_closed;
    }

    /* package */ void setCompletionHandler(Runnable completionHandler) {
        mCompletionHandler = completionHandler;
    }
}
