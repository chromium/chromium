// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.password_generation;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageView;

import androidx.annotation.Nullable;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.chrome.browser.password_manager.PasswordManagerResourceProviderFactory;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;

/**
 * This class is responsible for rendering the password generation bottom sheet. It is a View in
 * this Model-View-Controller component and doesn't inherit but holds Android Views.
 */
class TouchToFillPasswordGenerationView implements BottomSheetContent {
    private final View mContent;

    TouchToFillPasswordGenerationView(Context context) {
        mContent = LayoutInflater.from(context).inflate(
                R.layout.touch_to_fill_password_generation, null);
        ImageView sheetHeaderImage = mContent.findViewById(R.id.touch_to_fill_sheet_header_image);
        sheetHeaderImage.setImageDrawable(AppCompatResources.getDrawable(
                context, PasswordManagerResourceProviderFactory.create().getPasswordManagerIcon()));
    }

    @Override
    public View getContentView() {
        return mContent;
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
        return R.string.password_generation_bottom_sheet_content_description;
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        // Half-height is disabled so no need for an accessibility string.
        assert false : "This method should not be called";
        return 0;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        return R.string.password_generation_bottom_sheet_content_description;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        return R.string.password_generation_bottom_sheet_closed;
    }

    @Override
    public float getHalfHeightRatio() {
        return HeightMode.DISABLED;
    }

    @Override
    public float getFullHeightRatio() {
        return HeightMode.WRAP_CONTENT;
    }

    @Override
    public int getPeekHeight() {
        return HeightMode.DISABLED;
    }
}
