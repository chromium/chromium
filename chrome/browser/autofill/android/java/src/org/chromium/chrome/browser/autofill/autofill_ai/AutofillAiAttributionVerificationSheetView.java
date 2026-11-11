// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.autofill_ai;

import android.content.Context;
import android.content.res.Resources;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ScrollView;

import androidx.annotation.StringRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.autofill.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetType;

/**
 * View and {@link BottomSheetContent} implementation for the Autofill AI attribution verification
 * bottom sheet.
 */
@NullMarked
/*package*/ class AutofillAiAttributionVerificationSheetView implements BottomSheetContent {
    private static final BottomSheetType SHEET_TYPE =
            new BottomSheetType.Builder().setUserInitiated(true).setModal(false).build();

    private final ScrollView mScrollView;
    private @Nullable Runnable mDismissHandler;

    AutofillAiAttributionVerificationSheetView(Context context) {
        mScrollView =
                (ScrollView)
                        LayoutInflater.from(context)
                                .inflate(
                                        R.layout.autofill_ai_attribution_verification_sheet,
                                        /* root= */ null);
    }

    void setDismissHandler(Runnable dismissHandler) {
        mDismissHandler = dismissHandler;
    }

    @Override
    public View getContentView() {
        return mScrollView;
    }

    @Override
    public @Nullable View getToolbarView() {
        return null;
    }

    @Override
    public int getVerticalScrollOffset() {
        return mScrollView.getScrollY();
    }

    @Override
    public void destroy() {
        // BottomSheetController invokes destroy() only when this content is permanently dismissed
        // (or dropped from the queue), not when temporarily suppressed (e.g. omnibox focus) or
        // re-queued behind a higher-priority sheet.
        if (mDismissHandler != null) {
            mDismissHandler.run();
        }
    }

    @Override
    public boolean hasCustomLifecycle() {
        // TODO(crbug.com/578938007): Figure out if users should be able to interact with the page
        // behind the bottom sheet.
        return false;
    }

    @Override
    public boolean hasCustomScrimLifecycle() {
        // Prevents a blocking scrim over the underlying email tab while BottomSheetTypes is off.
        return true;
    }

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
        return false;
    }

    @Override
    public boolean showHandlebar() {
        return true;
    }

    @Override
    public int getPeekHeight() {
        return mScrollView
                .getResources()
                .getDimensionPixelSize(
                        R.dimen.autofill_ai_attribution_verification_sheet_peek_height);
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
        assert false : "Half height is disabled.";
        return Resources.ID_NULL;
    }

    @Override
    public @StringRes int getSheetFullHeightAccessibilityStringId() {
        return R.string.autofill_ai_attribution_verification_sheet_full_height;
    }

    @Override
    public @StringRes int getSheetClosedAccessibilityStringId() {
        return R.string.autofill_ai_attribution_verification_sheet_closed;
    }

    @Override
    public @StringRes int getSheetHiddenAccessibilityStringId() {
        // Must differ from the closed/peek string so TalkBack does not drop PEEK -> HIDDEN speech.
        return R.string.autofill_ai_attribution_verification_sheet_hidden;
    }
}
