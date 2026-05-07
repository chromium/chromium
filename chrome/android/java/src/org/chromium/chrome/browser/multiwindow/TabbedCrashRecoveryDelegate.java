// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.ui.modaldialog.ModalDialogManager;

/**
 * Delegate to help recover ChromeTabbedActivity windows from a previous session during app launch
 * after a crash.
 */
@NullMarked
public class TabbedCrashRecoveryDelegate {
    private static @Nullable TabbedCrashRecoveryDelegate sInstance;

    private TabbedCrashRecoveryDelegate() {}

    /* package */ static TabbedCrashRecoveryDelegate getInstance() {
        if (sInstance == null) {
            sInstance = new TabbedCrashRecoveryDelegate();
        }
        return sInstance;
    }

    /**
     * Shows a crash recovery prompt if applicable, when the {@link ModalDialogManager} for the host
     * activity is available.
     *
     * @param modalDialogManagerSupplier Supplier for ModalDialogManager.
     * @param hostActivity The host activity where the prompt will be displayed.
     */
    /* package */ void initiateCrashRecovery(
            MonotonicObservableSupplier<ModalDialogManager> modalDialogManagerSupplier,
            ChromeTabbedActivity hostActivity) {
        // TODO: Implement this method.
    }

    /**
     * Registers successful recovery of a window after a crash.
     *
     * @param activity The activity that was created when a window was successfully recovered after
     *     a crash.
     */
    /* package */ void registerRecovery(ChromeTabbedActivity activity) {
        // TODO: Implement this method.
    }
}
