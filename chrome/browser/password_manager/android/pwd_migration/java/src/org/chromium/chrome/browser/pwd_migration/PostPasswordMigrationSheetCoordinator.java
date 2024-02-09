// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pwd_migration;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyModel;

/** The coordinator of the post password migration sheet. */
public class PostPasswordMigrationSheetCoordinator {
    private final PostPasswordMigrationSheetMediator mMediator =
            new PostPasswordMigrationSheetMediator();

    public PostPasswordMigrationSheetCoordinator(BottomSheetController sheetController) {
        mMediator.initialize(
                PostPasswordMigrationSheetProperties.createDefaultModel(mMediator::onDismissed));
    }

    public void showSheet() {
        mMediator.showSheet();
    }

    PropertyModel getModelForTesting() {
        return mMediator.getModel();
    }
}
