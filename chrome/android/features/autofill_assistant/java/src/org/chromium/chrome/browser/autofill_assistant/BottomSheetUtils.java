// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetContent;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;
import org.chromium.chrome.browser.widget.bottomsheet.EmptyBottomSheetObserver;

class BottomSheetUtils {
    /** Request {@code controller} to show {@code content} and expand the sheet when it is shown. */
    static void showContentAndExpand(BottomSheetController controller,
            AssistantBottomSheetContent content, boolean animate) {
        // Show the content.
        if (controller.requestShowContent(content, animate)) {
            controller.expandSheet();
        } else {
            // If the content is not directly shown, add an observer that will expand the sheet when
            // it is.
            controller.addObserver(new EmptyBottomSheetObserver() {
                @Override
                public void onSheetContentChanged(BottomSheetContent newContent) {
                    if (newContent == content) {
                        controller.removeObserver(this);
                        controller.expandSheet();
                    }
                }
            });
        }
    }

    private BottomSheetUtils() {}
}
