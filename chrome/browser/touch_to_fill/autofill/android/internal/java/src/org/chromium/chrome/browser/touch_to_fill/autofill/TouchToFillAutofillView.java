// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.autofill;

import android.content.Context;
import android.content.res.Resources;
import android.view.LayoutInflater;
import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.touch_to_fill.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;

/**
 * This class is responsible for rendering the bottom sheet which displays the TouchToFillAutofill
 * notice. It is a View in this MVC component and holds Android Views.
 */
@NullMarked
class TouchToFillAutofillView implements BottomSheetContent {
    private final BottomSheetController mBottomSheetController;
    private final View mContentView;
    private @Nullable Runnable mDismissHandler;
    private boolean mIsShowing;

    private final BottomSheetObserver mBottomSheetObserver =
            new BottomSheetObserver() {
                @Override
                public void onSheetClosed(@BottomSheetController.StateChangeReason int reason) {
                    if (mBottomSheetController.getCurrentSheetContent()
                            != TouchToFillAutofillView.this) {
                        return;
                    }
                    mIsShowing = false;
                    if (mDismissHandler != null) {
                        mDismissHandler.run();
                    }
                    mBottomSheetController.removeObserver(mBottomSheetObserver);
                }
            };

    TouchToFillAutofillView(Context context, BottomSheetController bottomSheetController) {
        mBottomSheetController = bottomSheetController;
        mContentView =
                LayoutInflater.from(context)
                        .inflate(
                                R.layout.touch_to_fill_autofill_personal_context_notice_screen,
                                null);
    }

    void setAcknowledgeHandler(Runnable acknowledgeHandler) {
        mContentView
                .findViewById(R.id.notice_acknowledge_button)
                .setOnClickListener(v -> acknowledgeHandler.run());
    }

    void setSettingsLinkHandler(Runnable settingsLinkHandler) {
        mContentView
                .findViewById(R.id.notice_manage_settings_link)
                .setOnClickListener(v -> settingsLinkHandler.run());
    }

    void setDismissHandler(Runnable dismissHandler) {
        mDismissHandler = dismissHandler;
    }

    boolean setVisible(boolean visible) {
        if (visible == mIsShowing) return true;
        if (visible) {
            mBottomSheetController.addObserver(mBottomSheetObserver);
            if (!mBottomSheetController.requestShowContent(this, true)) {
                mBottomSheetController.removeObserver(mBottomSheetObserver);
                return false;
            }
            mIsShowing = true;
        } else {
            mBottomSheetController.hideContent(this, true);
            mIsShowing = false;
        }
        return true;
    }

    // BottomSheetContent implementation:
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
        return 0;
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
    public boolean swipeToDismissEnabled() {
        return true;
    }

    @Override
    public float getFullHeightRatio() {
        return HeightMode.WRAP_CONTENT;
    }

    @Override
    public float getHalfHeightRatio() {
        return HeightMode.DISABLED;
    }

    @Override
    public boolean hideOnScroll() {
        return true;
    }

    @Override
    public String getSheetContentDescription(Context context) {
        return context.getString(
                R.string.autofill_personal_context_notice_sheet_content_description);
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        return R.string.autofill_personal_context_notice_sheet_full_height;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        return R.string.autofill_personal_context_notice_sheet_closed;
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        assert false;
        return Resources.ID_NULL;
    }

    @Override
    public boolean hasCustomLifecycle() {
        return false;
    }
}
