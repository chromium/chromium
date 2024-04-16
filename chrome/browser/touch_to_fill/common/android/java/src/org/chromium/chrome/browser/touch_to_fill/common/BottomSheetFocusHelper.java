// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.common;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorSupplier;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsAccessibility;
import org.chromium.ui.base.WindowAndroid;

/**
 * This is a helper class used to restore focus to the previously focused element after the
 * {@link BottomSheet} closes. This is an observer that can be registered for a one time use since
 * it deregisters itself once the sheet is closed.
 */
public class BottomSheetFocusHelper extends EmptyBottomSheetObserver {
    private WindowAndroid mWindowAndroid;
    private BottomSheetController mBottomSheetController;
    private WebContentsAccessibility mWebContentsAccessibility;

    public BottomSheetFocusHelper(
            BottomSheetController bottomSheetController, WindowAndroid windowAndroid) {
        mBottomSheetController = bottomSheetController;
        mWindowAndroid = windowAndroid;
    }

    /**
     * Adds the observer which will remove itself after it observed the closing of the
     * {@link BottomSheet}.
     */
    public void registerForOneTimeUse() {
        mBottomSheetController.addObserver(this);
    }

    /** Sets the {@link WebContentsAccessibility} to be used in tests. */
    @VisibleForTesting
    public void setWebContentsAccessibility(WebContentsAccessibility webContentsAccessibility) {
        mWebContentsAccessibility = webContentsAccessibility;
    }

    @Override
    public void onSheetClosed(@StateChangeReason int reason) {
        Tab currentTab = TabModelSelectorSupplier.getCurrentTabFrom(mWindowAndroid);
        WebContents webContents = currentTab != null ? currentTab.getWebContents() : null;
        if (webContents != null) {
            WebContentsAccessibility webContentsAccessibility;
            if (mWebContentsAccessibility != null) {
                webContentsAccessibility = mWebContentsAccessibility;
            } else {
                webContentsAccessibility = WebContentsAccessibility.fromWebContents(webContents);
            }
            if (webContentsAccessibility != null) {
                webContentsAccessibility.restoreFocus();
            }
        }
        // TODO(crbug.com/40257910): Move the adding and removing of the observer out of the helper.
        mBottomSheetController.removeObserver(this);
    }
}
