// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.autofill_ai;

import android.view.View;

import androidx.annotation.StringRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.autofill.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent.ContentPriority;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetType;

@NullMarked
class AutofillAiSourceAttributionContent implements BottomSheetContent {
    private static final BottomSheetType SHEET_TYPE =
            new BottomSheetType.Builder().setUserInitiated(true).setModal(true).build();

    private final AutofillAiSourceAttributionView mView;

    AutofillAiSourceAttributionContent(AutofillAiSourceAttributionView view) {
        mView = view;
    }

    @Override
    public View getContentView() {
        return mView.getContentView();
    }

    @Override
    public @Nullable View getToolbarView() {
        return null;
    }

    @Override
    public int getVerticalScrollOffset() {
        return mView.getVerticalScrollOffset();
    }

    @Override
    public void destroy() {}

    @Override
    public BottomSheetType getSheetType() {
        return SHEET_TYPE;
    }

    @Override
    public @ContentPriority int getPriority() {
        return ContentPriority.HIGH;
    }

    @Override
    public boolean swipeToDismissEnabled() {
        return true;
    }

    @Override
    public boolean showHandlebar() {
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
    public @StringRes int getSheetHalfHeightAccessibilityStringId() {
        assert false : "Half height is disabled";
        return R.string.autofill_ai_attribution_sheet_accessibility_title;
    }

    @Override
    public @StringRes int getSheetFullHeightAccessibilityStringId() {
        return R.string.autofill_ai_attribution_sheet_accessibility_title;
    }

    @Override
    public @StringRes int getSheetClosedAccessibilityStringId() {
        return R.string.autofill_ai_attribution_sheet_closed;
    }
}
