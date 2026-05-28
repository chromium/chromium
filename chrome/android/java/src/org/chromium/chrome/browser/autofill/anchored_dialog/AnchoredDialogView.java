// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.anchored_dialog;

import static org.chromium.build.NullUtil.assertNonNull;

import android.content.Context;
import android.content.res.Resources;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.PopupWindow;
import android.widget.PopupWindow.OnDismissListener;

import org.chromium.build.annotations.MonotonicNonNull;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;

/**
 * Dialog UI which can be used to show BottomSheetContents in place of a bottomsheet. The sheet is
 * shown in a non-blocking way and is meant to be used on large form factor devices.
 */
@NullMarked
class AnchoredDialogView {
    private static class PopupHolder {
        private final Context mContext;
        private @MonotonicNonNull PopupWindow mPopup;

        PopupHolder(Context context) {
            mContext = context;
        }

        PopupWindow getOrCreate() {
            if (mPopup == null) {
                mPopup = new PopupWindow(mContext);

                Resources resources = mContext.getResources();

                mPopup.setFocusable(true);
                mPopup.setTouchModal(false);
                mPopup.setElevation(
                        resources.getDimensionPixelSize(R.dimen.anchored_dialog_elevation));
                mPopup.setInputMethodMode(PopupWindow.INPUT_METHOD_NOT_NEEDED);
                mPopup.setWindowLayoutType(WindowManager.LayoutParams.TYPE_APPLICATION_SUB_PANEL);
                mPopup.setBackgroundDrawable(
                        mContext.getDrawable(R.drawable.default_popup_menu_bg));
                mPopup.setAnimationStyle(R.style.PopupWindowAnimFade);
                mPopup.setOutsideTouchable(true);
                mPopup.setHeight(ViewGroup.LayoutParams.WRAP_CONTENT);
                mPopup.setWidth(resources.getDimensionPixelSize(R.dimen.anchored_dialog_width));
            }
            return mPopup;
        }

        @Nullable PopupWindow getIfExisting() {
            return mPopup;
        }
    }

    private final Context mContext;
    private final PopupHolder mPopupHolder;
    private @Nullable BottomSheetContent mContent;
    private @MonotonicNonNull View mContainerView;
    private int mOffsetY;

    AnchoredDialogView(Context context) {
        mContext = context;
        mPopupHolder = new PopupHolder(mContext);
    }

    @Nullable BottomSheetContent getContent() {
        return mContent;
    }

    void setContent(@Nullable BottomSheetContent content) {
        mContent = content;

        View root = mPopupHolder.getOrCreate().getContentView();
        if (root == null) {
            root =
                    LayoutInflater.from(mContext)
                            .inflate(R.layout.autofill_anchored_dialog_view, null);
            View closeButton = root.findViewById(R.id.anchored_dialog_close_button);
            closeButton.setOnClickListener(v -> hide());
            mPopupHolder.getOrCreate().setContentView(root);
        }
        ViewGroup contentContainer = root.findViewById(R.id.anchored_dialog_content_container);
        assertNonNull(contentContainer);
        contentContainer.removeAllViews();
        @Nullable View contentView = mContent == null ? null : mContent.getContentView();
        if (contentView != null) {
            contentContainer.addView(contentView);
        }
    }

    void setContainerView(View view) {
        mContainerView = view;
    }

    void setOnDismissListener(OnDismissListener listener) {
        mPopupHolder.getOrCreate().setOnDismissListener(listener);
    }

    void setOffsetY(int offset) {
        mOffsetY = offset;
    }

    void show() {
        assertNonNull(mContainerView);

        int margin = mContext.getResources().getDimensionPixelSize(R.dimen.anchored_dialog_margin);
        mPopupHolder
                .getOrCreate()
                .showAtLocation(
                        mContainerView, Gravity.TOP | Gravity.END, margin, mOffsetY + margin);
    }

    void hide() {
        PopupWindow popup = mPopupHolder.getIfExisting();
        if (popup != null) {
            popup.dismiss();
        }
    }
}
