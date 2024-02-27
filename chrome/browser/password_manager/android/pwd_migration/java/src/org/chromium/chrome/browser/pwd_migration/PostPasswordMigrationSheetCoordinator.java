// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pwd_migration;

import android.content.Context;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** The coordinator of the post password migration sheet. */
public class PostPasswordMigrationSheetCoordinator {
    private final PostPasswordMigrationSheetMediator mMediator =
            new PostPasswordMigrationSheetMediator();

    public PostPasswordMigrationSheetCoordinator(
            Context context, BottomSheetController sheetController, Profile profile) {
        mMediator.initialize(
                profile,
                PostPasswordMigrationSheetProperties.createDefaultModel(mMediator::onDismissed));
        setUpModelChangeProcessors(
                mMediator.getModel(), new PostPasswordMigrationSheetView(context, sheetController));
    }

    public void showSheet() {
        mMediator.showSheet();
    }

    PropertyModel getModelForTesting() {
        return mMediator.getModel();
    }

    static void setUpModelChangeProcessors(
            PropertyModel model, PostPasswordMigrationSheetView view) {
        PropertyModelChangeProcessor.create(
                model,
                view,
                PostPasswordMigrationSheetViewBinder::bindPostPasswordMigrationSheetView);
    }
}
