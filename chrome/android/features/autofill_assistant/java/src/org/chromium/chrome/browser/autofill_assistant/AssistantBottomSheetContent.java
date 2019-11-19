// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ScrollView;

import androidx.annotation.Nullable;

import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetContent;

/**
 * The {@link BottomSheetContent} for the Autofill Assistant. It supports notifying the
 * BottomSheet when its size changes and allows to dynamically set its scrollable content (in
 * practice, this allows to replace the onboarding by the actual Autofill Assistant content).
 */
class AssistantBottomSheetContent implements BottomSheetContent {
    private final View mToolbarView;
    private final SizeListenableLinearLayout mContentView;
    @Nullable
    private ScrollView mContentScrollableView;

    public AssistantBottomSheetContent(Context context) {
        mToolbarView = LayoutInflater.from(context).inflate(
                R.layout.autofill_assistant_bottom_sheet_toolbar, /* root= */ null);
        mContentView = new SizeListenableLinearLayout(context);
        mContentView.setLayoutParams(new ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
    }

    public void setContent(View content, ScrollView scrollableView) {
        if (mContentView.getChildCount() > 0) {
            mContentView.removeAllViews();
        }

        content.setLayoutParams(new ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        mContentView.addView(content);
        mContentScrollableView = scrollableView;
    }

    @Override
    public View getContentView() {
        return mContentView;
    }

    @Nullable
    @Override
    public View getToolbarView() {
        return mToolbarView;
    }

    @Override
    public int getVerticalScrollOffset() {
        if (mContentScrollableView != null) {
            return mContentScrollableView.getScrollY();
        }

        return 0;
    }

    @Override
    public boolean setContentSizeListener(@Nullable ContentSizeListener listener) {
        mContentView.setContentSizeListener(listener);
        return true;
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
    public float getFullHeightRatio() {
        return BottomSheetContent.HeightMode.WRAP_CONTENT;
    }

    @Override
    public boolean hasCustomLifecycle() {
        return true;
    }

    @Override
    public boolean hasCustomScrimLifecycle() {
        return true;
    }

    @Override
    public boolean hideOnScroll() {
        return false;
    }

    @Override
    public int getSheetContentDescriptionStringId() {
        return R.string.autofill_assistant_sheet_content_description;
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        return R.string.autofill_assistant_sheet_half_height;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        return R.string.autofill_assistant_sheet_full_height;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        return R.string.autofill_assistant_sheet_closed;
    }
}
