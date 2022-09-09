// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.view.Window;

import androidx.annotation.NonNull;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.incognito.IncognitoSnapshotController;

/**
 * This is the controller that prevents incognito tabs from being visible in Android Recents
 * for {@link CustomTabActivity}.
 */
public class IncognitoCustomTabSnapshotController extends IncognitoSnapshotController {
    /**
     * @param window The {@link Window} on which the snapshot capability needs to be controlled.
     * @param isShowingIncognitoSupplier {@link Supplier<Boolean>} which indicates whether we are
     *         showing Incognito or not currently.
     */
    IncognitoCustomTabSnapshotController(
            @NonNull Window window, @NonNull Supplier<Boolean> isShowingIncognitoSupplier) {
        super(window, isShowingIncognitoSupplier);
        updateIncognitoTabSnapshotState();
    }
}