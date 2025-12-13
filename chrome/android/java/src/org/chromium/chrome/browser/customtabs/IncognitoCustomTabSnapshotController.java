// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.app.Activity;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.incognito.IncognitoSnapshotController;

import java.util.function.Supplier;

/**
 * This is the controller that prevents incognito tabs from being visible in Android Recents for
 * {@link CustomTabActivity}.
 */
@NullMarked
public class IncognitoCustomTabSnapshotController extends IncognitoSnapshotController {
    /**
     * @param activity The {@link Activity} on which the snapshot capability needs to be controlled.
     * @param isShowingIncognitoSupplier {@link Supplier<Boolean>} which indicates whether we are
     *     showing Incognito or not currently.
     */
    IncognitoCustomTabSnapshotController(
            Activity activity, Supplier<Boolean> isShowingIncognitoSupplier) {
        super(activity, isShowingIncognitoSupplier);
        updateIncognitoTabSnapshotState();
    }
}
