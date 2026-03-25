// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.save_card;

import static org.chromium.build.NullUtil.assertNonNull;

import android.content.Context;
import android.content.res.Resources;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.PopupWindow;
import android.widget.PopupWindow.OnDismissListener;

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
    private final Context mContext;
    private final PopupWindow mPopup;
    private @Nullable BottomSheetContent mContent;
    private @Nullable View mContainerView;
    private int mOffsetY;

    AnchoredDialogView(Context context) {
        mContext = context;
        mPopup = new PopupWindow(mContext);

        Resources resources = context.getResources();

        mPopup.setFocusable(true);
        mPopup.setTouchModal(false);
        mPopup.setElevation(resources.getDimensionPixelSize(R.dimen.anchored_dialog_elevation));
        mPopup.setInputMethodMode(PopupWindow.INPUT_METHOD_NOT_NEEDED);
        mPopup.setWindowLayoutType(WindowManager.LayoutParams.TYPE_APPLICATION_SUB_PANEL);
        mPopup.setBackgroundDrawable(mContext.getDrawable(R.drawable.default_popup_menu_bg));
        mPopup.setAnimationStyle(R.style.PopupWindowAnimFade);
        mPopup.setOutsideTouchable(true);
        mPopup.setHeight(ViewGroup.LayoutParams.WRAP_CONTENT);
        mPopup.setWidth(resources.getDimensionPixelSize(R.dimen.anchored_dialog_width));
    }

    @Nullable BottomSheetContent getContent() {
        return mContent;
    }

    void setContent(@Nullable BottomSheetContent content) {
        mContent = content;
        mPopup.setContentView(mContent == null ? null : mContent.getContentView());
    }

    void setContainerView(View view) {
        mContainerView = view;
    }

    void setOnDismissListener(OnDismissListener listener) {
        mPopup.setOnDismissListener(listener);
    }

    void setOffsetY(int offset) {
        mOffsetY = offset;
    }

    void show() {
        assertNonNull(mContainerView);
        int margin = mContext.getResources().getDimensionPixelSize(R.dimen.anchored_dialog_margin);
        mPopup.showAtLocation(mContainerView, Gravity.TOP | Gravity.END, margin, mOffsetY + margin);
    }

    void hide() {
        mPopup.dismiss();
    }
}
