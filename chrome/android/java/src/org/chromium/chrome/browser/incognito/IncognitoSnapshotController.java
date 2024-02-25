// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito;

import android.view.Window;
import android.view.WindowManager;

import androidx.annotation.NonNull;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/**
 * An abstract base class to provide common functionalities related to allowing/blocking snapshot
 * for Incognito tabs across {@link ChromeTabbedActivity} and {@link CustomTabActivity}.
 */
public abstract class IncognitoSnapshotController {
    private final @NonNull Window mWindow;
    private final @NonNull Supplier<Boolean> mIsShowingIncognitoSupplier;

    /**
     * @param window The {@link Window} on which the snapshot capability needs to be controlled.
     * @param isShowingIncognitoSupplier {@link Supplier<Boolean>} which indicates whether we are
     *     showing Incognito or not currently.
     */
    protected IncognitoSnapshotController(
            @NonNull Window window, @NonNull Supplier<Boolean> isShowingIncognitoSupplier) {
        mWindow = window;
        mIsShowingIncognitoSupplier = isShowingIncognitoSupplier;
    }

    /** Sets the attributes flags to secure if there is an incognito tab visible. */
    protected void updateIncognitoTabSnapshotState() {
        assert mIsShowingIncognitoSupplier != null : "Supplier not found!";

        WindowManager.LayoutParams attributes = mWindow.getAttributes();
        boolean currentSecureState =
                (attributes.flags & WindowManager.LayoutParams.FLAG_SECURE)
                        == WindowManager.LayoutParams.FLAG_SECURE;

        boolean expectedSecureState = mIsShowingIncognitoSupplier.get();
        if (ChromeFeatureList.sIncognitoScreenshot.isEnabled()) {
            expectedSecureState = false;
        }
        if (currentSecureState == expectedSecureState) return;

        if (expectedSecureState) {
            mWindow.addFlags(WindowManager.LayoutParams.FLAG_SECURE);
        } else {
            mWindow.clearFlags(WindowManager.LayoutParams.FLAG_SECURE);
        }
    }
}
