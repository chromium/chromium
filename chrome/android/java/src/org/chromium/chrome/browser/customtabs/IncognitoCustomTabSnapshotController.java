// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.app.Activity;

import androidx.annotation.NonNull;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.incognito.IncognitoSnapshotController;

/**
 * This is the controller that prevents incognito tabs from being visible in Android Recents for
 * {@link CustomTabActivity}.
 */
public class IncognitoCustomTabSnapshotController extends IncognitoSnapshotController {
    /**
     * @param activity The {@link Activity} on which the snapshot capability needs to be controlled.
     * @param isShowingIncognitoSupplier {@link Supplier<Boolean>} which indicates whether we are
     *     showing Incognito or not currently.
     */
    IncognitoCustomTabSnapshotController(
            @NonNull Activity activity, @NonNull Supplier<Boolean> isShowingIncognitoSupplier) {
        super(activity, isShowingIncognitoSupplier);
        updateIncognitoTabSnapshotState();
    }
}
