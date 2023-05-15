// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pwd_migration;

import android.content.Context;

import androidx.annotation.Nullable;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** The coordinator of the password migration warning. */
public class PasswordMigrationWarningCoordinator {
    private final PasswordMigrationWarningMediator mMediator =
            new PasswordMigrationWarningMediator();

    public PasswordMigrationWarningCoordinator(
            @Nullable Context context, BottomSheetController sheetController) {
        mMediator.initialize(
                PasswordMigrationWarningProperties.createDefaultModel(mMediator::onDismissed));
        setUpModelChangeProcessors(
                mMediator.getModel(), new PasswordMigrationWarningView(context, sheetController));
    }

    public void showWarning() {
        mMediator.showWarning();
    }

    static void setUpModelChangeProcessors(PropertyModel model, PasswordMigrationWarningView view) {
        PropertyModelChangeProcessor.create(
                model, view, PasswordMigrationWarningViewBinder::bindPasswordMigrationWarningView);
    }
}
