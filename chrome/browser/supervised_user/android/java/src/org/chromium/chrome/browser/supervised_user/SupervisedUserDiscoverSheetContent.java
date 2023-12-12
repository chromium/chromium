// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.supervised_user;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.Button;

import androidx.annotation.Nullable;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

/**
 * Bottom sheet content for the screen appears when clicking on the discover info card. The Bottom
 * sheet contains information about how content is chosen for the supervised user discover feed.
 */
public class SupervisedUserDiscoverSheetContent implements BottomSheetContent {

    protected final Activity mActivity;
    private final View mContentView;
    private final BottomSheetController mBottomSheetController;

    public SupervisedUserDiscoverSheetContent(
            Activity activity, BottomSheetController bottomSheetController) {
        mActivity = activity;
        mContentView =
                LayoutInflater.from(mActivity)
                        .inflate(R.layout.supervised_user_discover_bottom_sheet, null);
        mBottomSheetController = bottomSheetController;
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
        return 0;
    }

    @Override
    public float getFullHeightRatio() {
        return HeightMode.WRAP_CONTENT;
    }

    @Override
    public void destroy() {}

    @Override
    public int getPeekHeight() {
        return HeightMode.DISABLED;
    }

    @Override
    public int getPriority() {
        return ContentPriority.HIGH;
    }

    @Override
    public boolean swipeToDismissEnabled() {
        return true;
    }

    @Override
    public int getSheetContentDescriptionStringId() {
        return R.string.supervised_user_discover_bottom_sheet_description;
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        return R.string.supervised_user_discover_bottom_sheet_half_height;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        return R.string.supervised_user_discover_bottom_sheet_full_height;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        return R.string.supervised_user_discover_bottom_sheet_closed;
    }
}
