// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bottom_sheet;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.RelativeLayout;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.chrome.browser.password_manager.PasswordManagerResourceProviderFactory;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;

/** This class is responsible for rendering the simple notice sheet. */
class SimpleNoticeSheetView implements BottomSheetContent {
    private final RelativeLayout mContentView;

    SimpleNoticeSheetView(Context context) {
        mContentView =
                (RelativeLayout)
                        LayoutInflater.from(context).inflate(R.layout.simple_notice_sheet, null);
        ImageView sheetHeaderImage = mContentView.findViewById(R.id.sheet_header_image);
        sheetHeaderImage.setImageDrawable(
                AppCompatResources.getDrawable(
                        context,
                        PasswordManagerResourceProviderFactory.create().getPasswordManagerIcon()));
    }

    void setTitle(String title) {
        TextView titleView = mContentView.findViewById(R.id.sheet_title);
        titleView.setText(title);
    }

    void setText(String text) {
        TextView textView = mContentView.findViewById(R.id.sheet_text);
        textView.setText(text);
    }

    void setButtonText(String text) {
        Button button = mContentView.findViewById(R.id.confirmation_button);
        button.setText(text);
    }

    @Nullable
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
        return 0;
    }

    @Override
    public void destroy() {}

    @Override
    public int getPriority() {
        return BottomSheetContent.ContentPriority.HIGH;
    }

    @Override
    public boolean swipeToDismissEnabled() {
        return false;
    }

    @Override
    public int getSheetContentDescriptionStringId() {
        // TODO(crbug.com/353283409): Introduce and use proper string.
        return android.R.string.ok;
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        // TODO(crbug.com/353283409): Introduce and use proper string.
        return android.R.string.ok;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        // TODO(crbug.com/353283409): Introduce and use proper string.
        return android.R.string.ok;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        // TODO(crbug.com/353283409): Introduce and use proper string.
        return android.R.string.ok;
    }

    @Override
    public float getHalfHeightRatio() {
        return HeightMode.DISABLED;
    }

    @Override
    public int getPeekHeight() {
        return HeightMode.DISABLED;
    }
}
