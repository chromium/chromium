// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bottom_sheet;

import androidx.annotation.VisibleForTesting;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyModel;

/** The coordinator of the simple notice sheet. */
public class SimpleNoticeSheetCoordinator {

    private PropertyModel mModel;
    private final SimpleNoticeSheetMediator mMediator = new SimpleNoticeSheetMediator();

    public SimpleNoticeSheetCoordinator(BottomSheetController sheetController) {
        mModel = SimpleNoticeSheetProperties.createDefaultModel(mMediator::onDismissed);
        mMediator.initialize(mModel);
    }

    public void showSheet() {
        mMediator.showSheet();
    }

    @VisibleForTesting
    PropertyModel getModel() {
        return mModel;
    }
}
