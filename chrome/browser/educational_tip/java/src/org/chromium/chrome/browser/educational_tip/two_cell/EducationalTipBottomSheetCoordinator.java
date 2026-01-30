// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip.two_cell;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.educational_tip.EducationTipModuleActionDelegate;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

/**
 * Coordinator for the bottom sheet container in the two-cell educational tip. It is responsible for
 * fetching and preparing the content for the bottom sheet.
 */
@NullMarked
public class EducationalTipBottomSheetCoordinator {
    BottomSheetController mBottomSheetController;

    /**
     * @param actionDelegate The instance of {@link EducationTipModuleActionDelegate}.
     */
    public EducationalTipBottomSheetCoordinator(EducationTipModuleActionDelegate actionDelegate) {
        mBottomSheetController = actionDelegate.getBottomSheetController();
    }

    public void showBottomSheet() {
        // TODO(crbug.com/479597724): Implement method and relevant tests.
    }
}
