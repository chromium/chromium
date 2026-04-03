// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.anchored_dialog;

import org.chromium.base.UnownedUserDataKey;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.base.WindowAndroid;

/** Provides an AnchoredDialogCoordinator object for each {@link WindowAndroid}. */
@NullMarked
public class AnchoredDialogCoordinatorProvider {
    /** The key used to bind the controller to the unowned data host. */
    private static final UnownedUserDataKey<AnchoredDialogCoordinator> KEY =
            new UnownedUserDataKey<>();

    /**
     * Get the shared {@link AnchoredDialogCoordinator} from the provided {@link WindowAndroid}.
     *
     * @param windowAndroid The window to pull the controller from.
     * @return A shared instance of a {@link AnchoredDialogCoordinator}.
     */
    public static @Nullable AnchoredDialogCoordinator from(WindowAndroid windowAndroid) {
        return KEY.retrieveDataFromHost(windowAndroid.getUnownedUserDataHost());
    }

    public static void attach(WindowAndroid windowAndroid, AnchoredDialogCoordinator coordinator) {
        KEY.attachToHost(windowAndroid.getUnownedUserDataHost(), coordinator);
    }

    public static void detach(AnchoredDialogCoordinator coordinator) {
        KEY.detachFromAllHosts(coordinator);
    }

    // Do not instantiate this class.
    private AnchoredDialogCoordinatorProvider() {}
}
