// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs;

import android.content.Context;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSession;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.List;
import java.util.function.Supplier;

/** Controller for accessing helper functions for the singleton factory instance. */
@NullMarked
public class RestoreTabsDialogControllerImpl implements RestoreTabsController {
    private RestoreTabsDialogCoordinator mRestoreTabsDialogCoordinator;

    public RestoreTabsDialogControllerImpl(
            Context context,
            Profile profile,
            TabCreatorManager tabCreatorManager,
            Supplier<ModalDialogManager> modalDialogManagerSupplier) {
        mRestoreTabsDialogCoordinator =
                new RestoreTabsDialogCoordinator(
                        context, profile, tabCreatorManager, modalDialogManagerSupplier);
    }

    @Override
    @SuppressWarnings("NullAway")
    public void destroy() {
        mRestoreTabsDialogCoordinator.destroy();
        mRestoreTabsDialogCoordinator = null;
    }

    @Override
    public void showHomeScreen(
            ForeignSessionHelper foreignSessionHelper,
            List<ForeignSession> sessions,
            RestoreTabsControllerDelegate delegate) {
        mRestoreTabsDialogCoordinator.showHomeScreen(foreignSessionHelper, sessions, delegate);
    }
}
