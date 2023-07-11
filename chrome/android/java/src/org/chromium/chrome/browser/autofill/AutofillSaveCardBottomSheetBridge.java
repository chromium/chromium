// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.content.Context;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.ui.base.WindowAndroid;

/**
 * Bridge class providing an entry point for autofill client to trigger the save card bottom sheet.
 */
@JNINamespace("autofill")
public class AutofillSaveCardBottomSheetBridge {
    private WindowAndroid mWindow;
    private BottomSheetController mBottomSheetController;

    @CalledByNative
    @VisibleForTesting
    /* package */ AutofillSaveCardBottomSheetBridge(WindowAndroid window) {
        mWindow = window;
        mBottomSheetController = BottomSheetControllerProvider.from(window);
    }

    /**
     * Requests to show the bottom sheet.
     *
     * The bottom sheet may not be shown in some cases.
     * {@see BottomSheetController#requestShowContent}
     *
     * @return True if shown.
     */
    @CalledByNative
    public boolean requestShowContent() {
        return mBottomSheetController.requestShowContent(
                new BottomSheetContentImpl(mWindow.getApplicationContext()), /* animate= */ true);
    }

    // TODO(crbug.com/1454271): Implement save card bottom sheet.
    @VisibleForTesting
    static final class BottomSheetContentImpl implements BottomSheetContent {
        private View mView;

        private BottomSheetContentImpl(Context context) {
            mView = new View(context);
        }

        @Override
        public View getContentView() {
            return mView;
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
            return ContentPriority.HIGH;
        }

        @Override
        public boolean swipeToDismissEnabled() {
            return false;
        }

        @Override
        public int getSheetContentDescriptionStringId() {
            return android.R.string.ok;
        }

        @Override
        public int getSheetHalfHeightAccessibilityStringId() {
            return android.R.string.ok;
        }

        @Override
        public int getSheetFullHeightAccessibilityStringId() {
            return android.R.string.ok;
        }

        @Override
        public int getSheetClosedAccessibilityStringId() {
            return android.R.string.ok;
        }
    }
}
