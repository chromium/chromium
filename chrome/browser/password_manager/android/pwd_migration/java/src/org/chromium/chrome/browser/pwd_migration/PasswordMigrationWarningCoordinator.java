// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pwd_migration;

import android.content.Context;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.pwd_migration.PasswordMigrationWarningProperties.ScreenType;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** The coordinator of the password migration warning. */
public class PasswordMigrationWarningCoordinator {
    private final PasswordMigrationWarningMediator mMediator =
            new PasswordMigrationWarningMediator();

    public PasswordMigrationWarningCoordinator(
            @Nullable Context context, BottomSheetController sheetController) {
        PropertyModel model = PasswordMigrationWarningProperties.createDefaultModel(
                mMediator::onDismissed, mMediator);
        mMediator.initialize(model);
        setUpModelChangeProcessors(
                model, new PasswordMigrationWarningView(context, sheetController));
    }

    public void showWarning(Profile profile) {
        mMediator.showWarning(ScreenType.INTRO_SCREEN, profile);
    }

    static void setUpModelChangeProcessors(PropertyModel model, PasswordMigrationWarningView view) {
        PropertyModelChangeProcessor.create(
                model, view, PasswordMigrationWarningViewBinder::bindPasswordMigrationWarningView);
    }
}
